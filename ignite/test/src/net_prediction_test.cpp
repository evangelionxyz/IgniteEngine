// Copyright (c) 2026 Evangelion Manuhutu
// Google Test suite for Client-Side Prediction & Reconciliation:
// 1. History recording and sliding window bounds
// 2. Exact agreement reconciliation (no visual hitch or rewind)
// 3. Misprediction detection, state rewind & input replay
// 4. Smooth visual error offset decay
// 5. Sequence eviction of acknowledged frames

#include <gtest/gtest.h>

#include <ignite/net/net_prediction.hpp>

using namespace ignite;

TEST(NetPrediction, RecordAndRetrieveHistory)
{
    ClientPrediction pred(0.05f);
    EXPECT_EQ(pred.GetHistoryCount(), 0u);

    for (uint32_t seq = 1; seq <= 5; ++seq)
    {
        netproto::InputCmd cmd;
        cmd.seq = seq;
        cmd.move = glm::vec2(1.0f, 0.0f);
        pred.Record(seq, seq * 10, glm::vec3(float(seq), 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), cmd);
    }

    EXPECT_EQ(pred.GetHistoryCount(), 5u);
    EXPECT_EQ(pred.GetHistory().front().seq, 1u);
    EXPECT_EQ(pred.GetHistory().back().seq, 5u);
}

TEST(NetPrediction, ReconcileAgreementPreservesPrediction)
{
    ClientPrediction pred(0.05f); // 5cm threshold

    netproto::InputCmd cmd;
    cmd.seq = 10;
    pred.Record(10, 100, glm::vec3(10.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), cmd);

    glm::vec3 correctedPos;
    glm::quat correctedRot;

    // Server agrees within 2cm (error < 0.05m)
    bool corrected = pred.Reconcile(
        10, glm::vec3(10.02f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        1.0f / 60.0f, nullptr, correctedPos, correctedRot
    );

    EXPECT_FALSE(corrected);
    // Acknowledged frame is evicted
    EXPECT_EQ(pred.GetHistoryCount(), 0u);
    // Visual offset remains zero
    EXPECT_FLOAT_EQ(glm::length(pred.GetVisualOffset()), 0.0f);
}

TEST(NetPrediction, ReconcileMispredictionRewindsAndReplays)
{
    ClientPrediction pred(0.05f);

    // Client predicted:
    // Frame 1 (seq=1): pos (1, 0, 0)
    // Frame 2 (seq=2): pos (2, 0, 0)
    // Frame 3 (seq=3): pos (3, 0, 0)
    for (uint32_t seq = 1; seq <= 3; ++seq)
    {
        netproto::InputCmd cmd;
        cmd.seq = seq;
        cmd.move = glm::vec2(1.0f, 0.0f);
        pred.Record(seq, seq, glm::vec3(float(seq), 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), cmd);
    }

    auto simFn = [](const netproto::InputCmd &c, float dt, glm::vec3 &pos, glm::quat &rot) {
        pos.x += c.move.x * 60.0f * dt; // moves +1.0 per tick
    };

    glm::vec3 correctedPos;
    glm::quat correctedRot;

    // Server snapshot arrives stating seq=1 was actually at pos (0, 0, 0) (e.g. pushed back by obstacle)
    bool corrected = pred.Reconcile(
        1, glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        1.0f / 60.0f, simFn, correctedPos, correctedRot
    );

    EXPECT_TRUE(corrected);
    // Rewound to (0, 0, 0), then replayed frame 2 (+1.0) and frame 3 (+1.0) => pos should be (2, 0, 0)
    EXPECT_NEAR(correctedPos.x, 2.0f, 0.001f);
    EXPECT_NEAR(correctedPos.y, 0.0f, 0.001f);
    EXPECT_NEAR(correctedPos.z, 0.0f, 0.001f);

    // Frame 1 was evicted, leaving unacknowledged frames 2 and 3 in history
    EXPECT_EQ(pred.GetHistoryCount(), 2u);

    // Visual offset captures the prediction discrepancy: (1.0 - 0.0) = 1.0 on X
    EXPECT_NEAR(pred.GetVisualOffset().x, 1.0f, 0.001f);
}

TEST(NetPrediction, VisualOffsetSmoothDecay)
{
    ClientPrediction pred(0.05f);

    netproto::InputCmd cmd;
    cmd.seq = 1;
    pred.Record(1, 1, glm::vec3(5.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), cmd);

    glm::vec3 cPos;
    glm::quat cRot;
    pred.Reconcile(1, glm::vec3(4.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), 0.016f, nullptr, cPos, cRot);

    // Visual offset is initially 1.0
    EXPECT_NEAR(pred.GetVisualOffset().x, 1.0f, 0.01f);

    // Step visual decay across several frames
    for (int i = 0; i < 20; ++i)
    {
        pred.UpdateVisualOffset(0.016f, 15.0f);
    }

    // Offset should smoothly diminish towards 0
    EXPECT_LT(pred.GetVisualOffset().x, 0.1f);

    // After sufficient steps, offset reaches 0
    for (int i = 0; i < 20; ++i)
    {
        pred.UpdateVisualOffset(0.016f, 15.0f);
    }
    EXPECT_FLOAT_EQ(pred.GetVisualOffset().x, 0.0f);
}

TEST(NetPrediction, AcknowledgedFrameEviction)
{
    ClientPrediction pred(0.05f);

    for (uint32_t seq = 1; seq <= 10; ++seq)
    {
        netproto::InputCmd cmd;
        cmd.seq = seq;
        pred.Record(seq, seq, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), cmd);
    }
    EXPECT_EQ(pred.GetHistoryCount(), 10u);

    glm::vec3 cPos;
    glm::quat cRot;
    // Server acknowledges up to seq=7
    pred.Reconcile(7, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), 0.016f, nullptr, cPos, cRot);

    // Frames 1..7 evicted, remaining: 8, 9, 10 (3 frames)
    EXPECT_EQ(pred.GetHistoryCount(), 3u);
    EXPECT_EQ(pred.GetHistory().front().seq, 8u);
    EXPECT_EQ(pred.GetHistory().back().seq, 10u);
}

