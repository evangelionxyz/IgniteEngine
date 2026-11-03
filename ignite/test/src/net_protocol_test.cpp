// Copyright (c) 2026 Evangelion Manuhutu
// Comprehensive Google Test suite for Ignite Network Protocol:
// 1. Fixed-point Position & Smallest-Three Quaternion quantization precision
// 2. Wire message serialization roundtrips (Hello, Welcome, Reject, Spawn, Despawn, Input, Snapshot, Chat, Ping/Pong)
// 3. Hostile / malformed packet fuzzing and buffer overrun defense

#include <gtest/gtest.h>

#include <ignite/net/net_protocol.hpp>
#include <net/BufferStream.hpp>

#include <vector>
#include <cmath>
#include <algorithm>
#include <string>

using namespace ignite;

namespace
{
    float QuatAngleDeg(const glm::quat &a, const glm::quat &b)
    {
        float d = std::min(1.0f, std::abs(glm::dot(a, b)));
        return glm::degrees(2.0f * std::acos(d));
    }
}

TEST(NetProtocol, QuaternionQuantizationAccuracy)
{
    float worstErrorDeg = 0.0f;
    for (int i = 0; i < 2000; ++i)
    {
        glm::vec3 axis = glm::normalize(glm::vec3(
            std::sin(i * 1.3f) + 0.1f,
            std::cos(i * 0.7f),
            std::sin(i * 2.1f) - 0.2f
        ));
        glm::quat q = glm::angleAxis(i * 0.173f, axis);
        glm::quat r = netproto::DequantizeQuat(netproto::QuantizeQuat(q));

        float err = QuatAngleDeg(q, r);
        worstErrorDeg = std::max(worstErrorDeg, err);
    }

    EXPECT_LT(worstErrorDeg, 0.5f);
}

TEST(NetProtocol, PositionQuantizationClampingAndResolution)
{
    // Test fine resolution within range
    const std::vector<float> testValues = {
        -16384.0f, -1000.0f, -123.456f, -0.25f, 0.0f, 0.125f, 3.14159f, 999.999f, 16384.0f
    };

    for (float v : testValues)
    {
        int32_t q = netproto::QuantizePos(v);
        float deq = netproto::DequantizePos(q);
        EXPECT_NEAR(v, deq, 0.001f);
    }

    // Test clamping for values exceeding POS_RANGE
    float extremeHigh = netproto::DequantizePos(netproto::QuantizePos(1e9f));
    EXPECT_NEAR(extremeHigh, netproto::POS_RANGE, 0.01f);

    float extremeLow = netproto::DequantizePos(netproto::QuantizePos(-1e9f));
    EXPECT_NEAR(extremeLow, -netproto::POS_RANGE, 0.01f);
}

TEST(NetProtocol, HelloAndWelcomeRoundtrip)
{
    uint8_t storage[1024];
    net::Buffer buf(storage, sizeof(storage));

    // 1. HelloMsg
    {
        netproto::HelloMsg in{ netproto::PROTOCOL_VERSION, "PlayerAlice" };
        net::BufferStreamWriter writer(buf);
        in.Write(writer);

        net::BufferStreamReader reader(buf);
        netproto::MsgType type = netproto::ReadMsgType(reader);
        EXPECT_EQ(type, netproto::MsgType::Hello);

        netproto::HelloMsg out;
        ASSERT_TRUE(out.Read(reader));
        EXPECT_EQ(out.version, in.version);
        EXPECT_EQ(out.name, in.name);
    }

    // 2. WelcomeMsg
    {
        netproto::WelcomeMsg in{ 42, 1024, 60, 20 };
        net::BufferStreamWriter writer(buf);
        in.Write(writer);

        net::BufferStreamReader reader(buf);
        netproto::MsgType type = netproto::ReadMsgType(reader);
        EXPECT_EQ(type, netproto::MsgType::Welcome);

        netproto::WelcomeMsg out;
        ASSERT_TRUE(out.Read(reader));
        EXPECT_EQ(out.clientId, 42u);
        EXPECT_EQ(out.serverTick, 1024u);
        EXPECT_EQ(out.tickRate, 60u);
        EXPECT_EQ(out.snapshotRate, 20u);
    }
}

