// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_CORE_AABB_HPP
#define IGN_CORE_AABB_HPP

#include "ignite/core/base.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

#if defined(__AVX2__) || defined(__AVX__) || defined(__SSE__)
#include <immintrin.h>
#endif

namespace ignite
{
    class MeshInstance;

    struct IGN_CORE_API AABB
    {
        glm::vec3 min = glm::vec3(0.0f);
        glm::vec3 max = glm::vec3(0.0f);

        static inline AABB FromMinMax(const glm::vec3 &min, const glm::vec3 &max)
        {
            AABB aabb;
            aabb.min = min;
            aabb.max = max;
            return aabb;
        }

        const glm::vec3 GetCenter() const
        {
            return (min + max) / 2.0f;
        }

        const glm::vec3 GetSize() const
        {
            return max - min;
        }

        inline bool RayIntersection(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection) const
        {
            float outT;
            return IntersectRay(rayOrigin, rayDirection, outT);
        }

        // Ray intersection that returns distance to intersection in outT (near hit)
        inline bool IntersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, float &outT) const
        {
#if defined(__AVX2__) || defined(__AVX__) || defined(__SSE__)
            __m128 vMin    = _mm_set_ps(0.0f, min.z, min.y, min.x);
            __m128 vMax    = _mm_set_ps(0.0f, max.z, max.y, max.x);
            __m128 vOrigin = _mm_set_ps(0.0f, rayOrigin.z, rayOrigin.y, rayOrigin.x);
            __m128 vInvDir = _mm_set_ps(0.0f, 1.0f / rayDirection.z, 1.0f / rayDirection.y, 1.0f / rayDirection.x);

            __m128 t0 = _mm_mul_ps(_mm_sub_ps(vMin, vOrigin), vInvDir);
            __m128 t1 = _mm_mul_ps(_mm_sub_ps(vMax, vOrigin), vInvDir);

            __m128 tsmaller = _mm_min_ps(t0, t1);
            __m128 tbigger  = _mm_max_ps(t0, t1);

            alignas(16) float ts[4];
            alignas(16) float tb[4];
            _mm_store_ps(ts, tsmaller);
            _mm_store_ps(tb, tbigger);

            float tmin = std::max({ ts[0], ts[1], ts[2] });
            float tmax = std::min({ tb[0], tb[1], tb[2] });

            if (tmax >= std::max(tmin, 0.0f))
            {
                outT = tmin < 0.0f ? tmax : tmin;
                return true;
            }

            return false;
#else
            const glm::vec3 invDir = 1.0f / rayDirection;
            glm::vec3 t0s = (min - rayOrigin) * invDir;
            glm::vec3 t1s = (max - rayOrigin) * invDir;

            glm::vec3 tsmaller = glm::min(t0s, t1s);
            glm::vec3 tbigger = glm::max(t0s, t1s);

            float tmin = glm::max(glm::max(tsmaller.x, tsmaller.y), tsmaller.z);
            float tmax = glm::min(glm::min(tbigger.x, tbigger.y), tbigger.z);

            if (tmax >= glm::max(tmin, 0.0f))
            {
                outT = tmin < 0.0f ? tmax : tmin;
                return true;
            }

            return false;
#endif
        }

        inline AABB Transform(const glm::mat4 &M) const
        {
#if defined(__AVX2__) || defined(__AVX__) || defined(__SSE__)
            __m128 col3 = _mm_loadu_ps(&M[3][0]);
            __m128 vMin = col3;
            __m128 vMax = col3;

            for (int i = 0; i < 3; ++i)
            {
                __m128 col = _mm_loadu_ps(&M[i][0]);
                __m128 a   = _mm_mul_ps(col, _mm_set1_ps(min[i]));
                __m128 b   = _mm_mul_ps(col, _mm_set1_ps(max[i]));

                vMin = _mm_add_ps(vMin, _mm_min_ps(a, b));
                vMax = _mm_add_ps(vMax, _mm_max_ps(a, b));
            }

            alignas(16) float resMin[4];
            alignas(16) float resMax[4];
            _mm_store_ps(resMin, vMin);
            _mm_store_ps(resMax, vMax);

            AABB result;
            result.min = glm::vec3(resMin[0], resMin[1], resMin[2]);
            result.max = glm::vec3(resMax[0], resMax[1], resMax[2]);
            return result;
#else
            glm::vec3 corners[8] = {
                glm::vec3(min.x, min.y, min.z),
                glm::vec3(min.x, min.y, max.z),
                glm::vec3(min.x, max.y, min.z),
                glm::vec3(min.x, max.y, max.z),
                glm::vec3(max.x, min.y, min.z),
                glm::vec3(max.x, min.y, max.z),
                glm::vec3(max.x, max.y, min.z),
                glm::vec3(max.x, max.y, max.z)
            };

            AABB result;
            result.min = glm::vec3(M * glm::vec4(corners[0], 1.0f));
            result.max = result.min;

            for (int i = 1; i < 8; ++i)
            {
                glm::vec3 transformed = glm::vec3(M * glm::vec4(corners[i], 1.0f));
                result.min = glm::min(result.min, transformed);
                result.max = glm::max(result.max, transformed);
            }
            return result;
#endif
        }
    };
}

#endif
