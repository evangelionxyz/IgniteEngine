// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_NET_PROTOCOL_HPP
#define IGN_NET_PROTOCOL_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <net/StreamWriter.hpp>
#include <net/StreamReader.hpp>

namespace ignite::netproto
{
    constexpr uint16_t PROTOCOL_VERSION = 1;
    constexpr uint32_t MAX_SNAPSHOT_ENTITIES = 4096;

    // Wire message types. The first byte of every packet.
    enum class MsgType : uint8_t
    {
        Invalid = 0,
        Hello,      // C->S reliable: protocol version, name
        Welcome,    // S->C reliable: client id, server tick, tick rate
        Reject,     // S->C reliable: reason
        Spawn,      // S->C reliable: replicated entity spawn
        Despawn,    // S->C reliable: replicated entity removal
        Input,      // C->S unreliable: input commands (redundant)
        Snapshot,   // S->C unreliable: entity states + last processed input
        Ping,       // C->S unreliable
        Pong,       // S->C unreliable
        Chat,       // both reliable
    };

    // -----------------------------------------------------------------
    // Quantization helpers
    // -----------------------------------------------------------------
    // Positions are clamped to +-POS_RANGE and stored as 32-bit fixed point
    // (~0.0005 resolution at 1000 units), well below visual threshold.
    constexpr float POS_RANGE = 16384.0f;
    constexpr float POS_SCALE = 2048.0f;

    inline int32_t QuantizePos(float v)
    {
        v = std::clamp(v, -POS_RANGE, POS_RANGE);
        return static_cast<int32_t>(std::lround(v * POS_SCALE));
    }

    inline float DequantizePos(int32_t q)
    {
        return static_cast<float>(q) / POS_SCALE;
    }

    // Smallest-three quaternion: 2 bits index of largest component + 3x10 bits
    inline uint32_t QuantizeQuat(glm::quat q)
    {
        q = glm::normalize(q);
        const float comps[4] = { q.x, q.y, q.z, q.w };

        int largest = 0;
        for (int i = 1; i < 4; i++)
        {
            if (std::abs(comps[i]) > std::abs(comps[largest]))
                largest = i;
        }

        const float sign = comps[largest] < 0.0f ? -1.0f : 1.0f;
        constexpr float kMax = 0.70710678f; // 1/sqrt(2)
        uint32_t packed = static_cast<uint32_t>(largest) << 30;
        int shift = 20;
        for (int i = 0; i < 4; i++)
        {
            if (i == largest)
                continue;

            const float n = std::clamp(comps[i] * sign / kMax, -1.0f, 1.0f);
            const uint32_t v = static_cast<uint32_t>(std::lround((n * 0.5f + 0.5f) * 1023.0f));
            packed |= (v & 0x3FFu) << shift;
            shift -= 10;
        }
        return packed;
    }

    inline glm::quat DequantizeQuat(uint32_t packed)
    {
        constexpr float kMax = 0.70710678f;
        const int largest = static_cast<int>(packed >> 30);

        float comps[4] = { 0, 0, 0, 0 };
        float sumSq = 0.0f;
        int shift = 20;
        for (int i = 0; i < 4; i++)
        {
            if (i == largest)
                continue;

            const uint32_t v = (packed >> shift) & 0x3FFu;
            const float n = (static_cast<float>(v) / 1023.0f) * 2.0f - 1.0f;
            comps[i] = n * kMax;
            sumSq += comps[i] * comps[i];
            shift -= 10;
        }
        comps[largest] = std::sqrt(std::max(0.0f, 1.0f - sumSq));
        return glm::normalize(glm::quat(comps[3], comps[0], comps[1], comps[2]));
    }

    // -----------------------------------------------------------------
    // Messages
    // -----------------------------------------------------------------
    struct HelloMsg
    {
        uint16_t version = PROTOCOL_VERSION;
        std::string name;

        void Write(net::StreamWriter &w) const
        {
            w.WriteRaw(MsgType::Hello);
            w.WriteRaw(version);
            w.WriteString(name);
        }

        bool Read(net::StreamReader &r)
        {
            return r.ReadRaw(version) && r.ReadString(name);
        }
    };