TEST(NetProtocol, RejectMessageRoundtrip)
{
    uint8_t storage[512];
    net::Buffer buf(storage, sizeof(storage));

    netproto::RejectMsg in{ "Server is full (max 32 clients)" };
    net::BufferStreamWriter writer(buf);
    in.Write(writer);

    net::BufferStreamReader reader(buf);
    netproto::MsgType type = netproto::ReadMsgType(reader);
    EXPECT_EQ(type, netproto::MsgType::Reject);

    netproto::RejectMsg out;
    ASSERT_TRUE(out.Read(reader));
    EXPECT_EQ(out.reason, in.reason);
}

TEST(NetProtocol, SpawnAndDespawnRoundtrip)
{
    uint8_t storage[1024];
    net::Buffer buf(storage, sizeof(storage));

    // SpawnMsg
    {
        netproto::SpawnMsg in;
        in.netId = 5001;
        in.ownerClient = 3;
        in.prefab = "WarriorPrefab";
        in.position = glm::vec3(12.5f, -3.25f, 44.125f);
        in.rotation = glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 1, 0));

        net::BufferStreamWriter writer(buf);
        in.Write(writer);

        net::BufferStreamReader reader(buf);
        netproto::MsgType type = netproto::ReadMsgType(reader);
        EXPECT_EQ(type, netproto::MsgType::Spawn);

        netproto::SpawnMsg out;
        ASSERT_TRUE(out.Read(reader));
        EXPECT_EQ(out.netId, in.netId);
        EXPECT_EQ(out.ownerClient, in.ownerClient);
        EXPECT_EQ(out.prefab, in.prefab);
        EXPECT_NEAR(out.position.x, in.position.x, 0.005f);
        EXPECT_NEAR(out.position.y, in.position.y, 0.005f);
        EXPECT_NEAR(out.position.z, in.position.z, 0.005f);
        EXPECT_LT(QuatAngleDeg(out.rotation, in.rotation), 0.5f);
    }

    // DespawnMsg
    {
        netproto::DespawnMsg in{ 5001 };
        net::BufferStreamWriter writer(buf);
        in.Write(writer);

        net::BufferStreamReader reader(buf);
        netproto::MsgType type = netproto::ReadMsgType(reader);
        EXPECT_EQ(type, netproto::MsgType::Despawn);

        netproto::DespawnMsg out;
        ASSERT_TRUE(out.Read(reader));
        EXPECT_EQ(out.netId, 5001u);
    }
}

TEST(NetProtocol, InputBatchingAndSequenceRoundtrip)
{
    uint8_t storage[2048];
    net::Buffer buf(storage, sizeof(storage));

    netproto::InputMsg in;
    for (uint32_t i = 1; i <= 6; ++i)
    {
        netproto::InputCmd cmd;
        cmd.seq = 100 + i;
        cmd.move = glm::vec2(float(i) * 0.15f, -float(i) * 0.1f);
        cmd.yaw = 1.57f * float(i);
        cmd.buttons = netproto::InputButton_Jump | (i % 2 == 0 ? netproto::InputButton_Sprint : 0);
        in.cmds.push_back(cmd);
    }

    net::BufferStreamWriter writer(buf);
    in.Write(writer);

    net::BufferStreamReader reader(buf);
    netproto::MsgType type = netproto::ReadMsgType(reader);
    EXPECT_EQ(type, netproto::MsgType::Input);

    netproto::InputMsg out;
    ASSERT_TRUE(out.Read(reader));
    ASSERT_EQ(out.cmds.size(), in.cmds.size());

    for (size_t i = 0; i < in.cmds.size(); ++i)
    {
        EXPECT_EQ(out.cmds[i].seq, in.cmds[i].seq);
        EXPECT_NEAR(out.cmds[i].move.x, in.cmds[i].move.x, 0.01f);
        EXPECT_NEAR(out.cmds[i].move.y, in.cmds[i].move.y, 0.01f);
        EXPECT_NEAR(out.cmds[i].yaw, in.cmds[i].yaw, 0.001f);
        EXPECT_EQ(out.cmds[i].buttons, in.cmds[i].buttons);
    }
}

