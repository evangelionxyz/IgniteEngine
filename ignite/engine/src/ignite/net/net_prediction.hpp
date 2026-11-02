// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_NET_PREDICTION_HPP
#define IGN_NET_PREDICTION_HPP

#include "net_protocol.hpp"
#include <ignite/core/base.hpp>

#include <deque>
#include <functional>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ignite
{
    // Client-side prediction and server reconciliation buffer.
    // Records predicted states alongside the inputs that produced them.
    // When a server snapshot arrives, it rewinds to the last acknowledged input
    // sequence, verifies server agreement, and replays unacknowledged inputs.
    class IGN_API ClientPrediction
    {
    public:
        struct PredictedState
        {
            uint32_t seq = 0;
            uint32_t tick = 0;
            glm::vec3 position{ 0.0f };
            glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
            netproto::InputCmd cmd;
        };

        // Function that simulates one input step given a state and dt.
        // Used to replay unacknowledged inputs on reconciliation.
        using SimulationFn = std::function<void(const netproto::InputCmd &cmd, float dt, glm::vec3 &inOutPos, glm::quat &inOutRot)>;

    public:
        explicit ClientPrediction(float errorThreshold = 0.05f)
            : m_ErrorThreshold(errorThreshold)
        {
        }

        void Reset()
        {
            m_History.clear();
            m_VisualOffset = glm::vec3(0.0f);
        }

        // Record a predicted state after local physics/movement step.
        void Record(uint32_t seq, uint32_t tick, const glm::vec3 &pos, const glm::quat &rot, const netproto::InputCmd &cmd)
        {
            m_History.push_back({ seq, tick, pos, glm::normalize(rot), cmd });
            if (m_History.size() > kMaxHistory)
                m_History.pop_front();
        }

        // Reconcile against server authoritative state for an acknowledged input sequence.
        // Returns true if a correction was applied.
        bool Reconcile(uint32_t ackedSeq, const glm::vec3 &serverPos, const glm::quat &serverRot,
                       float tickDt, const SimulationFn &simulateFn,
                       glm::vec3 &outCorrectedPos, glm::quat &outCorrectedRot)
        {
            if (m_History.empty())
                return false;

            // Find the history frame corresponding to ackedSeq
            size_t ackIdx = 0;
            bool found = false;
            for (size_t i = 0; i < m_History.size(); ++i)
            {
                if (m_History[i].seq == ackedSeq)
                {
                    ackIdx = i;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                // Discard frames older than ackedSeq if ackedSeq is beyond our window
                while (!m_History.empty() && m_History.front().seq < ackedSeq)
                    m_History.pop_front();
                return false;
            }

            const auto &predictedFrame = m_History[ackIdx];
            const float posError = glm::distance(predictedFrame.position, serverPos);

            bool corrected = false;
            if (posError > m_ErrorThreshold)
            {
                // Misprediction detected: rewind to server state and replay
                glm::vec3 currentPos = serverPos;
                glm::quat currentRot = serverRot;

                for (size_t i = ackIdx + 1; i < m_History.size(); ++i)
                {
                    if (simulateFn)
                    {
                        simulateFn(m_History[i].cmd, tickDt, currentPos, currentRot);
                    }
                    m_History[i].position = currentPos;
                    m_History[i].rotation = currentRot;
                }

                // Add visual error offset to smoothly blend out correction artifact
                m_VisualOffset += (predictedFrame.position - serverPos);

                outCorrectedPos = currentPos;
                outCorrectedRot = currentRot;
                corrected = true;
            }

            // Evict all frames up to and including the acked sequence
            m_History.erase(m_History.begin(), m_History.begin() + ackIdx + 1);

            return corrected;
        }

        // Smooth visual offset decay (call per render frame)
        void UpdateVisualOffset(float deltaTime, float blendSpeed = 15.0f)
        {
            if (glm::dot(m_VisualOffset, m_VisualOffset) > 0.0001f)
            {
                m_VisualOffset = glm::mix(m_VisualOffset, glm::vec3(0.0f), glm::clamp(deltaTime * blendSpeed, 0.0f, 1.0f));
            }
            else
            {
                m_VisualOffset = glm::vec3(0.0f);
            }
        }

        glm::vec3 GetVisualOffset() const { return m_VisualOffset; }
        const std::deque<PredictedState> &GetHistory() const { return m_History; }

    private:
        static constexpr size_t kMaxHistory = 128;
        float m_ErrorThreshold = 0.05f;
        glm::vec3 m_VisualOffset{ 0.0f };
        std::deque<PredictedState> m_History;
    };
}

#endif