    struct WelcomeMsg
    {
        uint32_t clientId = 0;
        uint32_t serverTick = 0;
        uint16_t tickRate = 60;
        uint16_t snapshotRate = 20;

        void Write(net::StreamWriter &w) const
        {
            w.WriteRaw(MsgType::Welcome);
            w.WriteRaw(clientId);
            w.WriteRaw(serverTick);
            w.WriteRaw(tickRate);
            w.WriteRaw(snapshotRate);
        }

        bool Read(net::StreamReader &r)
        {
            return r.ReadRaw(clientId) && r.ReadRaw(serverTick) && r.ReadRaw(tickRate) && r.ReadRaw(snapshotRate);
        }
    };

    struct RejectMsg
    {
        std::string reason;

        void Write(net::StreamWriter &w) const
        {
            w.WriteRaw(MsgType::Reject);
            w.WriteString(reason);
        }

        bool Read(net::StreamReader &r) { return r.ReadString(reason); }
    };

    struct SpawnMsg
    {
        uint64_t netId = 0;
        uint32_t ownerClient = 0; // 0 = server owned
        std::string prefab;       // template/prefab identifier
        glm::vec3 position{ 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };

        void Write(net::StreamWriter &w) const
        {
            w.WriteRaw(MsgType::Spawn);
            w.WriteRaw(netId);
            w.WriteRaw(ownerClient);
            w.WriteString(prefab);
            w.WriteRaw(QuantizePos(position.x));
            w.WriteRaw(QuantizePos(position.y));
            w.WriteRaw(QuantizePos(position.z));
            w.WriteRaw(QuantizeQuat(rotation));
        }

        bool Read(net::StreamReader &r)
        {
            int32_t x, y, z;
            uint32_t q;
            if (!(r.ReadRaw(netId) && r.ReadRaw(ownerClient) && r.ReadString(prefab)
                && r.ReadRaw(x) && r.ReadRaw(y) && r.ReadRaw(z) && r.ReadRaw(q)))
                return false;

            position = { DequantizePos(x), DequantizePos(y), DequantizePos(z) };
            rotation = DequantizeQuat(q);
            return true;
        }
    };

    struct DespawnMsg
    {
        uint64_t netId = 0;

        void Write(net::StreamWriter &w) const
        {
            w.WriteRaw(MsgType::Despawn);
            w.WriteRaw(netId);
        }

        bool Read(net::StreamReader &r) { return r.ReadRaw(netId); }
    };

    // Input flags
    enum InputButtons : uint16_t
    {
        InputButton_None = 0,
        InputButton_Jump = 1 << 0,
        InputButton_Sprint = 1 << 1,
        InputButton_Fire = 1 << 2,
        InputButton_Use = 1 << 3,
    };

    struct InputCmd
    {
        uint32_t seq = 0;
        glm::vec2 move{ 0.0f }; // clamped to [-1, 1]
        float yaw = 0.0f;
        uint16_t buttons = 0;

        void Write(net::StreamWriter &w) const
        {
            w.WriteRaw(seq);
            w.WriteRaw(static_cast<int8_t>(std::lround(std::clamp(move.x, -1.0f, 1.0f) * 127.0f)));
            w.WriteRaw(static_cast<int8_t>(std::lround(std::clamp(move.y, -1.0f, 1.0f) * 127.0f)));
            w.WriteRaw(yaw);
            w.WriteRaw(buttons);
        }

        bool Read(net::StreamReader &r)
        {
            int8_t mx, my;
            if (!(r.ReadRaw(seq) && r.ReadRaw(mx) && r.ReadRaw(my) && r.ReadRaw(yaw) && r.ReadRaw(buttons)))
                return false;

            move = { static_cast<float>(mx) / 127.0f, static_cast<float>(my) / 127.0f };
            if (!std::isfinite(yaw))
                yaw = 0.0f;
            return true;
        }
    };

    // Client sends the last few commands each packet so single losses are tolerated.
    constexpr uint8_t MAX_INPUTS_PER_PACKET = 8;

    struct InputMsg
    {
        std::vector<InputCmd> cmds; // oldest first