TEST(NetProtocol, SnapshotMsgSerializationRoundtrip)
{
    uint8_t storage[64 * 1024];
    net::Buffer buf(storage, sizeof(storage));

    netproto::SnapshotMsg in;
    in.serverTick = 54321;
    in.lastProcessedInputSeq = 88;

    for (uint64_t i = 0; i < 50; ++i)
    {
        netproto::EntityState st;
        st.netId = 2000 + i;
        st.position = glm::vec3(float(i) * 2.0f, -float(i) * 0.5f, 10.0f);
        st.rotation = glm::angleAxis(0.05f * float(i), glm::vec3(0, 1, 0));
        st.scale = glm::vec3(1.0f + float(i) * 0.01f, 1.0f, 1.0f);
        in.entities.push_back(st);
    }

    net::BufferStreamWriter writer(buf);
    in.Write(writer);

    net::BufferStreamReader reader(buf);
    netproto::MsgType type = netproto::ReadMsgType(reader);
    EXPECT_EQ(type, netproto::MsgType::Snapshot);

    netproto::SnapshotMsg out;
    ASSERT_TRUE(out.Read(reader));
    EXPECT_EQ(out.serverTick, in.serverTick);
    EXPECT_EQ(out.lastProcessedInputSeq, in.lastProcessedInputSeq);
    ASSERT_EQ(out.entities.size(), in.entities.size());

    for (size_t i = 0; i < in.entities.size(); ++i)
    {
        EXPECT_EQ(out.entities[i].netId, in.entities[i].netId);
        EXPECT_NEAR(out.entities[i].position.x, in.entities[i].position.x, 0.005f);
        EXPECT_NEAR(out.entities[i].position.y, in.entities[i].position.y, 0.005f);
        EXPECT_NEAR(out.entities[i].position.z, in.entities[i].position.z, 0.005f);
        EXPECT_LT(QuatAngleDeg(out.entities[i].rotation, in.entities[i].rotation), 0.5f);
        EXPECT_NEAR(out.entities[i].scale.x, in.entities[i].scale.x, 0.001f);
    }
}

TEST(NetProtocol, ChatSanitizationAndLengthClamping)
{
    uint8_t storage[2048];
    net::Buffer buf(storage, sizeof(storage));

    // Normal chat
    {
        netproto::ChatMsg in{ 7, "Hello world from client 7!" };
        net::BufferStreamWriter writer(buf);
        in.Write(writer);

        net::BufferStreamReader reader(buf);
        netproto::MsgType type = netproto::ReadMsgType(reader);
        EXPECT_EQ(type, netproto::MsgType::Chat);

        netproto::ChatMsg out;
        ASSERT_TRUE(out.Read(reader));
        EXPECT_EQ(out.clientId, 7u);
        EXPECT_EQ(out.text, in.text);
    }

    // Chat longer than MAX_LENGTH
    {
        std::string hugeString(500, 'Z');
        netproto::ChatMsg in{ 1, hugeString };
        net::BufferStreamWriter writer(buf);
        in.Write(writer);

        net::BufferStreamReader reader(buf);
        netproto::MsgType type = netproto::ReadMsgType(reader);
        EXPECT_EQ(type, netproto::MsgType::Chat);

        netproto::ChatMsg out;
        ASSERT_TRUE(out.Read(reader));
        EXPECT_LE(out.text.size(), netproto::ChatMsg::MAX_LENGTH);
    }
}

