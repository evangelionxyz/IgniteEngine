/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "math.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

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
        // From glm::decompose in matrix_decompose.inl

        using namespace glm;
        using T = float;

        mat4 localMatrix(transform);

        // Normalize the matrix.
        if (epsilonEqual(localMatrix[3][3], static_cast<float>(0), epsilon<T>()))
            return false;

        // First, isolate perspective. This is the messiest.
        if (
            epsilonNotEqual(localMatrix[0][3], static_cast<T>(0), epsilon<T>()) ||
            epsilonNotEqual(localMatrix[1][3], static_cast<T>(0), epsilon<T>()) ||
            epsilonNotEqual(localMatrix[2][3], static_cast<T>(0), epsilon<T>()))
        {
            // Clear the perspective partition
            localMatrix[0][3] = localMatrix[1][3] = localMatrix[2][3] = static_cast<T>(0);
            localMatrix[3][3] = static_cast<T>(1);
        }

        // Next take care of translation (easy).
        translation = vec3(localMatrix[3]);
        localMatrix[3] = vec4(0, 0, 0, localMatrix[3].w);

        vec3 Row[3], Pdum3{};

        // Now get scale and shear.
        for (length_t i = 0; i < 3; ++i)
            for (length_t j = 0; j < 3; ++j)
                Row[i][j] = localMatrix[i][j];

        // Compute X scale factor and normalize first row.
        outScale.x = length(Row[0]);
        Row[0] = detail::scale(Row[0], static_cast<T>(1));
        outScale.y = length(Row[1]);
        Row[1] = detail::scale(Row[1], static_cast<T>(1));
        outScale.z = length(Row[2]);
        Row[2] = detail::scale(Row[2], static_cast<T>(1));

        // At this point, the matrix (in rows[]) is orthonormal.
        // Check for a coordinate system flip.  If the determinant
        // is -1, then negate the matrix and the scaling factors.
    #if 0
        Pdum3 = cross(Row[1], Row[2]); // v3Cross(row[1], row[2], Pdum3);
        if (dot(Row[0], Pdum3) < 0)
        {
            for (length_t i = 0; i < 3; i++)
            {
                scale[i] *= static_cast<T>(-1);
                Row[i] *= static_cast<T>(-1);
            }
        }
    #endif

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
        // From glm::decompose in matrix_decompose.inl

        using namespace glm;
        using T = float;

        mat4 localMatrix(transform);

        // Normalize the matrix.
        if (epsilonEqual(localMatrix[3][3], static_cast<float>(0), epsilon<T>()))
            return false;

        // First, isolate perspective.  This is the messiest.
        if (
            epsilonNotEqual(localMatrix[0][3], static_cast<T>(0), epsilon<T>()) ||
            epsilonNotEqual(localMatrix[1][3], static_cast<T>(0), epsilon<T>()) ||
            epsilonNotEqual(localMatrix[2][3], static_cast<T>(0), epsilon<T>()))
        {
            // Clear the perspective partition
            localMatrix[0][3] = localMatrix[1][3] = localMatrix[2][3] = static_cast<T>(0);
            localMatrix[3][3] = static_cast<T>(1);
        }

        // Next take care of translation (easy).
        outTranslation = vec3(localMatrix[3]);
        localMatrix[3] = vec4(0, 0, 0, localMatrix[3].w);

        vec3 Row[3];

        // Now get scale and shear.
        for (length_t i = 0; i < 3; ++i)
            for (length_t j = 0; j < 3; ++j)
                Row[i][j] = localMatrix[i][j];

        // Compute X scale factor and normalize first row.
        outScale.x = length(Row[0]);
        Row[0] = detail::scale(Row[0], static_cast<T>(1));
        outScale.y = length(Row[1]);
        Row[1] = detail::scale(Row[1], static_cast<T>(1));
        outScale.z = length(Row[2]);
        Row[2] = detail::scale(Row[2], static_cast<T>(1));

        // At this point, the matrix (in rows[]) is orthonormal.
        // Check for a coordinate system flip.  If the determinant
        // is -1, then negate the matrix and the scaling factors.
    #if 0
        Pdum3 = cross(Row[1], Row[2]); // v3Cross(row[1], row[2], Pdum3);
        if (dot(Row[0], Pdum3) < 0)
        {
            for (length_t i = 0; i < 3; i++)
            {
                scale[i] *= static_cast<T>(-1);
                Row[i] *= static_cast<T>(-1);
            }
        }
    #endif

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

    glm::vec3 Math::WorldToScreen(const glm::vec3 &worldPosition, const glm::mat4 &worldTransform, const glm::mat4 &viewProjection, const glm::vec2 &screenSize)
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
        float y = 1.0f - (2.0f * position.y) / screen.y;
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

    glm::vec3 Math::GetRayFromScreenCoords(const glm::vec2 &coord, const glm::vec2 &screen, const glm::mat4 &projection, const glm::mat4 &view, bool isPerspective, glm::vec3 &outRayOrigin)
    {
        glm::vec2 ndc = GetNormalizedDeviceCoord(coord, screen);
        glm::vec4 hmc = glm::vec4(ndc.x, -ndc.y, -1.0f, 1.0f);

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

    glm::mat4 Math::RemoveScale(const glm::mat4 &matrix)
    {
        glm::vec3 scale(
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

    float Math::CascadeSplit(i32 index, i32 cascade_count, f32 near_plane, f32 far_plane, f32 lambda)
    {
        f32 linear_split = near_plane + (far_plane - near_plane) * (index / cascade_count);
        f32 log_split = near_plane * static_cast<f32>(pow(far_plane / near_plane, index / cascade_count));
        return lambda * log_split + (1.0f - lambda) * linear_split;
    }

    void Math::ComputeCascadeMatrices(const glm::vec3 &cameraPos, const glm::mat4 &cameraView, const glm::mat4 &cameraProjection, const glm::vec3 lightDirection, i32 cascadedCount, const std::vector<f32> &cascedSplits, std::vector<glm::mat4> &cascadeLightMatrices)
    {
        glm::mat4 lightViewProjection = glm::lookAt(-lightDirection * 1000.0f, glm::vec3(0.0f), glm::vec3(0, 1, 0));

        for (i32 i = 0; i < cascadedCount; ++i)
        {
            std::array<glm::vec4, 8> frustumCorners;
            ExtractFrustumCorners(cameraView, cameraProjection, cascedSplits[i - 1], cascedSplits[i], frustumCorners.data());

            auto minBounds = glm::vec3(FLT_MAX);
            auto maxBounds = glm::vec3(-FLT_MAX);

            for (i32 j = 0; j < 8; ++j)
            {
                glm::vec3 corner_light_space = glm::vec3(lightViewProjection * frustumCorners[j]);
                minBounds = glm::min(minBounds, corner_light_space);
                maxBounds = glm::min(maxBounds, corner_light_space);
            }

            glm::mat4 light_projection_matrix = glm::ortho(minBounds.x, maxBounds.x, minBounds.y, maxBounds.y, minBounds.z, maxBounds.z);
            cascadeLightMatrices[i] = light_projection_matrix * lightViewProjection;
        }
    }

    void Math::ExtractFrustumCorners(const glm::mat4 &view, const glm::mat4 &projection, f32 nearPlane, f32 farPlane, glm::vec4 outCorners[8])
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

    glm::vec3 Math::SnapToGrid(glm::vec3 position, f32 texelSize)
    {
        return glm::floor(position / texelSize) * texelSize;
    }

}