        void Write(net::StreamWriter &w) const
        {
            w.WriteRaw(MsgType::Input);
            const uint8_t count = static_cast<uint8_t>(std::min<size_t>(cmds.size(), MAX_INPUTS_PER_PACKET));
            w.WriteRaw(count);
            for (size_t i = cmds.size() - count; i < cmds.size(); i++)
                cmds[i].Write(w);
        }

        bool Read(net::StreamReader &r)
        {
            uint8_t count = 0;
            if (!r.ReadRaw(count) || count > MAX_INPUTS_PER_PACKET)
                return false;

            cmds.resize(count);
            for (auto &cmd : cmds)
            {
                if (!cmd.Read(r))
                    return false;
            }
            return true;
        }
    };

    struct EntityState
    {
        uint64_t netId = 0;
        glm::vec3 position{ 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 scale{ 1.0f };

        void Write(net::StreamWriter &w) const
        {
            w.WriteRaw(netId);
            w.WriteRaw(QuantizePos(position.x));
            w.WriteRaw(QuantizePos(position.y));
            w.WriteRaw(QuantizePos(position.z));
            w.WriteRaw(QuantizeQuat(rotation));
            w.WriteRaw(scale.x);
            w.WriteRaw(scale.y);
            w.WriteRaw(scale.z);
        }

        bool Read(net::StreamReader &r)
        {
            int32_t x, y, z;
            uint32_t q;
            if (!(r.ReadRaw(netId) && r.ReadRaw(x) && r.ReadRaw(y) && r.ReadRaw(z) && r.ReadRaw(q)
                && r.ReadRaw(scale.x) && r.ReadRaw(scale.y) && r.ReadRaw(scale.z)))
                return false;

            position = { DequantizePos(x), DequantizePos(y), DequantizePos(z) };
            rotation = DequantizeQuat(q);
            return true;
        }
    };

    struct SnapshotMsg
    {
        uint32_t serverTick = 0;
        uint32_t lastProcessedInputSeq = 0;
        std::vector<EntityState> entities;

        void Write(net::StreamWriter &w) const
        {
            w.WriteRaw(MsgType::Snapshot);
            w.WriteRaw(serverTick);
            w.WriteRaw(lastProcessedInputSeq);
            w.WriteRaw(static_cast<uint32_t>(entities.size()));
            for (const auto &e : entities)
                e.Write(w);
        }

        bool Read(net::StreamReader &r)
        {
            uint32_t count = 0;
            if (!(r.ReadRaw(serverTick) && r.ReadRaw(lastProcessedInputSeq) && r.ReadRaw(count)))
                return false;

            // Reject hostile sizes before allocating.
            if (count > MAX_SNAPSHOT_ENTITIES)
                return false;

            entities.resize(count);
            for (auto &e : entities)
            {
                if (!e.Read(r))
                    return false;
            }
            return true;
        }
    };

    struct ChatMsg
    {
        static constexpr size_t MAX_LENGTH = 256;

        uint32_t clientId = 0; // filled by the server on relay
        std::string text;

        void Write(net::StreamWriter &w) const
        {
            w.WriteRaw(MsgType::Chat);
            w.WriteRaw(clientId);
            w.WriteString(text.size() > MAX_LENGTH ? text.substr(0, MAX_LENGTH) : text);
        }

        bool Read(net::StreamReader &r)
        {
            if (!(r.ReadRaw(clientId) && r.ReadString(text)))
                return false;
            if (text.size() > MAX_LENGTH)
                text.resize(MAX_LENGTH);
            return true;
        }
    };

    struct PingMsg
    {
        uint32_t clientTimeMs = 0;

        void Write(net::StreamWriter &w, MsgType type = MsgType::Ping) const
        {
            w.WriteRaw(type);
            w.WriteRaw(clientTimeMs);
        }

        bool Read(net::StreamReader &r) { return r.ReadRaw(clientTimeMs); }
    };

    // Reads the leading message type; returns Invalid when the stream is empty/short.
    inline MsgType ReadMsgType(net::StreamReader &r)
    {
        MsgType type = MsgType::Invalid;
        if (!r.ReadRaw(type))
            return MsgType::Invalid;
        return type;
    }
}

#endif