TEST(NetProtocol, PingPongLatencyRoundtrip)
{
    uint8_t storage[256];
    net::Buffer buf(storage, sizeof(storage));

    netproto::PingMsg ping{ 12345678 };
    net::BufferStreamWriter writer(buf);
    ping.Write(writer, netproto::MsgType::Ping);

    net::BufferStreamReader reader(buf);
    netproto::MsgType type = netproto::ReadMsgType(reader);
    EXPECT_EQ(type, netproto::MsgType::Ping);

    netproto::PingMsg out;
    ASSERT_TRUE(out.Read(reader));
    EXPECT_EQ(out.clientTimeMs, 12345678u);

    // Pong variant
    net::BufferStreamWriter pongWriter(buf);
    ping.Write(pongWriter, netproto::MsgType::Pong);

    net::BufferStreamReader pongReader(buf);
    EXPECT_EQ(netproto::ReadMsgType(pongReader), netproto::MsgType::Pong);
    netproto::PingMsg pongOut;
    ASSERT_TRUE(pongOut.Read(pongReader));
    EXPECT_EQ(pongOut.clientTimeMs, 12345678u);
}

TEST(NetProtocol, HostileBufferOverrunDefense)
{
    // 1. Empty buffer
    {
        uint8_t empty[1] = { 0 };
        net::Buffer buf(empty, 0);
        net::BufferStreamReader reader(buf);
        EXPECT_EQ(netproto::ReadMsgType(reader), netproto::MsgType::Invalid);

        netproto::SnapshotMsg snap;
        EXPECT_FALSE(snap.Read(reader));

        netproto::HelloMsg hello;
        EXPECT_FALSE(hello.Read(reader));
    }

    // 2. Truncated snapshot (header valid, but entity count truncated before entities)
    {
        uint8_t storage[32];
        net::Buffer buf(storage, sizeof(storage));
        net::BufferStreamWriter writer(buf);
        writer.WriteRaw(netproto::MsgType::Snapshot);
        writer.WriteRaw(uint32_t(100)); // serverTick
        writer.WriteRaw(uint32_t(1));   // lastProcessedInputSeq
        writer.WriteRaw(uint32_t(10));  // count = 10 entities, but 0 entities written!

        net::BufferStreamReader reader(buf);
        EXPECT_EQ(netproto::ReadMsgType(reader), netproto::MsgType::Snapshot);
        netproto::SnapshotMsg snap;
        // Should safely fail reading without crash or out-of-bounds memory access
        EXPECT_FALSE(snap.Read(reader));
    }

    // 3. Oversized hostile count in SnapshotMsg (> MAX_SNAPSHOT_ENTITIES)
    {
        uint8_t storage[32];
        net::Buffer buf(storage, sizeof(storage));
        net::BufferStreamWriter writer(buf);
        writer.WriteRaw(netproto::MsgType::Snapshot);
        writer.WriteRaw(uint32_t(100));
        writer.WriteRaw(uint32_t(1));
        writer.WriteRaw(uint32_t(netproto::MAX_SNAPSHOT_ENTITIES + 1000)); // Hostile count

        net::BufferStreamReader reader(buf);
        EXPECT_EQ(netproto::ReadMsgType(reader), netproto::MsgType::Snapshot);
        netproto::SnapshotMsg snap;
        // Must reject without trying to allocate 1000s of objects
        EXPECT_FALSE(snap.Read(reader));
    }

    // 4. Oversized count in InputMsg (> MAX_INPUTS_PER_PACKET)
    {
        uint8_t storage[32];
        net::Buffer buf(storage, sizeof(storage));
        net::BufferStreamWriter writer(buf);
        writer.WriteRaw(netproto::MsgType::Input);
        writer.WriteRaw(uint8_t(netproto::MAX_INPUTS_PER_PACKET + 10)); // Over limit

        net::BufferStreamReader reader(buf);
        EXPECT_EQ(netproto::ReadMsgType(reader), netproto::MsgType::Input);
        netproto::InputMsg inMsg;
        EXPECT_FALSE(inMsg.Read(reader));
    }
}

