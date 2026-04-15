// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef KEYFRAMES_HPP
#define KEYFRAMES_HPP

#include "ignite/core/types.hpp"
#include "ignite/math/math.hpp"

namespace ignite {

    template<typename T>
    struct KeyFrame
    {
        T Value;
        float Timestamp;
    };

    template<typename T>
    using KeyFrames = std::vector<KeyFrame<T>>;

    struct TransformKeyFrameBase
    {
    protected:
        float GetScaleFactor(float last_time_stamp, float next_time_stamp, float time)
        {
            float midWayLength = time - last_time_stamp;
            float framesDiff = next_time_stamp - last_time_stamp;
            if (fabs(framesDiff) <= 0.000001f)
            {
                return 0.0f;
            }

            float scale_factor = midWayLength / framesDiff;
            return glm::clamp(scale_factor, 0.0f, 1.0f);
        }
    };

    struct Vec3Key : TransformKeyFrameBase
    {
        KeyFrames<glm::vec3> frames;

        void AddFrame(const KeyFrame<glm::vec3> &key_frame)
        {
            frames.push_back(key_frame);
        }

        int32_t GetIndex(float anim_time)
        {
            for (int32_t i = 0; i < frames.size() - 1; ++i)
            {
                if (anim_time < frames[i + 1].Timestamp)
                {
                    return i;
                }
            }

            return static_cast<int32_t>(frames.size()) - 2;
        }

        glm::vec3 InterpolateTranslation(float time)
        {
            if (frames.size() == 1)
            {
                return frames[0].Value;
            }

            int32_t p0Index = GetIndex(time);
            int32_t p1Index = p0Index + 1;
            float scale_factor = GetScaleFactor(frames[p0Index].Timestamp, frames[p1Index].Timestamp, time);
            return glm::mix(frames[p0Index].Value, frames[p1Index].Value, scale_factor);
        }

        glm::vec3 InterpolateScaling(float time)
        {
            if (frames.size() == 1)
                return frames[0].Value;
            int32_t p0Index = GetIndex(time);
            int32_t p1Index = p0Index + 1;
            float scale_factor = GetScaleFactor(frames[p0Index].Timestamp, frames[p1Index].Timestamp, time);
            return glm::mix(frames[p0Index].Value, frames[p1Index].Value, scale_factor);
        }

        glm::mat4 InterpolateTranslateMatrix(float time)
        {
            return glm::translate(glm::mat4(1.0f), InterpolateTranslation(time));
        }

        glm::mat4 InterpolateScaleMatrix(float time)
        {
            return glm::scale(glm::mat4(1.0f), InterpolateScaling(time));
        }
    };

    struct QuatKey : public TransformKeyFrameBase
    {
        KeyFrames<glm::quat> frames;

        void AddFrame(const KeyFrame<glm::quat> &keyFrame)
        {
            frames.push_back(keyFrame);
        }

        int32_t GetIndex(float time)
        {
            for (int32_t i = 0; i < frames.size() - 1; ++i)
            {
                if (time < frames[i + 1].Timestamp)
                {
                    return i;
                }
            }
            return static_cast<int32_t>(frames.size()) - 2;
        }

        glm::quat InterpolateRotation(float time)
        {
            if (frames.size() == 1)
                return glm::normalize(frames[0].Value);

            int32_t p0Index = GetIndex(time);
            int32_t p1Index = p0Index + 1;
            float scale_factor = GetScaleFactor(frames[p0Index].Timestamp, frames[p1Index].Timestamp, time);
            glm::quat final_rotation = glm::slerp(frames[p0Index].Value, frames[p1Index].Value, scale_factor);
            return glm::normalize(final_rotation);
        }

        glm::mat4 InterpolateRotationMatrix(float time)
        {
            return glm::toMat4(InterpolateRotation(time));
        }
    };
}

#endif
