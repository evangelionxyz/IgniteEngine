// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_NET_INTERPOLATOR_HPP
#define IGN_NET_INTERPOLATOR_HPP

#include <ignite/core/base.hpp>

#include <deque>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ignite
{
    // Snapshot interpolator for remote entities.
    // Buffers incoming entity states and smoothly interpolates between them
    // based on a configurable render delay (e.g. 100ms).
    class IGN_API NetworkTransformInterpolator
    {
    public:
        struct Snapshot
        {
            uint32_t tick = 0;
            double timestamp = 0.0;
            glm::vec3 position{ 0.0f };
            glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
            glm::vec3 scale{ 1.0f };
        };

    public:
        NetworkTransformInterpolator(double interpolationDelay = 0.1, double maxExtrapolation = 0.15)
            : m_InterpolationDelay(interpolationDelay), m_MaxExtrapolation(maxExtrapolation)
        {
        }

        void Reset()
        {
            m_Snapshots.clear();
        }

        void AddSnapshot(uint32_t tick, double timestamp, const glm::vec3 &pos, const glm::quat &rot, const glm::vec3 &scale)
        {
            if (!m_Snapshots.empty() && timestamp <= m_Snapshots.back().timestamp)
                return; // drop duplicate or out-of-order snapshot

            m_Snapshots.push_back({ tick, timestamp, pos, glm::normalize(rot), scale });
            if (m_Snapshots.size() > kMaxSnapshots)
                m_Snapshots.pop_front();
        }

        bool Sample(double currentTime, glm::vec3 &outPos, glm::quat &outRot, glm::vec3 &outScale) const
        {
            if (m_Snapshots.empty())
                return false;

            if (m_Snapshots.size() == 1)
            {
                outPos = m_Snapshots.front().position;
                outRot = m_Snapshots.front().rotation;
                outScale = m_Snapshots.front().scale;
                return true;
            }

            const double renderTime = currentTime - m_InterpolationDelay;

            // Before our oldest snapshot: clamp to oldest
            if (renderTime <= m_Snapshots.front().timestamp)
            {
                outPos = m_Snapshots.front().position;
                outRot = m_Snapshots.front().rotation;
                outScale = m_Snapshots.front().scale;
                return true;
            }

            // Find the two surrounding snapshots for interpolation
            for (size_t i = 0; i < m_Snapshots.size() - 1; ++i)
            {
                const auto &s0 = m_Snapshots[i];
                const auto &s1 = m_Snapshots[i + 1];

                if (renderTime >= s0.timestamp && renderTime <= s1.timestamp)
                {
                    const double delta = s1.timestamp - s0.timestamp;
                    const float factor = delta > 0.0001 ? static_cast<float>((renderTime - s0.timestamp) / delta) : 0.0f;

                    outPos = glm::mix(s0.position, s1.position, factor);
                    outRot = glm::slerp(s0.rotation, s1.rotation, factor);
                    outScale = glm::mix(s0.scale, s1.scale, factor);
                    return true;
                }
            }

            // Past our latest snapshot: extrapolate up to maxExtrapolation
            const auto &sNew = m_Snapshots.back();
            const auto &sPrev = m_Snapshots[m_Snapshots.size() - 2];
            const double timePastLatest = renderTime - sNew.timestamp;

            if (timePastLatest <= m_MaxExtrapolation)
            {
                const double dt = sNew.timestamp - sPrev.timestamp;
                if (dt > 0.0001)
                {
                    const glm::vec3 velocity = (sNew.position - sPrev.position) / static_cast<float>(dt);
                    outPos = sNew.position + velocity * static_cast<float>(timePastLatest);
                    outRot = sNew.rotation;
                    outScale = sNew.scale;
                    return true;
                }
            }

            outPos = sNew.position;
            outRot = sNew.rotation;
            outScale = sNew.scale;
            return true;
        }

        void SetInterpolationDelay(double delaySec) { m_InterpolationDelay = delaySec; }
        double GetInterpolationDelay() const { return m_InterpolationDelay; }

    private:
        static constexpr size_t kMaxSnapshots = 32;
        double m_InterpolationDelay = 0.1;
        double m_MaxExtrapolation = 0.15;
        std::deque<Snapshot> m_Snapshots;
    };
}

#endif

