// Copyright (c) 2026 Evangelion Manuhutu
// Standalone protocol round-trip tests (no engine dependencies).

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "ignite/net/net_protocol.hpp"
#include <net/BufferStream.hpp>

using namespace ignite;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); g_failures++; } } while (0)

static float QuatAngleDeg(const glm::quat &a, const glm::quat &b)
{
    float d = std::min(1.0f, std::abs(glm::dot(a, b)));
    return glm::degrees(2.0f * std::acos(d));
}

int main()
{
    uint8_t storage[64 * 1024];
    net::Buffer buf(storage, sizeof(storage));

    // Quaternion quantization error stays small across many orientations
    {
        float worst = 0.0f;
        for (int i = 0; i < 2000; i++)
        {
            glm::vec3 axis = glm::normalize(glm::vec3(std::sin(i * 1.3f) + 0.1f, std::cos(i * 0.7f), std::sin(i * 2.1f) - 0.2f));
            glm::quat q = glm::angleAxis(i * 0.173f, axis);
            glm::quat r = netproto::DequantizeQuat(netproto::QuantizeQuat(q));
            worst = std::max(worst, QuatAngleDeg(q, r));
        }
        std::printf("worst quat error: %.4f deg\n", worst);
        CHECK(worst < 0.5f);
    }

    // Position quantization
    {
        for (float v : { -1000.0f, -0.25f, 0.0f, 3.14159f, 999.999f })
            CHECK(std::abs(netproto::DequantizePos(netproto::QuantizePos(v)) - v) < 0.001f);
        // clamps instead of overflowing
        CHECK(std::abs(netproto::DequantizePos(netproto::QuantizePos(1e9f)) - netproto::POS_RANGE) < 0.01f);
    }

    // Snapshot round trip
    {
        netproto::SnapshotMsg in;
        in.serverTick = 1234;
        in.lastProcessedInputSeq = 99;
        for (uint64_t i = 0; i < 50; i++)
        {
            netproto::EntityState e;
            e.netId = 1000 + i;
            e.position = { float(i), -float(i) * 0.5f, 7.25f };
            e.rotation = glm::angleAxis(0.1f * float(i), glm::vec3(0, 1, 0));
            e.scale = { 1, 2, 3 };
            in.entities.push_back(e);
        }

        net::BufferStreamWriter w(buf);
        in.Write(w);
        std::printf("snapshot(50 entities) = %llu bytes\n", (unsigned long long)w.GetBuffer().Size);

        net::BufferStreamReader r(w.GetBuffer());
        CHECK(netproto::ReadMsgType(r) == netproto::MsgType::Snapshot);
        netproto::SnapshotMsg out;
        CHECK(out.Read(r));
        CHECK(out.serverTick == 1234 && out.lastProcessedInputSeq == 99);
        CHECK(out.entities.size() == 50);
        for (size_t i = 0; i < out.entities.size(); i++)
        {
            CHECK(out.entities[i].netId == in.entities[i].netId);
            CHECK(glm::distance(out.entities[i].position, in.entities[i].position) < 0.002f);
            CHECK(QuatAngleDeg(out.entities[i].rotation, in.entities[i].rotation) < 0.5f);
            CHECK(out.entities[i].scale == in.entities[i].scale);
        }
    }

    // Input round trip (only last MAX_INPUTS_PER_PACKET are sent)
    {
        netproto::InputMsg in;
        for (uint32_t i = 0; i < 12; i++)
            in.cmds.push_back({ i, { 1.0f, -0.5f }, 1.5f, netproto::InputButton_Jump });

        net::BufferStreamWriter w(buf);
        in.Write(w);
        net::BufferStreamReader r(w.GetBuffer());
        CHECK(netproto::ReadMsgType(r) == netproto::MsgType::Input);
        netproto::InputMsg out;
        CHECK(out.Read(r));
        CHECK(out.cmds.size() == netproto::MAX_INPUTS_PER_PACKET);
        CHECK(out.cmds.front().seq == 4 && out.cmds.back().seq == 11);
        CHECK(std::abs(out.cmds[0].move.y + 0.5f) < 0.01f);
    }

    // Hello / Welcome / Spawn
    {
        net::BufferStreamWriter w(buf);
        netproto::HelloMsg h; h.name = "Evan";
        h.Write(w);
        net::BufferStreamReader r(w.GetBuffer());
        CHECK(netproto::ReadMsgType(r) == netproto::MsgType::Hello);
        netproto::HelloMsg ho;
        CHECK(ho.Read(r) && ho.name == "Evan" && ho.version == netproto::PROTOCOL_VERSION);

        net::BufferStreamWriter w2(buf);
        netproto::SpawnMsg s; s.netId = 42; s.ownerClient = 7; s.prefab = "Player"; s.position = { 1, 2, 3 };
        s.Write(w2);
        net::BufferStreamReader r2(w2.GetBuffer());
        netproto::ReadMsgType(r2);
        netproto::SpawnMsg so;
        CHECK(so.Read(r2) && so.netId == 42 && so.ownerClient == 7 && so.prefab == "Player");
        CHECK(glm::distance(so.position, glm::vec3(1, 2, 3)) < 0.002f);
    }

    // Hostile packets must fail cleanly, not crash or allocate unbounded memory
    {
        // snapshot claiming 4 billion entities
        net::BufferStreamWriter w(buf);
        w.WriteRaw(netproto::MsgType::Snapshot);
        w.WriteRaw<uint32_t>(1); w.WriteRaw<uint32_t>(1); w.WriteRaw<uint32_t>(0xFFFFFFFFu);
        net::BufferStreamReader r(w.GetBuffer());
        netproto::ReadMsgType(r);
        netproto::SnapshotMsg out;
        CHECK(!out.Read(r));

        // string claiming 4 GiB
        net::BufferStreamWriter w2(buf);
        w2.WriteRaw(netproto::MsgType::Hello);
        w2.WriteRaw<uint16_t>(1);
        w2.WriteRaw<uint32_t>(0xFFFFFFFFu);
        net::BufferStreamReader r2(w2.GetBuffer());
        netproto::ReadMsgType(r2);
        netproto::HelloMsg h;
        CHECK(!h.Read(r2));

        // truncated snapshot
        net::BufferStreamWriter w3(buf);
        w3.WriteRaw(netproto::MsgType::Snapshot);
        w3.WriteRaw<uint32_t>(1); w3.WriteRaw<uint32_t>(1); w3.WriteRaw<uint32_t>(5);
        net::BufferStreamReader r3(w3.GetBuffer());
        netproto::ReadMsgType(r3);
        netproto::SnapshotMsg o3;
        CHECK(!o3.Read(r3));

        // empty buffer
        net::BufferStreamReader r4(net::Buffer(storage, 0));
        CHECK(netproto::ReadMsgType(r4) == netproto::MsgType::Invalid);
    }

    std::printf(g_failures == 0 ? "ALL TESTS PASSED\n" : "%d FAILURE(S)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}

