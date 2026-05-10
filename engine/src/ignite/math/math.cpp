// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "math.hpp"
#include "ignite/scene/component.hpp"

namespace ignite
{
    glm::mat4 Math::ComposeTransformComponent(const TransformComponent &transform)
    {
        const glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform.translation);
        const glm::mat4 rotation = glm::toMat4(transform.rotation);
        const glm::mat4 scale = glm::scale(glm::mat4(1.0f), transform.scale);
        return translation * rotation * scale;
    }

    void Math::DecomposeTransformComponent(const glm::mat4 &matrix, TransformComponent &outTransform)
    {
        glm::vec3 scale;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat orientation;
        glm::vec3 translation;

        if (!glm::decompose(matrix, scale, orientation, translation, skew, perspective))
        {
            translation = glm::vec3(0.0f);
            orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            scale = glm::vec3(1.0f);
        }

        orientation = glm::normalize(orientation);

        outTransform.translation = translation;
        outTransform.scale = scale;
        outTransform.rotation = orientation;
    }

    bool Math::DecomposeTransform(const glm::mat4 &transform, glm::vec3 &translation, glm::quat &outOrientation, glm::vec3 &outScale)
    {
        glm::mat4 localMatrix(transform);

        // Normalize the matrix.
        if (glm::epsilonEqual(localMatrix[3][3], static_cast<float>(0), glm::epsilon<float>()))
            return false;

        // First, isolate perspective. This is the messiest.
        if (glm::epsilonNotEqual(localMatrix[0][3], static_cast<float>(0), glm::epsilon<float>()) ||
            glm::epsilonNotEqual(localMatrix[1][3], static_cast<float>(0), glm::epsilon<float>()) ||
            glm::epsilonNotEqual(localMatrix[2][3], static_cast<float>(0), glm::epsilon<float>()))
        {
            // Clear the perspective partition
            localMatrix[0][3] = localMatrix[1][3] = localMatrix[2][3] = static_cast<float>(0);
            localMatrix[3][3] = static_cast<float>(1);
        }

        // Next take care of translation (easy).
        translation = glm::vec3(localMatrix[3]);
        localMatrix[3] = glm::vec4(0, 0, 0, localMatrix[3].w);

        glm::vec3 Row[3], Pdum3{};

        // Now get scale and shear.
        for (glm::length_t i = 0; i < 3; ++i)
            for (glm::length_t j = 0; j < 3; ++j)
                Row[i][j] = localMatrix[i][j];

        // Compute X scale factor and normalize first row.
        outScale.x = length(Row[0]);
        Row[0] = glm::detail::scale(Row[0], static_cast<float>(1));
        outScale.y = length(Row[1]);
        Row[1] = glm::detail::scale(Row[1], static_cast<float>(1));
        outScale.z = length(Row[2]);
        Row[2] = glm::detail::scale(Row[2], static_cast<float>(1));

        outOrientation.y = asin(-Row[0][2]);
        if (cos(outOrientation.y) != 0)
        {
            outOrientation.x = atan2(Row[1][2], Row[2][2]);
            outOrientation.z = atan2(Row[0][1], Row[0][0]);
        }
        else
        {
            outOrientation.x = atan2(-Row[2][0], Row[1][1]);
            outOrientation.z = 0;
        }


        return true;
    }

    bool Math::DecomposeTransformEuler(const glm::mat4 &transform, glm::vec3 &outTranslation, glm::vec3 &outRotation, glm::vec3 &outScale)
    {
        glm::mat4 localMatrix(transform);

        // Normalize the matrix.
        if (glm::epsilonEqual(localMatrix[3][3], static_cast<float>(0), glm::epsilon<float>()))
            return false;

        // First, isolate perspective.  This is the messiest.
        if (glm::epsilonNotEqual(localMatrix[0][3], static_cast<float>(0), glm::epsilon<float>()) ||
            glm::epsilonNotEqual(localMatrix[1][3], static_cast<float>(0), glm::epsilon<float>()) ||
            glm::epsilonNotEqual(localMatrix[2][3], static_cast<float>(0), glm::epsilon<float>()))
        {
            // Clear the perspective partition
            localMatrix[0][3] = localMatrix[1][3] = localMatrix[2][3] = static_cast<float>(0);
            localMatrix[3][3] = static_cast<float>(1);
        }

        // Next take care of translation (easy).
        outTranslation = glm::vec3(localMatrix[3]);
        localMatrix[3] = glm::vec4(0, 0, 0, localMatrix[3].w);

        glm::vec3 Row[3];

        // Now get scale and shear.
        for (glm::length_t i = 0; i < 3; ++i)
            for (glm::length_t j = 0; j < 3; ++j)
                Row[i][j] = localMatrix[i][j];

        // Compute X scale factor and normalize first row.
        outScale.x = length(Row[0]);
        Row[0] = glm::detail::scale(Row[0], static_cast<float>(1));
        outScale.y = length(Row[1]);
        Row[1] = glm::detail::scale(Row[1], static_cast<float>(1));
        outScale.z = length(Row[2]);
        Row[2] = glm::detail::scale(Row[2], static_cast<float>(1));

        outRotation.y = asin(-Row[0][2]);
        if (cos(outRotation.y) != 0)
        {
            outRotation.x = atan2(Row[1][2], Row[2][2]);
            outRotation.z = atan2(Row[0][1], Row[0][0]);
        }
        else
        {
            outRotation.x = atan2(-Row[2][0], Row[1][1]);
            outRotation.z = 0;
        }

        return true;
    }

    glm::vec3 Math::Normalize(const glm::vec3 &v)
    {
        float l = glm::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (l > 0.0f)
            return { v.x / l, v.y / l, v.z / l };
        return v;
    }

    glm::vec3 Math::WorldToScreen(const glm::vec3 &worldPosition, const glm::mat4 &worldTransform,
        const glm::mat4 &viewProjection, const glm::vec2 &screenSize)
    {
        glm::vec4 modelPos = worldTransform * glm::vec4(worldPosition, 1.0f);
        glm::vec4 clipPos = viewProjection * modelPos;
        glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;

        return 
        {
            (ndcPos.x + 1.0f) * 0.5f * screenSize.x,
            (1.0f - ndcPos.y) * 0.5f * screenSize.y,
            ndcPos.z
        };
    }

    bool Math::ProjectWorldToScreen(const glm::vec3 &worldPos, const glm::mat4 &viewProjection, const Rect &viewportRect, ImVec2 &outScreen)
    {
        const glm::vec4 clip = viewProjection * glm::vec4(worldPos, 1.0f);
        if (clip.w == 0.0f)
            return false;

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < 0.0f || ndc.z > 1.0f)
            return false;

        const glm::vec2 viewportSize = viewportRect.GetSize();
        outScreen.x = viewportRect.min.x + ((ndc.x + 1.0f) * 0.5f) * viewportSize.x;
        outScreen.y = viewportRect.min.y + ((1.0f - ndc.y) * 0.5f) * viewportSize.y;
        return true;
    }

    glm::vec2 Math::GetNormalizedDeviceCoord(const glm::vec2 &position, const glm::vec2 &screen)
    {
        float x = (2.0f * position.x) / screen.x - 1.0f;
        float y = (2.0f * position.y) / screen.y - 1.0f;
        return { x, y };
    }

    glm::vec4 Math::GetEyeCoord(glm::vec4 clipCoords, const glm::mat4 &projectionMatrix)
    {
        glm::mat4 inverseProjection = glm::inverse(projectionMatrix);
        glm::vec4 eyeCoords = inverseProjection * clipCoords;
        return { eyeCoords.x, eyeCoords.y, -1.0f, 0.0f };
    }

    glm::vec3 Math::GetWorldPosition(const glm::vec4 &eyeCoords, const glm::mat4 &viewMatrix)
    {
        glm::vec4 worldCoords = glm::inverse(viewMatrix) * eyeCoords;
        return glm::normalize(glm::vec3(worldCoords));
    }

    glm::vec3 Math::GetRayFromScreenCoords(const glm::vec2 &coord, const glm::vec2 &screen, const glm::mat4 &projection,
        const glm::mat4 &view, bool isPerspective, glm::vec3 &outRayOrigin)
    {
        glm::vec2 ndc = GetNormalizedDeviceCoord(coord, screen);
        glm::vec4 hmc = glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);

        if (isPerspective)
        {
            glm::vec4 eye = GetEyeCoord(hmc, projection);
            outRayOrigin = glm::vec3(glm::inverse(view) * glm::vec4(0, 0, 0, 1));
            return GetWorldPosition(eye, view);
        }
        else
        {
            glm::mat4 invViewProj = glm::inverse(projection * view);
            glm::vec4 worldCoords = invViewProj * hmc;
            outRayOrigin = glm::vec3(worldCoords) / worldCoords.w;
            return -glm::normalize(glm::vec3(view[2])); // Forward direction in world space
        }
    }

    glm::vec3 Math::ScreenToWorldOnPlane(const glm::vec2 &screenPos, float planeZ, const glm::mat4 &viewProjection, const Rect &viewportRect, bool *isValid)
    {
        if (isValid)
            *isValid = false;

        const glm::vec2 viewportSize = viewportRect.GetSize();
        if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
        {
            return glm::vec3(0.0f);
        }

        const float ndcX = ((screenPos.x - viewportRect.min.x) / viewportSize.x) * 2.0f - 1.0f;
        const float ndcY = 1.0f - ((screenPos.y - viewportRect.min.y) / viewportSize.y) * 2.0f;

        const glm::mat4 invViewProjection = glm::inverse(viewProjection);

        glm::vec4 nearPoint = invViewProjection * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
        glm::vec4 farPoint = invViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        if (nearPoint.w == 0.0f || farPoint.w == 0.0f)
        {
            return glm::vec3(0.0f);
        }

        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;

        const glm::vec3 origin = glm::vec3(nearPoint);
        const glm::vec3 rayDir = glm::normalize(glm::vec3(farPoint - nearPoint));
        if (glm::abs(rayDir.z) < 0.00001f)
            return glm::vec3(0.0f);

        const float t = (planeZ - origin.z) / rayDir.z;
        if (isValid)
            *isValid = true;

        return origin + rayDir * t;
    }

    bool Math::RaySphereIntersection(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, const glm::vec3 &sphereCenter, float sphereRadius)
    {
        glm::vec3 oc = rayOrigin - sphereCenter;
        float a = glm::dot(rayDirection, rayDirection);
        float b = 2.0f * glm::dot(oc, rayDirection);
        float c = glm::dot(oc, oc) - sphereRadius * sphereRadius;
        float discriminant = b * b - 4 * a * c;
        return (discriminant > 0);
    }

    bool Math::RayPlaneIntersection(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, const glm::vec3 &planeNormal, const glm::vec3 &planePoint, float &t)
    {
        float denom = glm::dot(planeNormal, rayDirection);
        if (glm::abs(denom) > 1e-5f)
        {
            glm::vec3 diff = planePoint - rayOrigin;
            t = glm::dot(diff, planeNormal) / denom;
            return t >= 0.0f;
        }
        t = 0.0f;
        return false;
    }

    bool Math::RayQuadIntersection(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2, const glm::vec3 &v3, float &t)
    {
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v3 - v0;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        if (RayPlaneIntersection(rayOrigin, rayDirection, normal, v0, t))
        {
            glm::vec3 hitPoint = rayOrigin + rayDirection * t;

            glm::vec3 n0 = glm::cross(v1 - v0, hitPoint - v0);
            glm::vec3 n1 = glm::cross(v2 - v1, hitPoint - v1);
            glm::vec3 n2 = glm::cross(v3 - v2, hitPoint - v2);
            glm::vec3 n3 = glm::cross(v0 - v3, hitPoint - v3);

            if (glm::dot(normal, n0) >= 0.0f &&
                glm::dot(normal, n1) >= 0.0f &&
                glm::dot(normal, n2) >= 0.0f &&
                glm::dot(normal, n3) >= 0.0f)
            {
                return true;
            }
        }
        return false;
    }

    glm::mat4 Math::RemoveScale(const glm::mat4 &matrix)
    {
        glm::vec3 scale
        (
            glm::length(glm::vec3(matrix[0])),
            glm::length(glm::vec3(matrix[1])),
            glm::length(glm::vec3(matrix[2]))
        );

        glm::mat4 result = matrix;
        result[0] /= scale.x;  // normalize X axis
        result[1] /= scale.y;  // normalize Y axis
        result[2] /= scale.z;  // normalize Z axis

        return result;
    }

    float Math::CascadeSplit(int32_t index, int32_t cascadeCount, float nearPlane, float farPlane, float lambda)
    {
        float linear_split = nearPlane + (farPlane - nearPlane) * (index / cascadeCount);
        float log_split = nearPlane * static_cast<float>(pow(farPlane / nearPlane, index / cascadeCount));
        return lambda * log_split + (1.0f - lambda) * linear_split;
    }

    void Math::ComputeCascadeMatrices(const glm::vec3 &cameraPos, const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 lightDirection, int32_t cascadedCount, const std::vector<float> &cascedSplits, std::vector<glm::mat4> &cascadeLightMatrices)
    {
        glm::mat4 lightViewProjection = glm::lookAt(-lightDirection * 1000.0f, glm::vec3(0.0f), glm::vec3(0, 1, 0));

        for (int32_t i = 0; i < cascadedCount; ++i)
        {
            std::array<glm::vec4, 8> frustumCorners;
            ExtractFrustumCorners(view, projection, cascedSplits[i - 1], cascedSplits[i], frustumCorners.data());

            auto minBounds = glm::vec3(FLT_MAX);
            auto maxBounds = glm::vec3(-FLT_MAX);

            for (int32_t j = 0; j < 8; ++j)
            {
                glm::vec3 corner_light_space = glm::vec3(lightViewProjection * frustumCorners[j]);
                minBounds = glm::min(minBounds, corner_light_space);
                maxBounds = glm::min(maxBounds, corner_light_space);
            }

            glm::mat4 light_projection_matrix = glm::ortho(minBounds.x, maxBounds.x, minBounds.y, maxBounds.y, minBounds.z, maxBounds.z);
            cascadeLightMatrices[i] = light_projection_matrix * lightViewProjection;
        }
    }

    void Math::ExtractFrustumCorners(const glm::mat4 &view, const glm::mat4 &projection, float nearPlane, float farPlane, glm::vec4 outCorners[8])
    {
        glm::mat4 inverseViewProjection = glm::inverse(projection * view);

        std::array<glm::vec4, 8> ndcCorners =
        {
            glm::vec4{-1, -1, 0, 1}, {1, -1, 0, 1}, {-1, 1, 0, 1}, {1, 1, 0, 1},
            glm::vec4{-1, -1, 1, 1}, {1, -1, 1, 1}, {-1, 1, 1, 1}, {1, 1, 1, 1},
        };

        for (size_t i = 0; i < ndcCorners.size(); i++)
        {
            glm::vec4 worldCorner = inverseViewProjection * ndcCorners[i];
            worldCorner /= worldCorner.w;
            outCorners[i] = worldCorner;
        }
    }

    glm::vec3 Math::SnapToGrid(glm::vec3 position, float texelSize)
    {
        return glm::floor(position / texelSize) * texelSize;
    }

}
