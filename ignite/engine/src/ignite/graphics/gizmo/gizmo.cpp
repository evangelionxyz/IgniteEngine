// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "gizmo.hpp"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace ignite
{
    namespace
    {
        std::atomic<int> s_ActiveGizmoManipulations{ 0 };

        constexpr float kAxisHideDot = 0.985f;
        constexpr float kAxisFadeStartDot = 0.935f;
        constexpr float kPlaneHideDot = 0.12f;
        constexpr float kMinScale = 0.001f;
        constexpr float kMinHalfExtent = 0.005f;
        constexpr int kRingHitSegments = 64;

        // Linear-space Unreal-inspired color palette
        const glm::vec4 kColorAxisX(0.92f, 0.16f, 0.20f, 1.0f);
        const glm::vec4 kColorAxisY(0.22f, 0.84f, 0.18f, 1.0f);
        const glm::vec4 kColorAxisZ(0.16f, 0.44f, 0.96f, 1.0f);
        const glm::vec4 kColorHover(1.00f, 0.84f, 0.12f, 1.0f);
        const glm::vec4 kColorScreen(0.90f, 0.90f, 0.92f, 1.0f);
        const glm::vec4 kColorBoundsFrame(0.95f, 0.72f, 0.14f, 0.96f);
        const glm::vec4 kColorBoundsCorner(0.96f, 0.94f, 0.88f, 1.0f);
        const glm::vec4 kColorBoundsEdge(0.92f, 0.76f, 0.28f, 1.0f);

        constexpr std::array<glm::ivec2, 8> kBounds2DSigns = {
            glm::ivec2(-1, -1),
            glm::ivec2( 1, -1),
            glm::ivec2( 1,  1),
            glm::ivec2(-1,  1),
            glm::ivec2( 0, -1),
            glm::ivec2( 1,  0),
            glm::ivec2( 0,  1),
            glm::ivec2(-1,  0)
        };

        constexpr std::array<glm::ivec3, 26> kBounds3DSigns = {
            // 6 face centers
            glm::ivec3(-1,  0,  0), glm::ivec3( 1,  0,  0),
            glm::ivec3( 0, -1,  0), glm::ivec3( 0,  1,  0),
            glm::ivec3( 0,  0, -1), glm::ivec3( 0,  0,  1),
            // 12 edge midpoints
            glm::ivec3( 0, -1, -1), glm::ivec3( 0,  1, -1), glm::ivec3( 0, -1,  1), glm::ivec3( 0,  1,  1),
            glm::ivec3(-1,  0, -1), glm::ivec3( 1,  0, -1), glm::ivec3(-1,  0,  1), glm::ivec3( 1,  0,  1),
            glm::ivec3(-1, -1,  0), glm::ivec3( 1, -1,  0), glm::ivec3(-1,  1,  0), glm::ivec3( 1,  1,  0),
            // 8 corners
            glm::ivec3(-1, -1, -1), glm::ivec3( 1, -1, -1), glm::ivec3( 1,  1, -1), glm::ivec3(-1,  1, -1),
            glm::ivec3(-1, -1,  1), glm::ivec3( 1, -1,  1), glm::ivec3( 1,  1,  1), glm::ivec3(-1,  1,  1)
        };

        glm::vec3 SafeNormalize(const glm::vec3 &v, const glm::vec3 &fallback)
        {
            const float lenSq = glm::dot(v, v);
            if (lenSq > 1e-10f)
                return v * glm::inversesqrt(lenSq);
            return fallback;
        }

        void BuildOrthonormalFrame(const glm::vec3 &yAxis, glm::vec3 &outX, glm::vec3 &outY, glm::vec3 &outZ)
        {
            outY = SafeNormalize(yAxis, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::vec3 ref = (std::abs(outY.y) < 0.99f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
            outX = SafeNormalize(glm::cross(ref, outY), glm::vec3(1.0f, 0.0f, 0.0f));
            outZ = SafeNormalize(glm::cross(outX, outY), glm::vec3(0.0f, 0.0f, 1.0f));
        }

        glm::mat4 MakeSegmentTransform(const glm::vec3 &p0, const glm::vec3 &p1, float radius)
        {
            const glm::vec3 delta = p1 - p0;
            const float length = glm::length(delta);
            if (length <= 1e-6f)
                return glm::translate(glm::mat4(1.0f), p0) * glm::scale(glm::mat4(1.0f), glm::vec3(radius, 1e-5f, radius));

            glm::vec3 bx, by, bz;
            BuildOrthonormalFrame(delta / length, bx, by, bz);

            glm::mat4 mat(1.0f);
            mat[0] = glm::vec4(bx * radius, 0.0f);
            mat[1] = glm::vec4(by * length, 0.0f);
            mat[2] = glm::vec4(bz * radius, 0.0f);
            mat[3] = glm::vec4(p0, 1.0f);
            return mat;
        }

        glm::mat4 MakeRingTransform(const glm::vec3 &center, const glm::vec3 &axisNormal, float radius)
        {
            glm::vec3 bx, by, bz;
            BuildOrthonormalFrame(axisNormal, bx, by, bz);

            glm::mat4 mat(1.0f);
            mat[0] = glm::vec4(bx * radius, 0.0f);
            mat[1] = glm::vec4(by * radius, 0.0f);
            mat[2] = glm::vec4(bz * radius, 0.0f);
            mat[3] = glm::vec4(center, 1.0f);
            return mat;
        }

        glm::mat4 MakeBasisTransform(const glm::vec3 &center, const std::array<glm::vec3, 3> &axes, const glm::vec3 &scale)
        {
            glm::mat4 mat(1.0f);
            mat[0] = glm::vec4(axes[0] * scale.x, 0.0f);
            mat[1] = glm::vec4(axes[1] * scale.y, 0.0f);
            mat[2] = glm::vec4(axes[2] * scale.z, 0.0f);
            mat[3] = glm::vec4(center, 1.0f);
            return mat;
        }

        float DistancePointToSegment2D(const glm::vec2 &p, const glm::vec2 &a, const glm::vec2 &b)
        {
            const glm::vec2 ab = b - a;
            const float lenSq = glm::dot(ab, ab);
            if (lenSq <= 1e-6f)
                return glm::distance(p, a);
            const float t = glm::clamp(glm::dot(p - a, ab) / lenSq, 0.0f, 1.0f);
            return glm::distance(p, a + ab * t);
        }

        bool RayPlaneIntersectAnyDir(const Ray &ray, const glm::vec3 &planePoint, const glm::vec3 &planeNormal, glm::vec3 &outHit, float *outT = nullptr)
        {
            const float denom = glm::dot(planeNormal, ray.direction);
            if (std::abs(denom) < 1e-5f)
                return false;
            const float t = glm::dot(planePoint - ray.origin, planeNormal) / denom;
            if (outT)
                *outT = t;
            outHit = ray.origin + ray.direction * t;
            return true;
        }

        bool RayAxisClosestParam(const Ray &ray, const glm::vec3 &axisOrigin, const glm::vec3 &axisDir, float &outT, float *outRayT = nullptr)
        {
            const glm::vec3 u = ray.direction;
            const glm::vec3 v = axisDir;
            const glm::vec3 w0 = ray.origin - axisOrigin;
            const float b = glm::dot(u, v);
            const float d = glm::dot(u, w0);
            const float e = glm::dot(v, w0);
            const float denom = 1.0f - b * b;
            if (denom < 1e-4f)
                return false;

            outT = (e - b * d) / denom;
            if (outRayT)
                *outRayT = b * outT - d;
            return true;
        }

        float SnapDelta(float value, float snapStep)
        {
            if (snapStep <= 1e-6f)
                return value;
            return std::round(value / snapStep) * snapStep;
        }

        void DecomposeMatrixBasis(const glm::mat4 &matrix, glm::vec3 &outTranslation, std::array<glm::vec3, 3> &outAxes, glm::vec3 &outScale)
        {
            outTranslation = glm::vec3(matrix[3]);

            glm::vec3 c0 = glm::vec3(matrix[0]);
            glm::vec3 c1 = glm::vec3(matrix[1]);
            glm::vec3 c2 = glm::vec3(matrix[2]);

            outScale.x = glm::length(c0);
            outScale.y = glm::length(c1);
            outScale.z = glm::length(c2);

            glm::vec3 ax = (outScale.x > 1e-5f) ? (c0 / outScale.x) : glm::vec3(1.0f, 0.0f, 0.0f);
            glm::vec3 ay = (outScale.y > 1e-5f) ? (c1 / outScale.y) : glm::vec3(0.0f, 1.0f, 0.0f);

            ay = ay - ax * glm::dot(ax, ay);
            ay = SafeNormalize(ay, (std::abs(ax.y) < 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f));
            ay = SafeNormalize(ay - ax * glm::dot(ax, ay), glm::vec3(0.0f, 1.0f, 0.0f));

            glm::vec3 az = SafeNormalize(glm::cross(ax, ay), glm::vec3(0.0f, 0.0f, 1.0f));
            if (glm::dot(az, c2) < 0.0f && outScale.z > 1e-5f)
            {
                // Keep orthonormal right-handed gizmo frame; preserve sign on Z scale if negative
                outScale.z = -outScale.z;
            }

            outAxes = { ax, ay, az };
        }

        bool ProjectToScreen(const glm::vec3 &worldPos, const glm::mat4 &viewProj, const Rect &viewRect, glm::vec2 &outScreen)
        {
            const glm::vec4 clip = viewProj * glm::vec4(worldPos, 1.0f);
            if (std::abs(clip.w) < 1e-6f)
                return false;
            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            const glm::vec2 size = viewRect.GetSize();
            outScreen.x = viewRect.min.x + (ndc.x + 1.0f) * 0.5f * size.x;
            outScreen.y = viewRect.min.y + (1.0f - ndc.y) * 0.5f * size.y;
            return std::isfinite(outScreen.x) && std::isfinite(outScreen.y);
        }

        void AppendDynamicQuad(std::vector<GizmoVertex> &dst,
            const glm::vec3 &p0, const glm::vec3 &p1, const glm::vec3 &p2, const glm::vec3 &p3,
            const glm::vec3 &normal, const glm::vec4 &color, float hoverFactor, uint32_t shadingMode)
        {
            const GizmoVertex v0{ p0, normal, color, hoverFactor, shadingMode };
            const GizmoVertex v1{ p1, normal, color, hoverFactor, shadingMode };
            const GizmoVertex v2{ p2, normal, color, hoverFactor, shadingMode };
            const GizmoVertex v3{ p3, normal, color, hoverFactor, shadingMode };
            dst.push_back(v0); dst.push_back(v1); dst.push_back(v2);
            dst.push_back(v0); dst.push_back(v2); dst.push_back(v3);
        }

        void AppendDynamicTriangle(std::vector<GizmoVertex> &dst,
            const glm::vec3 &p0, const glm::vec3 &p1, const glm::vec3 &p2,
            const glm::vec3 &normal, const glm::vec4 &color, float hoverFactor, uint32_t shadingMode)
        {
            dst.push_back({ p0, normal, color, hoverFactor, shadingMode });
            dst.push_back({ p1, normal, color, hoverFactor, shadingMode });
            dst.push_back({ p2, normal, color, hoverFactor, shadingMode });
        }

        glm::vec4 GetAxisColor(int axisIndex, bool hoveredOrActive)
        {
            if (hoveredOrActive)
                return kColorHover;
            switch (axisIndex)
            {
                case 0: return kColorAxisX;
                case 1: return kColorAxisY;
                default: return kColorAxisZ;
            }
        }

        glm::vec4 GetPlaneColor(int missingAxisIndex, bool hoveredOrActive)
        {
            if (hoveredOrActive)
                return kColorHover;
            // XY -> Z color, XZ -> Y color, YZ -> X color (Unreal Engine convention)
            return GetAxisColor(missingAxisIndex, false);
        }

        glm::vec3 DeriveCameraPosition(const GizmoInfo &info)
        {
            if (glm::length2(info.cameraPosition) > 1e-8f || info.hasExplicitInput)
            {
                // Still verify against inverse(cameraView) so cameraView alone always suffices
                const glm::mat4 invView = glm::inverse(info.cameraView);
                return glm::vec3(invView[3]);
            }
            const glm::mat4 invView = glm::inverse(info.cameraView);
            return glm::vec3(invView[3]);
        }

        glm::vec3 DeriveCameraForward(const GizmoInfo &info)
        {
            const glm::mat4 invView = glm::inverse(info.cameraView);
            return SafeNormalize(-glm::vec3(invView[2]), glm::vec3(0.0f, 0.0f, -1.0f));
        }

        glm::vec3 ComputeViewDirAtPivot(const GizmoInfo &info, const glm::vec3 &pivot)
        {
            if (info.cameraType == ProjectionType::Orthographic)
            {
                return -DeriveCameraForward(info);
            }
            return SafeNormalize(DeriveCameraPosition(info) - pivot, -DeriveCameraForward(info));
        }

        float SanitizeZero(float value, float epsilon)
        {
            return (std::abs(value) < epsilon) ? 0.0f : value;
        }

        std::string FormatFloat(float value, int decimals)
        {
            const float eps = (decimals <= 1) ? 0.05f : 0.005f;
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%.*f", decimals, SanitizeZero(value, eps));
            return buf;
        }

        std::string FormatSigned(float value, int decimals)
        {
            const float eps = (decimals <= 1) ? 0.05f : 0.005f;
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%+.*f", decimals, SanitizeZero(value, eps));
            return buf;
        }

        ImU32 LinearColorToImU32(const glm::vec4 &linear, float alphaScale = 1.0f)
        {
            auto toByte = [](float c) -> int
            {
                const float srgb = std::pow(glm::clamp(c, 0.0f, 1.0f), 1.0f / 2.2f);
                return static_cast<int>(std::round(srgb * 255.0f));
            };
            const int a = static_cast<int>(std::round(glm::clamp(linear.a * alphaScale, 0.0f, 1.0f) * 255.0f));
            return IM_COL32(toByte(linear.r), toByte(linear.g), toByte(linear.b), a);
        }

        void DrawDashedLine2D(ImDrawList *drawList, const ImVec2 &p0, const ImVec2 &p1, ImU32 color, float thickness, float dashLen = 6.0f, float gapLen = 4.0f)
        {
            const float dx = p1.x - p0.x;
            const float dy = p1.y - p0.y;
            const float totalLen = std::sqrt(dx * dx + dy * dy);
            if (totalLen <= 0.5f)
                return;

            const float ux = dx / totalLen;
            const float uy = dy / totalLen;
            float dist = 0.0f;
            while (dist < totalLen)
            {
                const float endDist = std::min(dist + dashLen, totalLen);
                drawList->AddLine(
                    ImVec2(p0.x + ux * dist, p0.y + uy * dist),
                    ImVec2(p0.x + ux * endDist, p0.y + uy * endDist),
                    color,
                    thickness);
                dist += dashLen + gapLen;
            }
        }
    }

    Gizmo::~Gizmo()
    {
        ResetInteraction();
    }

    void Gizmo::SetManipulatingState(bool manipulating)
    {
        if (m_IsManipulating == manipulating)
            return;

        m_IsManipulating = manipulating;
        if (manipulating)
            s_ActiveGizmoManipulations.fetch_add(1, std::memory_order_relaxed);
        else
            s_ActiveGizmoManipulations.fetch_sub(1, std::memory_order_relaxed);
    }

    bool Gizmo::IsAnyManipulating()
    {
        return s_ActiveGizmoManipulations.load(std::memory_order_relaxed) > 0;
    }

    void Gizmo::ResetInteraction()
    {
        SetManipulatingState(false);
        m_ActiveHandle = GizmoHandle::None;
        m_HoveredHandle = GizmoHandle::None;
        m_PrevMouseDown = false;
        m_DragAppliedAngle = 0.0f;
        m_DragVisualAxisScale = 1.0f;
        m_DragVisualScaleFactors = glm::vec3(1.0f);
        m_OverlayInfo.Clear();
    }

    void Gizmo::SetInfo(const GizmoInfo &info)
    {
        m_GizmoInfo = info;
    }

    void Gizmo::SetOperation(GizmoOperation op)
    {
        if (m_Operation != op)
        {
            m_Operation = op;
            ResetInteraction();
            if (op == GizmoOperation::NONE)
            {
                m_DrawList.Clear();
                m_OverlayInfo.Clear();
            }
        }
    }

    void Gizmo::SetMode(GizmoMode mode)
    {
        m_Mode = mode;
    }

    void Gizmo::ResolveInputState()
    {
        if (!m_GizmoInfo.hasExplicitInput && ImGui::GetCurrentContext() != nullptr)
        {
            const ImGuiIO &io = ImGui::GetIO();
            m_GizmoInfo.mousePos = glm::vec2(io.MousePos.x, io.MousePos.y);
            m_GizmoInfo.mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            m_GizmoInfo.mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            m_GizmoInfo.mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
            m_GizmoInfo.shiftDown = io.KeyShift;
            m_GizmoInfo.ctrlDown = io.KeyCtrl;
            m_GizmoInfo.altDown = io.KeyAlt;
            m_GizmoInfo.viewportHovered = m_GizmoInfo.viewRect.Contains(m_GizmoInfo.mousePos);
        }
        else
        {
            if (!m_GizmoInfo.mouseClicked && m_GizmoInfo.mouseDown && !m_PrevMouseDown)
                m_GizmoInfo.mouseClicked = true;
            if (!m_GizmoInfo.mouseReleased && !m_GizmoInfo.mouseDown && m_PrevMouseDown)
                m_GizmoInfo.mouseReleased = true;
        }
    }

    Ray Gizmo::ComputeMouseRay(const glm::vec2 &screenPos) const
    {
        const glm::vec2 vpSize = glm::max(m_GizmoInfo.viewRect.GetSize(), glm::vec2(1.0f));
        const float ndcX = ((screenPos.x - m_GizmoInfo.viewRect.min.x) / vpSize.x) * 2.0f - 1.0f;
        const float ndcY = 1.0f - ((screenPos.y - m_GizmoInfo.viewRect.min.y) / vpSize.y) * 2.0f;

        const glm::mat4 invVP = glm::inverse(m_GizmoInfo.cameraProjection * m_GizmoInfo.cameraView);
        glm::vec4 nearW = invVP * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
        glm::vec4 farW = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);

        if (std::abs(nearW.w) > 1e-6f) nearW /= nearW.w;
        if (std::abs(farW.w) > 1e-6f) farW /= farW.w;

        Ray ray;
        const bool isOrtho = (m_GizmoInfo.cameraType == ProjectionType::Orthographic);
        ray.origin = isOrtho ? glm::vec3(nearW) : DeriveCameraPosition(m_GizmoInfo);
        ray.direction = SafeNormalize(glm::vec3(farW - nearW), DeriveCameraForward(m_GizmoInfo));
        return ray;
    }

    float Gizmo::ComputeGizmoScale(const glm::vec3 &pivot, float *outWorldPerPixel) const
    {
        const glm::vec2 vpSize = glm::max(m_GizmoInfo.viewRect.GetSize(), glm::vec2(1.0f));
        const bool isOrtho = (m_GizmoInfo.cameraType == ProjectionType::Orthographic);
        float clipW = 1.0f;
        if (!isOrtho)
        {
            const glm::vec4 clip = m_GizmoInfo.cameraProjection * m_GizmoInfo.cameraView * glm::vec4(pivot, 1.0f);
            clipW = glm::max(clip.w, 0.05f);
        }

        const float projScaleY = glm::max(std::abs(m_GizmoInfo.cameraProjection[1][1]), 1e-4f);
        const float worldPerPixel = (2.0f * clipW) / (projScaleY * vpSize.y);
        if (outWorldPerPixel)
            *outWorldPerPixel = worldPerPixel;

        return worldPerPixel * glm::max(m_GizmoInfo.screenSizePx, 24.0f);
    }

    void Gizmo::ComputeGizmoAxes(const glm::mat4 &matrix, bool forceLocal, std::array<glm::vec3, 3> &outAxes) const
    {
        if (!forceLocal && m_Mode == GizmoMode::WORLD)
        {
            outAxes = { glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) };
            return;
        }

        glm::vec3 pos, scale;
        DecomposeMatrixBasis(matrix, pos, outAxes, scale);
    }

    void Gizmo::Select2DBoundsAxes(const std::array<glm::vec3, 3> &localAxes, const glm::vec3 &viewDir, const AABB &localBounds, int &outU, int &outV, int &outW) const
    {
        const glm::vec3 size = localBounds.GetSize();
        if (size.z < 1e-3f)
        {
            outU = 0; outV = 1; outW = 2;
            return;
        }

        float bestAbsDot = -1.0f;
        outW = 2;
        for (int i = 0; i < 3; ++i)
        {
            const float d = std::abs(glm::dot(localAxes[i], viewDir));
            if (d > bestAbsDot)
            {
                bestAbsDot = d;
                outW = i;
            }
        }

        if (outW == 2)      { outU = 0; outV = 1; }
        else if (outW == 1) { outU = 0; outV = 2; }
        else                { outU = 2; outV = 1; }
    }

    float Gizmo::ComputeAxisVisibility(const glm::vec3 &axisDir, const glm::vec3 &viewDir) const
    {
        const float d = std::abs(glm::dot(axisDir, viewDir));
        if (d >= kAxisHideDot)
            return 0.0f;
        if (d <= kAxisFadeStartDot)
            return 1.0f;
        return 1.0f - (d - kAxisFadeStartDot) / (kAxisHideDot - kAxisFadeStartDot);
    }

    GizmoHandle Gizmo::HitTest(const glm::mat4 &matrix, const AABB &localBounds, float gizmoScale, float worldPerPixel) const
    {
        const glm::vec2 vpSize = m_GizmoInfo.viewRect.GetSize();
        if (vpSize.x <= 1.0f || vpSize.y <= 1.0f || !m_GizmoInfo.viewportHovered)
            return GizmoHandle::None;

        if (m_Operation == GizmoOperation::NONE)
            return GizmoHandle::None;

        const glm::mat4 viewProj = m_GizmoInfo.cameraProjection * m_GizmoInfo.cameraView;
        const Ray ray = ComputeMouseRay(m_GizmoInfo.mousePos);

        const glm::vec3 pivot = glm::vec3(matrix[3]);
        const glm::vec3 viewDir = ComputeViewDirAtPivot(m_GizmoInfo, pivot);

        if (m_Operation == GizmoOperation::TRANSLATE)
        {
            std::array<glm::vec3, 3> axes;
            ComputeGizmoAxes(matrix, false, axes);

            // 1. Center screen-plane sphere
            glm::vec2 centerScreen;
            if (ProjectToScreen(pivot, viewProj, m_GizmoInfo.viewRect, centerScreen))
            {
                if (glm::distance(m_GizmoInfo.mousePos, centerScreen) <= 13.0f)
                {
                    return GizmoHandle::TranslateScreen;
                }
            }

            // 2. Planar quads (XY=2, XZ=1, YZ=0)
            struct PlaneInfo { int a; int b; int n; GizmoHandle handle; };
            constexpr PlaneInfo kPlanes[3] = {
                { 0, 1, 2, GizmoHandle::TranslateXY },
                { 0, 2, 1, GizmoHandle::TranslateXZ },
                { 1, 2, 0, GizmoHandle::TranslateYZ }
            };

            float bestPlaneT = std::numeric_limits<float>::max();
            GizmoHandle hitPlane = GizmoHandle::None;
            for (const auto &pl : kPlanes)
            {
                if (ComputeAxisVisibility(axes[pl.a], viewDir) <= 0.05f || ComputeAxisVisibility(axes[pl.b], viewDir) <= 0.05f)
                    continue;
                if (std::abs(glm::dot(axes[pl.n], viewDir)) < kPlaneHideDot)
                    continue;

                glm::vec3 hitPos;
                float hitT = 0.0f;
                if (RayPlaneIntersectAnyDir(ray, pivot, axes[pl.n], hitPos, &hitT))
                {
                    const float signA = (glm::dot(viewDir, axes[pl.a]) >= -0.05f) ? 1.0f : -1.0f;
                    const float signB = (glm::dot(viewDir, axes[pl.b]) >= -0.05f) ? 1.0f : -1.0f;
                    const float u = glm::dot(hitPos - pivot, axes[pl.a] * signA);
                    const float v = glm::dot(hitPos - pivot, axes[pl.b] * signB);
                    if (u >= 0.21f * gizmoScale && u <= 0.47f * gizmoScale && v >= 0.21f * gizmoScale && v <= 0.47f * gizmoScale)
                    {
                        if (hitT < bestPlaneT)
                        {
                            bestPlaneT = hitT;
                            hitPlane = pl.handle;
                        }
                    }
                }
            }
            if (hitPlane != GizmoHandle::None)
                return hitPlane;

            // 3. Axis arrows
            float bestDist = 11.0f;
            float bestRayT = std::numeric_limits<float>::max();
            GizmoHandle bestAxis = GizmoHandle::None;
            constexpr GizmoHandle kAxisHandles[3] = { GizmoHandle::TranslateX, GizmoHandle::TranslateY, GizmoHandle::TranslateZ };
            for (int i = 0; i < 3; ++i)
            {
                if (ComputeAxisVisibility(axes[i], viewDir) <= 0.05f)
                    continue;

                const glm::vec3 p0 = pivot + axes[i] * (0.12f * gizmoScale);
                const glm::vec3 p1 = pivot + axes[i] * (1.03f * gizmoScale);
                glm::vec2 s0, s1;
                if (!ProjectToScreen(p0, viewProj, m_GizmoInfo.viewRect, s0) || !ProjectToScreen(p1, viewProj, m_GizmoInfo.viewRect, s1))
                    continue;

                const float d = DistancePointToSegment2D(m_GizmoInfo.mousePos, s0, s1);
                if (d <= 11.0f)
                {
                    float axisParam = 0.0f, rayT = 0.0f;
                    RayAxisClosestParam(ray, pivot, axes[i], axisParam, &rayT);
                    if (d < bestDist - 2.0f || (std::abs(d - bestDist) <= 2.0f && rayT < bestRayT))
                    {
                        bestDist = d;
                        bestRayT = rayT;
                        bestAxis = kAxisHandles[i];
                    }
                }
            }
            return bestAxis;
        }

        if (m_Operation == GizmoOperation::ROTATE)
        {
            std::array<glm::vec3, 3> axes;
            ComputeGizmoAxes(matrix, false, axes);

            float bestDist = 10.0f;
            float bestDepth = std::numeric_limits<float>::max();
            GizmoHandle bestRing = GizmoHandle::None;

            auto testRing = [&](const glm::vec3 &axis, float radius, bool horizonCull, GizmoHandle handle)
            {
                glm::vec3 bx, by, bz;
                BuildOrthonormalFrame(axis, bx, by, bz);

                for (int s = 0; s < kRingHitSegments; ++s)
                {
                    const float a0 = (static_cast<float>(s) / static_cast<float>(kRingHitSegments)) * glm::two_pi<float>();
                    const float a1 = (static_cast<float>(s + 1) / static_cast<float>(kRingHitSegments)) * glm::two_pi<float>();
                    const glm::vec3 w0 = pivot + (bx * std::cos(a0) + bz * std::sin(a0)) * radius;
                    const glm::vec3 w1 = pivot + (bx * std::cos(a1) + bz * std::sin(a1)) * radius;
                    const glm::vec3 wMid = 0.5f * (w0 + w1);

                    if (horizonCull && glm::dot(SafeNormalize(wMid - pivot, viewDir), viewDir) < -0.10f)
                        continue;

                    glm::vec2 s0, s1;
                    if (!ProjectToScreen(w0, viewProj, m_GizmoInfo.viewRect, s0) || !ProjectToScreen(w1, viewProj, m_GizmoInfo.viewRect, s1))
                        continue;

                    const float d = DistancePointToSegment2D(m_GizmoInfo.mousePos, s0, s1);
                    if (d <= 10.0f)
                    {
                        const float depth = glm::dot(wMid - ray.origin, ray.direction);
                        if (d < bestDist - 2.0f || (std::abs(d - bestDist) <= 2.0f && depth < bestDepth))
                        {
                            bestDist = d;
                            bestDepth = depth;
                            bestRing = handle;
                        }
                    }
                }
            };

            constexpr GizmoHandle kRotHandles[3] = { GizmoHandle::RotateX, GizmoHandle::RotateY, GizmoHandle::RotateZ };
            for (int i = 0; i < 3; ++i)
            {
                const bool horizonCull = std::abs(glm::dot(axes[i], viewDir)) < 0.90f;
                testRing(axes[i], 1.0f * gizmoScale, horizonCull, kRotHandles[i]);
            }

            // Outer screen-space rotation ring
            testRing(viewDir, 1.15f * gizmoScale, false, GizmoHandle::RotateScreen);
            return bestRing;
        }

        if (m_Operation == GizmoOperation::SCALE)
        {
            std::array<glm::vec3, 3> axes;
            ComputeGizmoAxes(matrix, true, axes);

            // 1. Center uniform cube
            glm::vec2 centerScreen;
            if (ProjectToScreen(pivot, viewProj, m_GizmoInfo.viewRect, centerScreen))
            {
                if (glm::distance(m_GizmoInfo.mousePos, centerScreen) <= 13.0f)
                {
                    return GizmoHandle::ScaleXYZ;
                }
            }

            // 2. Planar scale triangles
            struct PlaneInfo { int a; int b; int n; GizmoHandle handle; };
            constexpr PlaneInfo kPlanes[3] = {
                { 0, 1, 2, GizmoHandle::ScaleXY },
                { 0, 2, 1, GizmoHandle::ScaleXZ },
                { 1, 2, 0, GizmoHandle::ScaleYZ }
            };

            for (const auto &pl : kPlanes)
            {
                if (ComputeAxisVisibility(axes[pl.a], viewDir) <= 0.05f || ComputeAxisVisibility(axes[pl.b], viewDir) <= 0.05f)
                    continue;
                if (std::abs(glm::dot(axes[pl.n], viewDir)) < kPlaneHideDot)
                    continue;

                glm::vec3 hitPos;
                if (RayPlaneIntersectAnyDir(ray, pivot, axes[pl.n], hitPos))
                {
                    const float u = glm::dot(hitPos - pivot, axes[pl.a]) / gizmoScale;
                    const float v = glm::dot(hitPos - pivot, axes[pl.b]) / gizmoScale;
                    if (u >= 0.02f && v >= 0.02f && (u + v) >= 0.30f && (u + v) <= 0.56f)
                    {
                        return pl.handle;
                    }
                }
            }

            // 3. Axis scale bars
            float bestDist = 11.0f;
            float bestRayT = std::numeric_limits<float>::max();
            GizmoHandle bestAxis = GizmoHandle::None;
            constexpr GizmoHandle kScaleHandles[3] = { GizmoHandle::ScaleX, GizmoHandle::ScaleY, GizmoHandle::ScaleZ };
            for (int i = 0; i < 3; ++i)
            {
                if (ComputeAxisVisibility(axes[i], viewDir) <= 0.05f)
                    continue;

                const glm::vec3 p0 = pivot + axes[i] * (0.14f * gizmoScale);
                const glm::vec3 p1 = pivot + axes[i] * (0.96f * gizmoScale);
                glm::vec2 s0, s1;
                if (!ProjectToScreen(p0, viewProj, m_GizmoInfo.viewRect, s0) || !ProjectToScreen(p1, viewProj, m_GizmoInfo.viewRect, s1))
                    continue;

                const float d = DistancePointToSegment2D(m_GizmoInfo.mousePos, s0, s1);
                if (d <= 11.0f)
                {
                    float axisParam = 0.0f, rayT = 0.0f;
                    RayAxisClosestParam(ray, pivot, axes[i], axisParam, &rayT);
                    if (d < bestDist - 2.0f || (std::abs(d - bestDist) <= 2.0f && rayT < bestRayT))
                    {
                        bestDist = d;
                        bestRayT = rayT;
                        bestAxis = kScaleHandles[i];
                    }
                }
            }
            return bestAxis;
        }

        if (m_Operation == GizmoOperation::BOUND_SIZING_2D)
        {
            glm::vec3 startPos, startScale;
            std::array<glm::vec3, 3> localAxes;
            DecomposeMatrixBasis(matrix, startPos, localAxes, startScale);
            const glm::vec3 absScale = glm::max(glm::abs(startScale), glm::vec3(1e-4f));
            const glm::vec3 localCenter = localBounds.GetCenter();
            const glm::vec3 localSize = localBounds.GetSize();

            int uIdx = 0, vIdx = 1, wIdx = 2;
            Select2DBoundsAxes(localAxes, viewDir, localBounds, uIdx, vIdx, wIdx);

            const float halfU = 0.5f * glm::max(localSize[uIdx], 1e-3f) * absScale[uIdx];
            const float halfV = 0.5f * glm::max(localSize[vIdx], 1e-3f) * absScale[vIdx];
            const glm::vec3 obbCenter = startPos
                + localAxes[0] * (localCenter.x * startScale.x)
                + localAxes[1] * (localCenter.y * startScale.y)
                + localAxes[2] * (localCenter.z * startScale.z);

            float bestScreenDist = 12.0f;
            GizmoHandle bestHandle = GizmoHandle::None;
            for (int i = 0; i < 8; ++i)
            {
                const glm::ivec2 s = kBounds2DSigns[i];
                const glm::vec3 hWorld = obbCenter
                    + localAxes[uIdx] * (static_cast<float>(s.x) * halfU)
                    + localAxes[vIdx] * (static_cast<float>(s.y) * halfV);
                glm::vec2 hScreen;
                if (!ProjectToScreen(hWorld, viewProj, m_GizmoInfo.viewRect, hScreen))
                    continue;
                const float d = glm::distance(m_GizmoInfo.mousePos, hScreen);
                if (d < bestScreenDist)
                {
                    bestScreenDist = d;
                    bestHandle = static_cast<GizmoHandle>(static_cast<int>(GizmoHandle::Bounds2D_0) + i);
                }
            }
            return bestHandle;
        }

        if (m_Operation == GizmoOperation::BOUND_SIZING_3D)
        {
            glm::vec3 startPos, startScale;
            std::array<glm::vec3, 3> localAxes;
            DecomposeMatrixBasis(matrix, startPos, localAxes, startScale);
            const glm::vec3 absScale = glm::max(glm::abs(startScale), glm::vec3(1e-4f));
            const glm::vec3 localCenter = localBounds.GetCenter();
            const glm::vec3 localHalf = glm::max(localBounds.GetSize() * 0.5f, glm::vec3(1e-3f));
            const glm::vec3 worldHalf = localHalf * absScale;
            const glm::vec3 obbCenter = startPos
                + localAxes[0] * (localCenter.x * startScale.x)
                + localAxes[1] * (localCenter.y * startScale.y)
                + localAxes[2] * (localCenter.z * startScale.z);

            float bestScore = 13.0f;
            float bestDepth = std::numeric_limits<float>::max();
            GizmoHandle bestHandle = GizmoHandle::None;
            (void)worldPerPixel;

            for (int i = 0; i < 26; ++i)
            {
                const glm::ivec3 s = kBounds3DSigns[i];
                const glm::vec3 hWorld = obbCenter
                    + localAxes[0] * (static_cast<float>(s.x) * worldHalf.x)
                    + localAxes[1] * (static_cast<float>(s.y) * worldHalf.y)
                    + localAxes[2] * (static_cast<float>(s.z) * worldHalf.z);

                // Skip handles facing strictly away on faces in orthographic/3D unless no front handle exists
                const glm::vec3 normalDir = SafeNormalize(
                    localAxes[0] * static_cast<float>(s.x)
                    + localAxes[1] * static_cast<float>(s.y)
                    + localAxes[2] * static_cast<float>(s.z), viewDir);
                const float facing = glm::dot(normalDir, viewDir);
                if (facing < -0.35f)
                    continue;

                glm::vec2 hScreen;
                if (!ProjectToScreen(hWorld, viewProj, m_GizmoInfo.viewRect, hScreen))
                    continue;

                const float d = glm::distance(m_GizmoInfo.mousePos, hScreen);
                const int nonZeroCount = (s.x != 0 ? 1 : 0) + (s.y != 0 ? 1 : 0) + (s.z != 0 ? 1 : 0);
                // Corners get slight priority when stacked on cardinal views; faces/edges win when clearly closer
                const float priorityBias = (nonZeroCount == 3) ? -0.5f : 0.0f;
                const float score = d + priorityBias;
                const float depth = glm::dot(hWorld - ray.origin, ray.direction);

                if (d <= 12.5f && (score < bestScore - 1.5f || (std::abs(score - bestScore) <= 1.5f && depth < bestDepth)))
                {
                    bestScore = score;
                    bestDepth = depth;
                    bestHandle = static_cast<GizmoHandle>(static_cast<int>(GizmoHandle::Bounds3D_Base) + i);
                }
            }
            return bestHandle;
        }

        return GizmoHandle::None;
    }

    void Gizmo::UpdateDrag(glm::mat4 &inOutMatrix, const AABB &localBounds, float gizmoScale, float worldPerPixel)
    {
        (void)gizmoScale;
        const Ray ray = ComputeMouseRay(m_GizmoInfo.mousePos);
        const float snap = m_GizmoInfo.isSnapping ? m_GizmoInfo.snapValue : 0.0f;

        // TRANSLATE
        if (m_Operation == GizmoOperation::TRANSLATE)
        {
            glm::vec3 newPos = m_DragStartPivot;

            if (m_ActiveHandle == GizmoHandle::TranslateX || m_ActiveHandle == GizmoHandle::TranslateY || m_ActiveHandle == GizmoHandle::TranslateZ)
            {
                const int axisIdx = static_cast<int>(m_ActiveHandle) - static_cast<int>(GizmoHandle::TranslateX);
                const glm::vec3 axis = m_DragStartAxes[axisIdx];
                float tAxis = 0.0f;
                if (RayAxisClosestParam(ray, m_DragStartPivot, axis, tAxis))
                {
                    float deltaT = tAxis - m_DragStartAxisParam;
                    if (snap > 0.0f)
                        deltaT = SnapDelta(deltaT, snap);
                    newPos = m_DragStartPivot + axis * deltaT;
                }
            }
            else if (m_ActiveHandle == GizmoHandle::TranslateXY || m_ActiveHandle == GizmoHandle::TranslateXZ || m_ActiveHandle == GizmoHandle::TranslateYZ)
            {
                int a = 0, b = 1, n = 2;
                if (m_ActiveHandle == GizmoHandle::TranslateXZ) { a = 0; b = 2; n = 1; }
                else if (m_ActiveHandle == GizmoHandle::TranslateYZ) { a = 1; b = 2; n = 0; }

                glm::vec3 hitPos;
                if (RayPlaneIntersectAnyDir(ray, m_DragStartPivot, m_DragStartAxes[n], hitPos))
                {
                    glm::vec3 delta = hitPos - m_DragStartHitPoint;
                    if (snap > 0.0f)
                    {
                        const float da = SnapDelta(glm::dot(delta, m_DragStartAxes[a]), snap);
                        const float db = SnapDelta(glm::dot(delta, m_DragStartAxes[b]), snap);
                        delta = m_DragStartAxes[a] * da + m_DragStartAxes[b] * db;
                    }
                    newPos = m_DragStartPivot + delta;
                }
            }
            else if (m_ActiveHandle == GizmoHandle::TranslateScreen)
            {
                const glm::vec3 camFwd = DeriveCameraForward(m_GizmoInfo);
                glm::vec3 hitPos;
                if (RayPlaneIntersectAnyDir(ray, m_DragStartPivot, camFwd, hitPos))
                {
                    glm::vec3 delta = hitPos - m_DragStartHitPoint;
                    if (snap > 0.0f)
                    {
                        const glm::mat4 invView = glm::inverse(m_GizmoInfo.cameraView);
                        const glm::vec3 camRight = SafeNormalize(glm::vec3(invView[0]), glm::vec3(1.0f, 0.0f, 0.0f));
                        const glm::vec3 camUp = SafeNormalize(glm::vec3(invView[1]), glm::vec3(0.0f, 1.0f, 0.0f));
                        const float dx = SnapDelta(glm::dot(delta, camRight), snap);
                        const float dy = SnapDelta(glm::dot(delta, camUp), snap);
                        delta = camRight * dx + camUp * dy;
                    }
                    newPos = m_DragStartPivot + delta;
                }
            }

            inOutMatrix = m_DragStartMatrix;
            inOutMatrix[3] = glm::vec4(newPos, 1.0f);
            return;
        }

        // ROTATE
        if (m_Operation == GizmoOperation::ROTATE)
        {
            glm::vec3 rotAxis = glm::vec3(0.0f, 0.0f, 1.0f);
            if (m_ActiveHandle == GizmoHandle::RotateX) rotAxis = m_DragStartAxes[0];
            else if (m_ActiveHandle == GizmoHandle::RotateY) rotAxis = m_DragStartAxes[1];
            else if (m_ActiveHandle == GizmoHandle::RotateZ) rotAxis = m_DragStartAxes[2];
            else if (m_ActiveHandle == GizmoHandle::RotateScreen) rotAxis = -DeriveCameraForward(m_GizmoInfo);

            const float facingDot = std::abs(glm::dot(rotAxis, ray.direction));
            if (facingDot > 0.22f || m_ActiveHandle == GizmoHandle::RotateScreen)
            {
                glm::vec3 hitPos;
                if (RayPlaneIntersectAnyDir(ray, m_DragStartPivot, rotAxis, hitPos))
                {
                    const glm::vec3 curVec = SafeNormalize(hitPos - m_DragStartPivot, m_DragPrevVec);
                    const float step = std::atan2(
                        glm::dot(rotAxis, glm::cross(m_DragPrevVec, curVec)),
                        glm::dot(m_DragPrevVec, curVec));
                    if (std::isfinite(step))
                    {
                        m_DragAccumAngle += step;
                        m_DragPrevVec = curVec;
                    }
                }
            }
            else
            {
                // Edge-on cardinal rotation: project 3D tangent onto screen space
                const glm::vec3 tangent3D = SafeNormalize(glm::cross(rotAxis, m_DragStartHitPoint - m_DragStartPivot), glm::vec3(0.0f, 1.0f, 0.0f));
                const glm::mat4 viewProj = m_GizmoInfo.cameraProjection * m_GizmoInfo.cameraView;
                glm::vec2 s0, s1;
                glm::vec2 screenTangent(0.0f, -1.0f);
                if (ProjectToScreen(m_DragStartPivot, viewProj, m_GizmoInfo.viewRect, s0)
                    && ProjectToScreen(m_DragStartPivot + tangent3D * m_DragStartGizmoScale, viewProj, m_GizmoInfo.viewRect, s1))
                {
                    const glm::vec2 diff = s1 - s0;
                    if (glm::dot(diff, diff) > 1e-4f)
                        screenTangent = glm::normalize(diff);
                }

                const glm::vec2 mouseDelta = m_GizmoInfo.mousePos - m_DragPrevMousePos;
                const float pixelRadius = glm::max(m_DragStartGizmoScale / glm::max(worldPerPixel, 1e-5f), 20.0f);
                m_DragAccumAngle += glm::dot(mouseDelta, screenTangent) / pixelRadius * 1.35f;
            }
            m_DragPrevMousePos = m_GizmoInfo.mousePos;

            float finalAngle = m_DragAccumAngle;
            if (snap > 0.0f)
            {
                finalAngle = glm::radians(SnapDelta(glm::degrees(m_DragAccumAngle), snap));
            }

            m_DragAppliedAngle = finalAngle;
            const glm::mat4 deltaRot = glm::rotate(glm::mat4(1.0f), finalAngle, rotAxis);
            const glm::mat4 toOrigin = glm::translate(glm::mat4(1.0f), -m_DragStartPivot);
            const glm::mat4 fromOrigin = glm::translate(glm::mat4(1.0f), m_DragStartPivot);
            inOutMatrix = fromOrigin * deltaRot * toOrigin * m_DragStartMatrix;
            return;
        }

        // SCALE
        if (m_Operation == GizmoOperation::SCALE)
        {
            glm::vec3 scaleFactors(1.0f);
            const float refDist = glm::max(m_DragStartGizmoScale * 0.85f, 1e-3f);

            if (m_ActiveHandle == GizmoHandle::ScaleX || m_ActiveHandle == GizmoHandle::ScaleY || m_ActiveHandle == GizmoHandle::ScaleZ)
            {
                const int axisIdx = static_cast<int>(m_ActiveHandle) - static_cast<int>(GizmoHandle::ScaleX);
                float tAxis = 0.0f;
                if (RayAxisClosestParam(ray, m_DragStartPivot, m_DragStartAxes[axisIdx], tAxis))
                {
                    float deltaFactor = (tAxis - m_DragStartAxisParam) / refDist;
                    if (snap > 0.0f)
                        deltaFactor = SnapDelta(deltaFactor, snap);
                    scaleFactors[axisIdx] = glm::max(1.0f + deltaFactor, kMinScale);
                }
            }
            else if (m_ActiveHandle == GizmoHandle::ScaleXY || m_ActiveHandle == GizmoHandle::ScaleXZ || m_ActiveHandle == GizmoHandle::ScaleYZ)
            {
                int a = 0, b = 1, n = 2;
                if (m_ActiveHandle == GizmoHandle::ScaleXZ) { a = 0; b = 2; n = 1; }
                else if (m_ActiveHandle == GizmoHandle::ScaleYZ) { a = 1; b = 2; n = 0; }

                glm::vec3 hitPos;
                if (RayPlaneIntersectAnyDir(ray, m_DragStartPivot, m_DragStartAxes[n], hitPos))
                {
                    const glm::vec3 delta = hitPos - m_DragStartHitPoint;
                    float fa = glm::dot(delta, m_DragStartAxes[a]) / refDist;
                    float fb = glm::dot(delta, m_DragStartAxes[b]) / refDist;
                    if (m_GizmoInfo.shiftDown)
                    {
                        const float avg = 0.5f * (fa + fb);
                        fa = avg;
                        fb = avg;
                    }
                    if (snap > 0.0f)
                    {
                        fa = SnapDelta(fa, snap);
                        fb = SnapDelta(fb, snap);
                    }
                    scaleFactors[a] = glm::max(1.0f + fa, kMinScale);
                    scaleFactors[b] = glm::max(1.0f + fb, kMinScale);
                }
            }
            else if (m_ActiveHandle == GizmoHandle::ScaleXYZ)
            {
                const glm::vec2 totalDelta = m_GizmoInfo.mousePos - m_DragStartMousePos;
                float uniformDelta = (totalDelta.x - totalDelta.y) / 100.0f;
                if (snap > 0.0f)
                    uniformDelta = SnapDelta(uniformDelta, snap);
                const float f = glm::max(1.0f + uniformDelta, kMinScale);
                scaleFactors = glm::vec3(f);
            }

            m_DragVisualScaleFactors = scaleFactors;
            const glm::vec3 newScale = m_DragStartScale * scaleFactors;
            inOutMatrix = MakeBasisTransform(m_DragStartTranslation, m_DragStartAxes, newScale);
            return;
        }

        // BOUND_SIZING_2D
        if (m_Operation == GizmoOperation::BOUND_SIZING_2D)
        {
            const int u = m_DragBounds2DU;
            const int v = m_DragBounds2DV;
            const int w = m_DragBounds2DW;
            const int handleIdx = static_cast<int>(m_ActiveHandle) - static_cast<int>(GizmoHandle::Bounds2D_0);
            if (handleIdx < 0 || handleIdx >= 8)
                return;

            const glm::ivec2 signUV = kBounds2DSigns[handleIdx];
            glm::vec3 dragWorld = m_DragStartHitPoint;
            if (!RayPlaneIntersectAnyDir(ray, m_DragStartOBBCenter, m_DragStartAxes[w], dragWorld))
                return;

            const glm::vec3 absStartScale = glm::max(glm::abs(m_DragStartScale), glm::vec3(1e-4f));
            const glm::vec3 localSize = m_DragStartLocalBounds.GetSize();
            const glm::vec3 localCenter = m_DragStartLocalBounds.GetCenter();

            float newHalfU = m_DragStartOBBHalfExtents[u];
            float newHalfV = m_DragStartOBBHalfExtents[v];
            glm::vec3 newObbCenter = m_DragStartOBBCenter;

            if (m_GizmoInfo.altDown)
            {
                const glm::vec3 delta = dragWorld - m_DragStartOBBCenter;
                if (signUV.x != 0)
                    newHalfU = glm::max(static_cast<float>(signUV.x) * glm::dot(delta, m_DragStartAxes[u]), kMinHalfExtent);
                if (signUV.y != 0)
                    newHalfV = glm::max(static_cast<float>(signUV.y) * glm::dot(delta, m_DragStartAxes[v]), kMinHalfExtent);

                if (m_GizmoInfo.shiftDown && signUV.x != 0 && signUV.y != 0)
                {
                    const float ratio = m_DragStartOBBHalfExtents[u] / glm::max(m_DragStartOBBHalfExtents[v], 1e-5f);
                    if ((newHalfU / m_DragStartOBBHalfExtents[u]) >= (newHalfV / m_DragStartOBBHalfExtents[v]))
                        newHalfV = glm::max(newHalfU / ratio, kMinHalfExtent);
                    else
                        newHalfU = glm::max(newHalfV * ratio, kMinHalfExtent);
                }

                if (snap > 0.0f)
                {
                    if (signUV.x != 0) newHalfU = glm::max(SnapDelta(newHalfU * 2.0f, snap) * 0.5f, kMinHalfExtent);
                    if (signUV.y != 0) newHalfV = glm::max(SnapDelta(newHalfV * 2.0f, snap) * 0.5f, kMinHalfExtent);
                }
            }
            else
            {
                const glm::vec3 delta = dragWorld - m_DragBoundsAnchorWorld;
                if (signUV.x != 0)
                    newHalfU = glm::max(static_cast<float>(signUV.x) * glm::dot(delta, m_DragStartAxes[u]) * 0.5f, kMinHalfExtent);
                if (signUV.y != 0)
                    newHalfV = glm::max(static_cast<float>(signUV.y) * glm::dot(delta, m_DragStartAxes[v]) * 0.5f, kMinHalfExtent);

                if (m_GizmoInfo.shiftDown && signUV.x != 0 && signUV.y != 0)
                {
                    const float ratio = m_DragStartOBBHalfExtents[u] / glm::max(m_DragStartOBBHalfExtents[v], 1e-5f);
                    if ((newHalfU / m_DragStartOBBHalfExtents[u]) >= (newHalfV / m_DragStartOBBHalfExtents[v]))
                        newHalfV = glm::max(newHalfU / ratio, kMinHalfExtent);
                    else
                        newHalfU = glm::max(newHalfV * ratio, kMinHalfExtent);
                }

                if (snap > 0.0f)
                {
                    if (signUV.x != 0) newHalfU = glm::max(SnapDelta(newHalfU * 2.0f, snap) * 0.5f, kMinHalfExtent);
                    if (signUV.y != 0) newHalfV = glm::max(SnapDelta(newHalfV * 2.0f, snap) * 0.5f, kMinHalfExtent);
                }

                newObbCenter = m_DragBoundsAnchorWorld
                    + m_DragStartAxes[u] * (static_cast<float>(signUV.x) * newHalfU)
                    + m_DragStartAxes[v] * (static_cast<float>(signUV.y) * newHalfV);
            }

            glm::vec3 newScale = absStartScale;
            newScale[u] = (2.0f * newHalfU) / glm::max(localSize[u], 1e-3f);
            newScale[v] = (2.0f * newHalfV) / glm::max(localSize[v], 1e-3f);
            if (m_DragStartScale.x < 0.0f) newScale.x = -newScale.x;
            if (m_DragStartScale.y < 0.0f) newScale.y = -newScale.y;
            if (m_DragStartScale.z < 0.0f) newScale.z = -newScale.z;

            const glm::vec3 newTranslation = newObbCenter
                - (m_DragStartAxes[0] * (localCenter.x * newScale.x)
                 + m_DragStartAxes[1] * (localCenter.y * newScale.y)
                 + m_DragStartAxes[2] * (localCenter.z * newScale.z));

            inOutMatrix = MakeBasisTransform(newTranslation, m_DragStartAxes, newScale);
            return;
        }

        // BOUND_SIZING_3D
        if (m_Operation == GizmoOperation::BOUND_SIZING_3D)
        {
            const glm::ivec3 s = m_DragBoundsSign;
            const int nonZeroCount = (s.x != 0 ? 1 : 0) + (s.y != 0 ? 1 : 0) + (s.z != 0 ? 1 : 0);
            if (nonZeroCount == 0)
                return;

            const glm::vec3 absStartScale = glm::max(glm::abs(m_DragStartScale), glm::vec3(1e-4f));
            const glm::vec3 localSize = glm::max(m_DragStartLocalBounds.GetSize(), glm::vec3(1e-3f));
            const glm::vec3 localCenter = m_DragStartLocalBounds.GetCenter();
            const glm::vec3 viewDir = ComputeViewDirAtPivot(m_GizmoInfo, m_DragStartOBBCenter);

            glm::vec3 newHalf = m_DragStartOBBHalfExtents;
            const glm::vec3 refPoint = m_GizmoInfo.altDown ? m_DragStartOBBCenter : m_DragBoundsAnchorWorld;
            const float halfFactor = m_GizmoInfo.altDown ? 1.0f : 0.5f;

            if (nonZeroCount == 1)
            {
                const int k = (s.x != 0) ? 0 : (s.y != 0 ? 1 : 2);
                float tAxis = 0.0f;
                if (RayAxisClosestParam(ray, refPoint, m_DragStartAxes[k], tAxis))
                {
                    newHalf[k] = glm::max(static_cast<float>(s[k]) * tAxis * halfFactor, kMinHalfExtent);
                }
            }
            else if (nonZeroCount == 2)
            {
                const int zeroAxis = (s.x == 0) ? 0 : (s.y == 0 ? 1 : 2);
                const int a = (zeroAxis + 1) % 3;
                const int b = (zeroAxis + 2) % 3;

                glm::vec3 hitPos;
                if (std::abs(glm::dot(m_DragStartAxes[zeroAxis], ray.direction)) > 0.18f
                    && RayPlaneIntersectAnyDir(ray, refPoint, m_DragStartAxes[zeroAxis], hitPos))
                {
                    const glm::vec3 delta = hitPos - refPoint;
                    newHalf[a] = glm::max(static_cast<float>(s[a]) * glm::dot(delta, m_DragStartAxes[a]) * halfFactor, kMinHalfExtent);
                    newHalf[b] = glm::max(static_cast<float>(s[b]) * glm::dot(delta, m_DragStartAxes[b]) * halfFactor, kMinHalfExtent);
                }
                else
                {
                    float ta = 0.0f, tb = 0.0f;
                    if (RayAxisClosestParam(ray, refPoint, m_DragStartAxes[a], ta))
                        newHalf[a] = glm::max(static_cast<float>(s[a]) * ta * halfFactor, kMinHalfExtent);
                    if (RayAxisClosestParam(ray, refPoint, m_DragStartAxes[b], tb))
                        newHalf[b] = glm::max(static_cast<float>(s[b]) * tb * halfFactor, kMinHalfExtent);
                }
            }
            else // 3D corner
            {
                glm::vec3 hitPos;
                if (RayPlaneIntersectAnyDir(ray, m_DragStartHitPoint, viewDir, hitPos))
                {
                    const glm::vec3 delta = hitPos - refPoint;
                    for (int k = 0; k < 3; ++k)
                    {
                        newHalf[k] = glm::max(static_cast<float>(s[k]) * glm::dot(delta, m_DragStartAxes[k]) * halfFactor, kMinHalfExtent);
                    }

                    if (m_GizmoInfo.shiftDown)
                    {
                        const glm::vec3 ratios = newHalf / glm::max(m_DragStartOBBHalfExtents, glm::vec3(1e-5f));
                        const float uniformRatio = std::max({ ratios.x, ratios.y, ratios.z });
                        newHalf = glm::max(m_DragStartOBBHalfExtents * uniformRatio, glm::vec3(kMinHalfExtent));
                    }
                }
            }

            if (snap > 0.0f)
            {
                for (int k = 0; k < 3; ++k)
                {
                    if (s[k] != 0)
                        newHalf[k] = glm::max(SnapDelta(newHalf[k] * 2.0f, snap) * 0.5f, kMinHalfExtent);
                }
            }

            const glm::vec3 newObbCenter = m_GizmoInfo.altDown
                ? m_DragStartOBBCenter
                : (m_DragBoundsAnchorWorld
                    + m_DragStartAxes[0] * (static_cast<float>(s.x) * newHalf.x)
                    + m_DragStartAxes[1] * (static_cast<float>(s.y) * newHalf.y)
                    + m_DragStartAxes[2] * (static_cast<float>(s.z) * newHalf.z));

            glm::vec3 newScale = (2.0f * newHalf) / localSize;
            for (int k = 0; k < 3; ++k)
            {
                if (s[k] == 0)
                    newScale[k] = absStartScale[k];
                if (m_DragStartScale[k] < 0.0f)
                    newScale[k] = -newScale[k];
            }

            const glm::vec3 newTranslation = newObbCenter
                - (m_DragStartAxes[0] * (localCenter.x * newScale.x)
                 + m_DragStartAxes[1] * (localCenter.y * newScale.y)
                 + m_DragStartAxes[2] * (localCenter.z * newScale.z));

            inOutMatrix = MakeBasisTransform(newTranslation, m_DragStartAxes, newScale);
        }
    }

    bool Gizmo::Manipulate(glm::mat4 &inOutMatrix, const AABB &localBounds)
    {
        if (m_Operation == GizmoOperation::NONE)
        {
            ResetInteraction();
            m_DrawList.Clear();
            return false;
        }

        ResolveInputState();

        float worldPerPixel = 0.01f;
        const glm::vec3 pivot = glm::vec3(inOutMatrix[3]);
        const float gizmoScale = ComputeGizmoScale(pivot, &worldPerPixel);

        if (!m_IsManipulating)
        {
            m_HoveredHandle = HitTest(inOutMatrix, localBounds, gizmoScale, worldPerPixel);

            if (m_GizmoInfo.mouseClicked && m_HoveredHandle != GizmoHandle::None && m_GizmoInfo.viewportHovered)
            {
                m_ActiveHandle = m_HoveredHandle;
                SetManipulatingState(true);

                m_DragStartMatrix = inOutMatrix;
                DecomposeMatrixBasis(inOutMatrix, m_DragStartTranslation, m_DragStartAxes, m_DragStartScale);
                if (m_Operation != GizmoOperation::SCALE
                    && m_Operation != GizmoOperation::BOUND_SIZING_2D
                    && m_Operation != GizmoOperation::BOUND_SIZING_3D)
                {
                    ComputeGizmoAxes(inOutMatrix, false, m_DragStartAxes);
                }

                m_DragStartPivot = pivot;
                m_DragStartMousePos = m_GizmoInfo.mousePos;
                m_DragPrevMousePos = m_GizmoInfo.mousePos;
                m_DragStartGizmoScale = gizmoScale;
                m_DragStartWorldPerPixel = worldPerPixel;
                m_DragAccumAngle = 0.0f;
                m_DragAppliedAngle = 0.0f;
                m_DragVisualScaleFactors = glm::vec3(1.0f);
                m_DragStartLocalBounds = localBounds;

                const Ray ray = ComputeMouseRay(m_GizmoInfo.mousePos);
                const glm::vec3 viewDir = ComputeViewDirAtPivot(m_GizmoInfo, pivot);

                if (m_Operation == GizmoOperation::TRANSLATE)
                {
                    if (m_ActiveHandle == GizmoHandle::TranslateX || m_ActiveHandle == GizmoHandle::TranslateY || m_ActiveHandle == GizmoHandle::TranslateZ)
                    {
                        const int idx = static_cast<int>(m_ActiveHandle) - static_cast<int>(GizmoHandle::TranslateX);
                        RayAxisClosestParam(ray, pivot, m_DragStartAxes[idx], m_DragStartAxisParam);
                    }
                    else if (m_ActiveHandle == GizmoHandle::TranslateXY || m_ActiveHandle == GizmoHandle::TranslateXZ || m_ActiveHandle == GizmoHandle::TranslateYZ)
                    {
                        int n = 2;
                        if (m_ActiveHandle == GizmoHandle::TranslateXZ) n = 1;
                        else if (m_ActiveHandle == GizmoHandle::TranslateYZ) n = 0;
                        RayPlaneIntersectAnyDir(ray, pivot, m_DragStartAxes[n], m_DragStartHitPoint);
                    }
                    else if (m_ActiveHandle == GizmoHandle::TranslateScreen)
                    {
                        RayPlaneIntersectAnyDir(ray, pivot, DeriveCameraForward(m_GizmoInfo), m_DragStartHitPoint);
                    }
                }
                else if (m_Operation == GizmoOperation::ROTATE)
                {
                    glm::vec3 rotAxis = viewDir;
                    if (m_ActiveHandle == GizmoHandle::RotateX) rotAxis = m_DragStartAxes[0];
                    else if (m_ActiveHandle == GizmoHandle::RotateY) rotAxis = m_DragStartAxes[1];
                    else if (m_ActiveHandle == GizmoHandle::RotateZ) rotAxis = m_DragStartAxes[2];
                    else if (m_ActiveHandle == GizmoHandle::RotateScreen) rotAxis = -DeriveCameraForward(m_GizmoInfo);

                    if (!RayPlaneIntersectAnyDir(ray, pivot, rotAxis, m_DragStartHitPoint))
                    {
                        glm::vec3 bx, by, bz;
                        BuildOrthonormalFrame(rotAxis, bx, by, bz);
                        m_DragStartHitPoint = pivot + bx * gizmoScale;
                    }
                    m_DragStartVec = SafeNormalize(m_DragStartHitPoint - pivot, glm::vec3(1.0f, 0.0f, 0.0f));
                    m_DragPrevVec = m_DragStartVec;
                }
                else if (m_Operation == GizmoOperation::SCALE)
                {
                    if (m_ActiveHandle == GizmoHandle::ScaleX || m_ActiveHandle == GizmoHandle::ScaleY || m_ActiveHandle == GizmoHandle::ScaleZ)
                    {
                        const int idx = static_cast<int>(m_ActiveHandle) - static_cast<int>(GizmoHandle::ScaleX);
                        RayAxisClosestParam(ray, pivot, m_DragStartAxes[idx], m_DragStartAxisParam);
                    }
                    else if (m_ActiveHandle == GizmoHandle::ScaleXY || m_ActiveHandle == GizmoHandle::ScaleXZ || m_ActiveHandle == GizmoHandle::ScaleYZ)
                    {
                        int n = 2;
                        if (m_ActiveHandle == GizmoHandle::ScaleXZ) n = 1;
                        else if (m_ActiveHandle == GizmoHandle::ScaleYZ) n = 0;
                        RayPlaneIntersectAnyDir(ray, pivot, m_DragStartAxes[n], m_DragStartHitPoint);
                    }
                }
                else if (m_Operation == GizmoOperation::BOUND_SIZING_2D)
                {
                    const glm::vec3 absScale = glm::max(glm::abs(m_DragStartScale), glm::vec3(1e-4f));
                    const glm::vec3 localCenter = localBounds.GetCenter();
                    const glm::vec3 localSize = glm::max(localBounds.GetSize(), glm::vec3(1e-3f));
                    m_DragStartOBBHalfExtents = localSize * 0.5f * absScale;
                    m_DragStartOBBCenter = m_DragStartTranslation
                        + m_DragStartAxes[0] * (localCenter.x * m_DragStartScale.x)
                        + m_DragStartAxes[1] * (localCenter.y * m_DragStartScale.y)
                        + m_DragStartAxes[2] * (localCenter.z * m_DragStartScale.z);

                    Select2DBoundsAxes(m_DragStartAxes, viewDir, localBounds, m_DragBounds2DU, m_DragBounds2DV, m_DragBounds2DW);

                    const int handleIdx = static_cast<int>(m_ActiveHandle) - static_cast<int>(GizmoHandle::Bounds2D_0);
                    const glm::ivec2 s = (handleIdx >= 0 && handleIdx < 8) ? kBounds2DSigns[handleIdx] : glm::ivec2(1, 1);
                    m_DragBoundsAnchorWorld = m_DragStartOBBCenter
                        - m_DragStartAxes[m_DragBounds2DU] * (static_cast<float>(s.x) * m_DragStartOBBHalfExtents[m_DragBounds2DU])
                        - m_DragStartAxes[m_DragBounds2DV] * (static_cast<float>(s.y) * m_DragStartOBBHalfExtents[m_DragBounds2DV]);
                    m_DragStartHitPoint = m_DragStartOBBCenter
                        + m_DragStartAxes[m_DragBounds2DU] * (static_cast<float>(s.x) * m_DragStartOBBHalfExtents[m_DragBounds2DU])
                        + m_DragStartAxes[m_DragBounds2DV] * (static_cast<float>(s.y) * m_DragStartOBBHalfExtents[m_DragBounds2DV]);
                }
                else if (m_Operation == GizmoOperation::BOUND_SIZING_3D)
                {
                    const glm::vec3 absScale = glm::max(glm::abs(m_DragStartScale), glm::vec3(1e-4f));
                    const glm::vec3 localCenter = localBounds.GetCenter();
                    const glm::vec3 localSize = glm::max(localBounds.GetSize(), glm::vec3(1e-3f));
                    m_DragStartOBBHalfExtents = localSize * 0.5f * absScale;
                    m_DragStartOBBCenter = m_DragStartTranslation
                        + m_DragStartAxes[0] * (localCenter.x * m_DragStartScale.x)
                        + m_DragStartAxes[1] * (localCenter.y * m_DragStartScale.y)
                        + m_DragStartAxes[2] * (localCenter.z * m_DragStartScale.z);

                    const int handleIdx = static_cast<int>(m_ActiveHandle) - static_cast<int>(GizmoHandle::Bounds3D_Base);
                    m_DragBoundsSign = (handleIdx >= 0 && handleIdx < 26) ? kBounds3DSigns[handleIdx] : glm::ivec3(1);
                    m_DragBoundsAnchorWorld = m_DragStartOBBCenter
                        - m_DragStartAxes[0] * (static_cast<float>(m_DragBoundsSign.x) * m_DragStartOBBHalfExtents.x)
                        - m_DragStartAxes[1] * (static_cast<float>(m_DragBoundsSign.y) * m_DragStartOBBHalfExtents.y)
                        - m_DragStartAxes[2] * (static_cast<float>(m_DragBoundsSign.z) * m_DragStartOBBHalfExtents.z);
                    m_DragStartHitPoint = m_DragStartOBBCenter
                        + m_DragStartAxes[0] * (static_cast<float>(m_DragBoundsSign.x) * m_DragStartOBBHalfExtents.x)
                        + m_DragStartAxes[1] * (static_cast<float>(m_DragBoundsSign.y) * m_DragStartOBBHalfExtents.y)
                        + m_DragStartAxes[2] * (static_cast<float>(m_DragBoundsSign.z) * m_DragStartOBBHalfExtents.z);
                }
            }
        }
        else
        {
            if (m_GizmoInfo.mouseDown && !m_GizmoInfo.mouseReleased)
            {
                UpdateDrag(inOutMatrix, localBounds, gizmoScale, worldPerPixel);
            }
            else
            {
                SetManipulatingState(false);
                m_ActiveHandle = GizmoHandle::None;
                m_DragVisualScaleFactors = glm::vec3(1.0f);
                m_HoveredHandle = HitTest(inOutMatrix, localBounds, gizmoScale, worldPerPixel);
            }
        }

        m_PrevMouseDown = m_GizmoInfo.mouseDown;
        BuildDrawList(inOutMatrix, localBounds);
        UpdateOverlayInfo(inOutMatrix, localBounds, gizmoScale, worldPerPixel);
        if (m_OverlayInfo.active && !m_GizmoInfo.hasExplicitInput && ImGui::GetCurrentContext() != nullptr)
        {
            DrawOverlay(nullptr);
        }
        return m_IsManipulating;
    }

    void Gizmo::BuildDrawList(const glm::mat4 &matrix, const AABB &localBounds)
    {
        m_DrawList.Clear();
        if (m_Operation == GizmoOperation::NONE)
            return;

        const bool isOrtho = (m_GizmoInfo.cameraType == ProjectionType::Orthographic);
        m_DrawList.depthBias = isOrtho ? 0.00055f : 0.00022f;

        float worldPerPixel = 0.01f;
        const glm::vec3 pivot = glm::vec3(matrix[3]);
        const float gizmoScale = ComputeGizmoScale(pivot, &worldPerPixel);
        const glm::vec3 viewDir = ComputeViewDirAtPivot(m_GizmoInfo, pivot);

        std::array<glm::vec3, 3> axes;
        const bool forceLocal = (m_Operation == GizmoOperation::SCALE
            || m_Operation == GizmoOperation::BOUND_SIZING_2D
            || m_Operation == GizmoOperation::BOUND_SIZING_3D);
        ComputeGizmoAxes(matrix, forceLocal, axes);

        switch (m_Operation)
        {
            case GizmoOperation::TRANSLATE:
                BuildTranslateDrawList(pivot, axes, viewDir, gizmoScale);
                break;
            case GizmoOperation::ROTATE:
                BuildRotateDrawList(pivot, axes, viewDir, gizmoScale);
                break;
            case GizmoOperation::SCALE:
                BuildScaleDrawList(pivot, axes, viewDir, gizmoScale);
                break;
            case GizmoOperation::BOUND_SIZING_2D:
                BuildBounds2DDrawList(matrix, localBounds, viewDir, gizmoScale, worldPerPixel);
                break;
            case GizmoOperation::BOUND_SIZING_3D:
                BuildBounds3DDrawList(matrix, localBounds, viewDir, gizmoScale, worldPerPixel);
                break;
            default:
                break;
        }
    }

    void Gizmo::BuildTranslateDrawList(const glm::vec3 &pivot, const std::array<glm::vec3, 3> &axes, const glm::vec3 &viewDir, float gizmoScale)
    {
        const GizmoHandle active = m_IsManipulating ? m_ActiveHandle : m_HoveredHandle;

        // 1. Center screen-plane sphere
        {
            const bool hi = (active == GizmoHandle::TranslateScreen);
            GizmoPrimitiveInstance inst;
            inst.meshType = GizmoMeshType::Sphere;
            inst.worldMatrix = glm::translate(glm::mat4(1.0f), pivot) * glm::scale(glm::mat4(1.0f), glm::vec3(0.088f * gizmoScale));
            inst.color = hi ? kColorHover : kColorScreen;
            inst.hoverFactor = hi ? 1.0f : 0.0f;
            inst.shadingMode = 0;
            m_DrawList.opaqueInstances.push_back(inst);
        }

        // 2. X, Y, Z shafts and cones
        constexpr GizmoHandle kAxisHandles[3] = { GizmoHandle::TranslateX, GizmoHandle::TranslateY, GizmoHandle::TranslateZ };
        const float shaftStart = 0.11f * gizmoScale;
        const float shaftEnd = 0.80f * gizmoScale;
        const float coneEnd = 1.02f * gizmoScale;
        const float shaftRadius = 0.022f * gizmoScale;
        const float coneRadius = 0.068f * gizmoScale;

        for (int i = 0; i < 3; ++i)
        {
            const float vis = ComputeAxisVisibility(axes[i], viewDir);
            if (vis <= 0.01f)
                continue;

            const bool hi = (active == kAxisHandles[i]);
            glm::vec4 col = GetAxisColor(i, hi);
            col.a *= vis;

            GizmoPrimitiveInstance shaft;
            shaft.meshType = GizmoMeshType::Cylinder;
            shaft.worldMatrix = MakeSegmentTransform(pivot + axes[i] * shaftStart, pivot + axes[i] * shaftEnd, shaftRadius);
            shaft.color = col;
            shaft.hoverFactor = hi ? 1.0f : 0.0f;
            shaft.shadingMode = 0;
            shaft.translucent = vis < 0.99f;
            if (shaft.translucent) m_DrawList.translucentInstances.push_back(shaft);
            else m_DrawList.opaqueInstances.push_back(shaft);

            GizmoPrimitiveInstance cone;
            cone.meshType = GizmoMeshType::Cone;
            cone.worldMatrix = MakeSegmentTransform(pivot + axes[i] * shaftEnd, pivot + axes[i] * coneEnd, coneRadius);
            cone.color = col;
            cone.hoverFactor = hi ? 1.0f : 0.0f;
            cone.shadingMode = 0;
            cone.translucent = vis < 0.99f;
            if (cone.translucent) m_DrawList.translucentInstances.push_back(cone);
            else m_DrawList.opaqueInstances.push_back(cone);
        }

        // 3. Planar quads (XY, XZ, YZ) with Unreal-style L-brackets
        struct PlaneDef { int a; int b; int n; GizmoHandle handle; };
        constexpr PlaneDef kPlanes[3] = {
            { 0, 1, 2, GizmoHandle::TranslateXY },
            { 0, 2, 1, GizmoHandle::TranslateXZ },
            { 1, 2, 0, GizmoHandle::TranslateYZ }
        };

        const float minOff = 0.24f * gizmoScale;
        const float maxOff = 0.44f * gizmoScale;
        const float borderRadius = 0.013f * gizmoScale;

        for (const auto &pl : kPlanes)
        {
            const float visA = ComputeAxisVisibility(axes[pl.a], viewDir);
            const float visB = ComputeAxisVisibility(axes[pl.b], viewDir);
            if (visA <= 0.05f || visB <= 0.05f)
                continue;
            if (std::abs(glm::dot(axes[pl.n], viewDir)) < kPlaneHideDot)
                continue;

            const bool hi = (active == pl.handle);
            const float signA = (glm::dot(viewDir, axes[pl.a]) >= -0.05f) ? 1.0f : -1.0f;
            const float signB = (glm::dot(viewDir, axes[pl.b]) >= -0.05f) ? 1.0f : -1.0f;
            const glm::vec3 dA = axes[pl.a] * signA;
            const glm::vec3 dB = axes[pl.b] * signB;

            const glm::vec3 p00 = pivot + dA * minOff + dB * minOff;
            const glm::vec3 p10 = pivot + dA * maxOff + dB * minOff;
            const glm::vec3 p11 = pivot + dA * maxOff + dB * maxOff;
            const glm::vec3 p01 = pivot + dA * minOff + dB * maxOff;

            glm::vec4 fillCol = GetPlaneColor(pl.n, hi);
            fillCol.a = (hi ? 0.48f : 0.24f) * std::min(visA, visB);
            AppendDynamicQuad(m_DrawList.dynamicTranslucentTriangles, p00, p10, p11, p01, axes[pl.n], fillCol, hi ? 1.0f : 0.0f, 1);

            // Two crisp outer L-bracket edges matching their perpendicular axis colors
            GizmoPrimitiveInstance edgeA;
            edgeA.meshType = GizmoMeshType::Cylinder;
            edgeA.worldMatrix = MakeSegmentTransform(p10, p11, borderRadius);
            edgeA.color = hi ? kColorHover : GetAxisColor(pl.a, false);
            edgeA.hoverFactor = hi ? 1.0f : 0.0f;
            m_DrawList.opaqueInstances.push_back(edgeA);

            GizmoPrimitiveInstance edgeB;
            edgeB.meshType = GizmoMeshType::Cylinder;
            edgeB.worldMatrix = MakeSegmentTransform(p01, p11, borderRadius);
            edgeB.color = hi ? kColorHover : GetAxisColor(pl.b, false);
            edgeB.hoverFactor = hi ? 1.0f : 0.0f;
            m_DrawList.opaqueInstances.push_back(edgeB);
        }
    }

    void Gizmo::BuildRotateDrawList(const glm::vec3 &pivot, const std::array<glm::vec3, 3> &axes, const glm::vec3 &viewDir, float gizmoScale)
    {
        const GizmoHandle active = m_IsManipulating ? m_ActiveHandle : m_HoveredHandle;

        // 1. X, Y, Z torus rings
        constexpr GizmoHandle kRotHandles[3] = { GizmoHandle::RotateX, GizmoHandle::RotateY, GizmoHandle::RotateZ };
        for (int i = 0; i < 3; ++i)
        {
            const bool hi = (active == kRotHandles[i]);
            const bool horizonFade = std::abs(glm::dot(axes[i], viewDir)) < 0.90f;

            GizmoPrimitiveInstance ring;
            ring.meshType = GizmoMeshType::Torus;
            ring.worldMatrix = MakeRingTransform(pivot, axes[i], 1.0f * gizmoScale);
            ring.color = GetAxisColor(i, hi);
            ring.ringCenterAndFlag = glm::vec4(pivot, horizonFade ? 1.0f : 0.0f);
            ring.hoverFactor = hi ? 1.0f : 0.0f;
            ring.shadingMode = 2; // RingLit
            m_DrawList.opaqueInstances.push_back(ring);
        }

        // 2. Outer screen-space rotation ring
        {
            const bool hi = (active == GizmoHandle::RotateScreen);
            GizmoPrimitiveInstance screenRing;
            screenRing.meshType = GizmoMeshType::Torus;
            screenRing.worldMatrix = MakeRingTransform(pivot, viewDir, 1.15f * gizmoScale);
            screenRing.color = hi ? kColorHover : glm::vec4(0.84f, 0.86f, 0.90f, 0.90f);
            screenRing.ringCenterAndFlag = glm::vec4(pivot, 0.0f);
            screenRing.hoverFactor = hi ? 1.0f : 0.0f;
            screenRing.shadingMode = 2;
            screenRing.translucent = true;
            m_DrawList.translucentInstances.push_back(screenRing);
        }

        // 3. Active swept-angle pie wedge & radial ticks while dragging
        if (m_IsManipulating)
        {
            glm::vec3 rotAxis = viewDir;
            int axisIdx = -1;
            if (m_ActiveHandle == GizmoHandle::RotateX)      { rotAxis = m_DragStartAxes[0]; axisIdx = 0; }
            else if (m_ActiveHandle == GizmoHandle::RotateY) { rotAxis = m_DragStartAxes[1]; axisIdx = 1; }
            else if (m_ActiveHandle == GizmoHandle::RotateZ) { rotAxis = m_DragStartAxes[2]; axisIdx = 2; }
            else if (m_ActiveHandle == GizmoHandle::RotateScreen) { rotAxis = -DeriveCameraForward(m_GizmoInfo); }

            const float radius = (m_ActiveHandle == GizmoHandle::RotateScreen ? 1.15f : 1.0f) * gizmoScale;
            const float snap = m_GizmoInfo.isSnapping ? m_GizmoInfo.snapValue : 0.0f;
            const float angle = (snap > 0.0f)
                ? glm::radians(SnapDelta(glm::degrees(m_DragAccumAngle), snap))
                : m_DragAccumAngle;

            const glm::vec3 startDir = SafeNormalize(m_DragStartVec - rotAxis * glm::dot(m_DragStartVec, rotAxis), glm::vec3(1.0f, 0.0f, 0.0f));
            const glm::vec3 orthoDir = SafeNormalize(glm::cross(rotAxis, startDir), glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::vec3 endDir = startDir * std::cos(angle) + orthoDir * std::sin(angle);

            glm::vec4 wedgeCol = (axisIdx >= 0) ? GetAxisColor(axisIdx, false) : kColorHover;
            wedgeCol.a = 0.28f;

            const float clampedSweep = glm::clamp(angle, -glm::two_pi<float>(), glm::two_pi<float>());
            const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(clampedSweep) / glm::radians(6.0f))));
            for (int s = 0; s < steps; ++s)
            {
                const float a0 = clampedSweep * (static_cast<float>(s) / static_cast<float>(steps));
                const float a1 = clampedSweep * (static_cast<float>(s + 1) / static_cast<float>(steps));
                const glm::vec3 p0 = pivot + (startDir * std::cos(a0) + orthoDir * std::sin(a0)) * radius;
                const glm::vec3 p1 = pivot + (startDir * std::cos(a1) + orthoDir * std::sin(a1)) * radius;
                AppendDynamicTriangle(m_DrawList.dynamicTranslucentTriangles, pivot, p0, p1, rotAxis, wedgeCol, 1.0f, 1);
            }

            // Start & current radial tick cylinders
            const float tickRadius = 0.014f * gizmoScale;
            GizmoPrimitiveInstance tickStart;
            tickStart.meshType = GizmoMeshType::Cylinder;
            tickStart.worldMatrix = MakeSegmentTransform(pivot, pivot + startDir * radius, tickRadius);
            tickStart.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.92f);
            tickStart.hoverFactor = 0.5f;
            m_DrawList.opaqueInstances.push_back(tickStart);

            GizmoPrimitiveInstance tickEnd;
            tickEnd.meshType = GizmoMeshType::Cylinder;
            tickEnd.worldMatrix = MakeSegmentTransform(pivot, pivot + endDir * radius, tickRadius);
            tickEnd.color = kColorHover;
            tickEnd.hoverFactor = 1.0f;
            m_DrawList.opaqueInstances.push_back(tickEnd);
        }
    }

    void Gizmo::BuildScaleDrawList(const glm::vec3 &pivot, const std::array<glm::vec3, 3> &axes, const glm::vec3 &viewDir, float gizmoScale)
    {
        const GizmoHandle active = m_IsManipulating ? m_ActiveHandle : m_HoveredHandle;

        // 1. Center uniform cube
        {
            const bool hi = (active == GizmoHandle::ScaleXYZ);
            GizmoPrimitiveInstance centerCube;
            centerCube.meshType = GizmoMeshType::Cube;
            centerCube.worldMatrix = MakeBasisTransform(pivot, axes, glm::vec3(0.145f * gizmoScale));
            centerCube.color = hi ? kColorHover : kColorScreen;
            centerCube.hoverFactor = hi ? 1.0f : 0.0f;
            m_DrawList.opaqueInstances.push_back(centerCube);
        }

        // 2. X, Y, Z bars and end cubes
        constexpr GizmoHandle kScaleHandles[3] = { GizmoHandle::ScaleX, GizmoHandle::ScaleY, GizmoHandle::ScaleZ };
        const float shaftStart = 0.12f * gizmoScale;
        const float shaftRadius = 0.022f * gizmoScale;
        const float cubeSize = 0.125f * gizmoScale;

        for (int i = 0; i < 3; ++i)
        {
            const float vis = ComputeAxisVisibility(axes[i], viewDir);
            if (vis <= 0.01f)
                continue;

            const bool hi = (active == kScaleHandles[i]);
            const float visualExtent = m_IsManipulating ? glm::clamp(m_DragVisualScaleFactors[i], 0.35f, 2.5f) : 1.0f;
            const float shaftEnd = 0.88f * gizmoScale * visualExtent;

            glm::vec4 col = GetAxisColor(i, hi);
            col.a *= vis;

            GizmoPrimitiveInstance shaft;
            shaft.meshType = GizmoMeshType::Cylinder;
            shaft.worldMatrix = MakeSegmentTransform(pivot + axes[i] * shaftStart, pivot + axes[i] * shaftEnd, shaftRadius);
            shaft.color = col;
            shaft.hoverFactor = hi ? 1.0f : 0.0f;
            shaft.translucent = vis < 0.99f;
            if (shaft.translucent) m_DrawList.translucentInstances.push_back(shaft);
            else m_DrawList.opaqueInstances.push_back(shaft);

            GizmoPrimitiveInstance cap;
            cap.meshType = GizmoMeshType::Cube;
            cap.worldMatrix = MakeBasisTransform(pivot + axes[i] * shaftEnd, axes, glm::vec3(cubeSize));
            cap.color = col;
            cap.hoverFactor = hi ? 1.0f : 0.0f;
            cap.translucent = vis < 0.99f;
            if (cap.translucent) m_DrawList.translucentInstances.push_back(cap);
            else m_DrawList.opaqueInstances.push_back(cap);
        }

        // 3. Planar scale bridges (XY, XZ, YZ)
        struct PlaneDef { int a; int b; int n; GizmoHandle handle; };
        constexpr PlaneDef kPlanes[3] = {
            { 0, 1, 2, GizmoHandle::ScaleXY },
            { 0, 2, 1, GizmoHandle::ScaleXZ },
            { 1, 2, 0, GizmoHandle::ScaleYZ }
        };

        const float innerOff = 0.30f * gizmoScale;
        const float outerOff = 0.48f * gizmoScale;
        const float barRadius = 0.013f * gizmoScale;

        for (const auto &pl : kPlanes)
        {
            const float visA = ComputeAxisVisibility(axes[pl.a], viewDir);
            const float visB = ComputeAxisVisibility(axes[pl.b], viewDir);
            if (visA <= 0.05f || visB <= 0.05f)
                continue;
            if (std::abs(glm::dot(axes[pl.n], viewDir)) < kPlaneHideDot)
                continue;

            const bool hi = (active == pl.handle);
            const glm::vec3 a0 = pivot + axes[pl.a] * innerOff;
            const glm::vec3 a1 = pivot + axes[pl.a] * outerOff;
            const glm::vec3 b0 = pivot + axes[pl.b] * innerOff;
            const glm::vec3 b1 = pivot + axes[pl.b] * outerOff;

            glm::vec4 fillCol = GetPlaneColor(pl.n, hi);
            fillCol.a = (hi ? 0.50f : 0.25f) * std::min(visA, visB);
            AppendDynamicQuad(m_DrawList.dynamicTranslucentTriangles, a0, a1, b1, b0, axes[pl.n], fillCol, hi ? 1.0f : 0.0f, 1);

            GizmoPrimitiveInstance barOuter;
            barOuter.meshType = GizmoMeshType::Cylinder;
            barOuter.worldMatrix = MakeSegmentTransform(a1, b1, barRadius);
            barOuter.color = hi ? kColorHover : GetPlaneColor(pl.n, false);
            barOuter.hoverFactor = hi ? 1.0f : 0.0f;
            m_DrawList.opaqueInstances.push_back(barOuter);

            GizmoPrimitiveInstance barInner;
            barInner.meshType = GizmoMeshType::Cylinder;
            barInner.worldMatrix = MakeSegmentTransform(a0, b0, barRadius * 0.85f);
            barInner.color = hi ? kColorHover : GetPlaneColor(pl.n, false);
            barInner.hoverFactor = hi ? 1.0f : 0.0f;
            m_DrawList.opaqueInstances.push_back(barInner);
        }
    }

    void Gizmo::BuildBounds2DDrawList(const glm::mat4 &matrix, const AABB &localBounds, const glm::vec3 &viewDir, float gizmoScale, float worldPerPixel)
    {
        (void)gizmoScale;
        glm::vec3 pos, scale;
        std::array<glm::vec3, 3> localAxes;
        DecomposeMatrixBasis(matrix, pos, localAxes, scale);

        const glm::vec3 absScale = glm::max(glm::abs(scale), glm::vec3(1e-4f));
        const glm::vec3 localCenter = localBounds.GetCenter();
        const glm::vec3 localSize = glm::max(localBounds.GetSize(), glm::vec3(1e-3f));

        int u = 0, v = 1, w = 2;
        Select2DBoundsAxes(localAxes, viewDir, localBounds, u, v, w);

        const float halfU = 0.5f * localSize[u] * absScale[u];
        const float halfV = 0.5f * localSize[v] * absScale[v];
        const glm::vec3 obbCenter = pos
            + localAxes[0] * (localCenter.x * scale.x)
            + localAxes[1] * (localCenter.y * scale.y)
            + localAxes[2] * (localCenter.z * scale.z);

        const glm::vec3 c0 = obbCenter - localAxes[u] * halfU - localAxes[v] * halfV;
        const glm::vec3 c1 = obbCenter + localAxes[u] * halfU - localAxes[v] * halfV;
        const glm::vec3 c2 = obbCenter + localAxes[u] * halfU + localAxes[v] * halfV;
        const glm::vec3 c3 = obbCenter - localAxes[u] * halfU + localAxes[v] * halfV;

        // Subtle fill quad over bounds interior
        {
            glm::vec4 interiorCol = kColorBoundsFrame;
            interiorCol.a = m_IsManipulating ? 0.12f : 0.06f;
            AppendDynamicQuad(m_DrawList.dynamicTranslucentTriangles, c0, c1, c2, c3, localAxes[w], interiorCol, 0.0f, 1);
        }

        // 4 crisp 3D cylinder border struts
        const float strutRadius = worldPerPixel * 1.85f;
        const glm::vec3 corners[4] = { c0, c1, c2, c3 };
        for (int i = 0; i < 4; ++i)
        {
            GizmoPrimitiveInstance strut;
            strut.meshType = GizmoMeshType::Cylinder;
            strut.worldMatrix = MakeSegmentTransform(corners[i], corners[(i + 1) % 4], strutRadius);
            strut.color = kColorBoundsFrame;
            strut.hoverFactor = 0.0f;
            m_DrawList.opaqueInstances.push_back(strut);
        }

        // 8 handles (4 corners + 4 edge midpoints)
        const GizmoHandle active = m_IsManipulating ? m_ActiveHandle : m_HoveredHandle;
        for (int i = 0; i < 8; ++i)
        {
            const GizmoHandle h = static_cast<GizmoHandle>(static_cast<int>(GizmoHandle::Bounds2D_0) + i);
            const bool hi = (active == h);
            const bool isCorner = (i < 4);
            const glm::ivec2 s = kBounds2DSigns[i];
            const glm::vec3 hPos = obbCenter
                + localAxes[u] * (static_cast<float>(s.x) * halfU)
                + localAxes[v] * (static_cast<float>(s.y) * halfV);

            const float pxRadius = isCorner ? (hi ? 12.5f : 10.0f) : (hi ? 10.5f : 8.5f);
            const float handleSize = worldPerPixel * pxRadius;

            GizmoPrimitiveInstance handleInst;
            handleInst.meshType = GizmoMeshType::Cube;
            handleInst.worldMatrix = MakeBasisTransform(hPos, localAxes, glm::vec3(handleSize));
            handleInst.color = hi ? kColorHover : (isCorner ? kColorBoundsCorner : kColorBoundsEdge);
            handleInst.hoverFactor = hi ? 1.0f : 0.0f;
            m_DrawList.opaqueInstances.push_back(handleInst);
        }
    }

    void Gizmo::BuildBounds3DDrawList(const glm::mat4 &matrix, const AABB &localBounds, const glm::vec3 &viewDir, float gizmoScale, float worldPerPixel)
    {
        (void)gizmoScale;
        glm::vec3 pos, scale;
        std::array<glm::vec3, 3> localAxes;
        DecomposeMatrixBasis(matrix, pos, localAxes, scale);

        const glm::vec3 absScale = glm::max(glm::abs(scale), glm::vec3(1e-4f));
        const glm::vec3 localCenter = localBounds.GetCenter();
        const glm::vec3 localHalf = glm::max(localBounds.GetSize() * 0.5f, glm::vec3(1e-3f));
        const glm::vec3 worldHalf = localHalf * absScale;

        const glm::vec3 obbCenter = pos
            + localAxes[0] * (localCenter.x * scale.x)
            + localAxes[1] * (localCenter.y * scale.y)
            + localAxes[2] * (localCenter.z * scale.z);

        const glm::vec3 ax = localAxes[0] * worldHalf.x;
        const glm::vec3 ay = localAxes[1] * worldHalf.y;
        const glm::vec3 az = localAxes[2] * worldHalf.z;

        const glm::vec3 corners[8] = {
            obbCenter - ax - ay - az,
            obbCenter + ax - ay - az,
            obbCenter + ax + ay - az,
            obbCenter - ax + ay - az,
            obbCenter - ax - ay + az,
            obbCenter + ax - ay + az,
            obbCenter + ax + ay + az,
            obbCenter - ax + ay + az
        };

        static constexpr int kEdges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}
        };

        const float strutRadius = worldPerPixel * 1.75f;
        for (const auto &e : kEdges)
        {
            GizmoPrimitiveInstance strut;
            strut.meshType = GizmoMeshType::Cylinder;
            strut.worldMatrix = MakeSegmentTransform(corners[e[0]], corners[e[1]], strutRadius);
            strut.color = kColorBoundsFrame;
            strut.hoverFactor = 0.0f;
            m_DrawList.opaqueInstances.push_back(strut);
        }

        const GizmoHandle active = m_IsManipulating ? m_ActiveHandle : m_HoveredHandle;
        for (int i = 0; i < 26; ++i)
        {
            const GizmoHandle h = static_cast<GizmoHandle>(static_cast<int>(GizmoHandle::Bounds3D_Base) + i);
            const bool hi = (active == h);
            const glm::ivec3 s = kBounds3DSigns[i];
            const int nonZeroCount = (s.x != 0 ? 1 : 0) + (s.y != 0 ? 1 : 0) + (s.z != 0 ? 1 : 0);

            const glm::vec3 normalDir = SafeNormalize(
                localAxes[0] * static_cast<float>(s.x)
                + localAxes[1] * static_cast<float>(s.y)
                + localAxes[2] * static_cast<float>(s.z), viewDir);
            if (!hi && glm::dot(normalDir, viewDir) < -0.35f)
                continue;

            const glm::vec3 hPos = obbCenter
                + localAxes[0] * (static_cast<float>(s.x) * worldHalf.x)
                + localAxes[1] * (static_cast<float>(s.y) * worldHalf.y)
                + localAxes[2] * (static_cast<float>(s.z) * worldHalf.z);

            GizmoPrimitiveInstance handleInst;
            handleInst.hoverFactor = hi ? 1.0f : 0.0f;

            if (nonZeroCount == 1)
            {
                // Face handle: axis-colored cube / plate
                const int axisIdx = (s.x != 0) ? 0 : (s.y != 0 ? 1 : 2);
                const float sizePx = worldPerPixel * (hi ? 12.5f : 10.0f);
                handleInst.meshType = GizmoMeshType::Cube;
                handleInst.worldMatrix = MakeBasisTransform(hPos, localAxes, glm::vec3(sizePx));
                handleInst.color = GetAxisColor(axisIdx, hi);

                if (hi)
                {
                    const int a = (axisIdx + 1) % 3;
                    const int b = (axisIdx + 2) % 3;
                    const glm::vec3 ea = localAxes[a] * worldHalf[a];
                    const glm::vec3 eb = localAxes[b] * worldHalf[b];
                    glm::vec4 faceCol = GetAxisColor(axisIdx, false);
                    faceCol.a = 0.22f;
                    AppendDynamicQuad(m_DrawList.dynamicTranslucentTriangles,
                        hPos - ea - eb, hPos + ea - eb, hPos + ea + eb, hPos - ea + eb,
                        localAxes[axisIdx] * static_cast<float>(s[axisIdx]), faceCol, 1.0f, 1);
                }
            }
            else if (nonZeroCount == 2)
            {
                // Edge midpoint handle: octahedron
                const float sizePx = worldPerPixel * (hi ? 8.0f : 6.2f);
                handleInst.meshType = GizmoMeshType::Octahedron;
                handleInst.worldMatrix = MakeBasisTransform(hPos, localAxes, glm::vec3(sizePx));
                handleInst.color = hi ? kColorHover : kColorBoundsEdge;
            }
            else
            {
                // Corner handle: sphere
                const float sizePx = worldPerPixel * (hi ? 7.5f : 5.8f);
                handleInst.meshType = GizmoMeshType::Sphere;
                handleInst.worldMatrix = MakeBasisTransform(hPos, localAxes, glm::vec3(sizePx));
                handleInst.color = hi ? kColorHover : kColorBoundsCorner;
            }

            m_DrawList.opaqueInstances.push_back(handleInst);
        }
    }

    void Gizmo::UpdateOverlayInfo(const glm::mat4 &matrix, const AABB &localBounds, float gizmoScale, float worldPerPixel)
    {
        m_OverlayInfo.Clear();
        if (!m_IsManipulating || m_ActiveHandle == GizmoHandle::None || m_Operation == GizmoOperation::NONE)
            return;

        const glm::mat4 viewProj = m_GizmoInfo.cameraProjection * m_GizmoInfo.cameraView;
        const glm::vec3 currentPos = glm::vec3(matrix[3]);
        const glm::vec3 viewDir = ComputeViewDirAtPivot(m_GizmoInfo, currentPos);

        m_OverlayInfo.active = true;
        m_OverlayInfo.operation = m_Operation;
        m_OverlayInfo.activeHandle = m_ActiveHandle;
        m_OverlayInfo.mode = m_Mode;
        m_OverlayInfo.snapping = m_GizmoInfo.isSnapping && m_GizmoInfo.snapValue > 0.0f;
        m_OverlayInfo.snapValue = m_GizmoInfo.snapValue;
        m_OverlayInfo.symmetric = m_GizmoInfo.altDown;
        m_OverlayInfo.aspectLocked = m_GizmoInfo.shiftDown;
        m_OverlayInfo.currentPosition = currentPos;
        m_OverlayInfo.primaryAnchorScreen = m_GizmoInfo.mousePos;

        auto pushGuideLine = [&](const glm::vec3 &startWorld, const glm::vec3 &endWorld, const glm::vec4 &color, bool dashed, bool originDot, bool endTicks)
        {
            GizmoOverlayGuideLine line;
            line.worldStart = startWorld;
            line.worldEnd = endWorld;
            line.color = color;
            line.dashed = dashed;
            line.showOriginDot = originDot;
            line.showEndTicks = endTicks;
            line.valid = ProjectToScreen(startWorld, viewProj, m_GizmoInfo.viewRect, line.screenStart)
                && ProjectToScreen(endWorld, viewProj, m_GizmoInfo.viewRect, line.screenEnd);
            m_OverlayInfo.guideLines.push_back(std::move(line));
        };

        auto pushLabel = [&](const std::string &text, const glm::vec3 &worldPos, const glm::vec4 &accent, bool emphasized)
        {
            GizmoOverlayLabel lbl;
            lbl.text = text;
            lbl.worldPos = worldPos;
            lbl.accentColor = accent;
            lbl.emphasized = emphasized;
            lbl.valid = ProjectToScreen(worldPos, viewProj, m_GizmoInfo.viewRect, lbl.screenPos);
            m_OverlayInfo.labels.push_back(std::move(lbl));
        };

        static constexpr const char *kAxisNames[3] = { "X", "Y", "Z" };
        static constexpr const char *kDimNames[3] = { "W", "H", "D" };

        if (m_Operation == GizmoOperation::TRANSLATE)
        {
            const glm::vec3 worldDelta = currentPos - m_DragStartPivot;
            const glm::vec3 axisDelta(
                glm::dot(worldDelta, m_DragStartAxes[0]),
                glm::dot(worldDelta, m_DragStartAxes[1]),
                glm::dot(worldDelta, m_DragStartAxes[2]));
            const float dist = glm::length(worldDelta);

            m_OverlayInfo.worldTranslationDelta = worldDelta;
            m_OverlayInfo.translationDelta = axisDelta;
            m_OverlayInfo.translationDistance = dist;

            std::string labelText;
            if (m_ActiveHandle == GizmoHandle::TranslateX || m_ActiveHandle == GizmoHandle::TranslateY || m_ActiveHandle == GizmoHandle::TranslateZ)
            {
                const int idx = static_cast<int>(m_ActiveHandle) - static_cast<int>(GizmoHandle::TranslateX);
                m_OverlayInfo.accentColor = GetAxisColor(idx, false);
                m_OverlayInfo.titleText = std::string("Move ") + kAxisNames[idx];
                m_OverlayInfo.primaryText = std::string("d") + kAxisNames[idx] + " " + FormatSigned(axisDelta[idx], 2) + "m";
                labelText = FormatSigned(axisDelta[idx], 2) + "m";
            }
            else if (m_ActiveHandle == GizmoHandle::TranslateXY || m_ActiveHandle == GizmoHandle::TranslateXZ || m_ActiveHandle == GizmoHandle::TranslateYZ)
            {
                int a = 0, b = 1, missing = 2;
                const char *planeLabel = "XY";
                if (m_ActiveHandle == GizmoHandle::TranslateXZ) { a = 0; b = 2; missing = 1; planeLabel = "XZ"; }
                else if (m_ActiveHandle == GizmoHandle::TranslateYZ) { a = 1; b = 2; missing = 0; planeLabel = "YZ"; }

                m_OverlayInfo.accentColor = GetPlaneColor(missing, false);
                m_OverlayInfo.titleText = std::string("Move ") + planeLabel;
                m_OverlayInfo.primaryText = std::string("d") + kAxisNames[a] + " " + FormatSigned(axisDelta[a], 2) + "m  d"
                    + kAxisNames[b] + " " + FormatSigned(axisDelta[b], 2) + "m  (|d| " + FormatFloat(dist, 2) + "m)";
                labelText = FormatFloat(dist, 2) + "m";
            }
            else
            {
                m_OverlayInfo.accentColor = kColorScreen;
                m_OverlayInfo.titleText = "Move Free";
                m_OverlayInfo.primaryText = "d(" + FormatSigned(worldDelta.x, 2) + ", " + FormatSigned(worldDelta.y, 2)
                    + ", " + FormatSigned(worldDelta.z, 2) + ")m  (|d| " + FormatFloat(dist, 2) + "m)";
                labelText = FormatFloat(dist, 2) + "m";
            }

            m_OverlayInfo.secondaryText = "Pos (" + FormatFloat(currentPos.x, 2) + ", " + FormatFloat(currentPos.y, 2) + ", " + FormatFloat(currentPos.z, 2) + ")";
            m_OverlayInfo.badges.push_back(m_Mode == GizmoMode::LOCAL ? "Local" : "World");
            if (m_OverlayInfo.snapping)
            {
                m_OverlayInfo.badges.push_back("Snap " + FormatFloat(m_GizmoInfo.snapValue, 2) + "m");
            }

            if (dist > 1e-4f)
            {
                pushGuideLine(m_DragStartPivot, currentPos, m_OverlayInfo.accentColor, true, true, false);
                pushLabel(labelText, 0.5f * (m_DragStartPivot + currentPos) + glm::vec3(0.0f, 8.0f * worldPerPixel, 0.0f), m_OverlayInfo.accentColor, true);
            }
            return;
        }

        if (m_Operation == GizmoOperation::ROTATE)
        {
            const float angleDeg = glm::degrees(m_DragAppliedAngle);
            m_OverlayInfo.rotationAngleDeg = angleDeg;

            glm::vec3 rotAxis = m_DragStartAxes[2];
            float ringRadius = gizmoScale;
            if (m_ActiveHandle == GizmoHandle::RotateX)
            {
                rotAxis = m_DragStartAxes[0];
                m_OverlayInfo.accentColor = kColorAxisX;
                m_OverlayInfo.titleText = "Rotate X";
            }
            else if (m_ActiveHandle == GizmoHandle::RotateY)
            {
                rotAxis = m_DragStartAxes[1];
                m_OverlayInfo.accentColor = kColorAxisY;
                m_OverlayInfo.titleText = "Rotate Y";
            }
            else if (m_ActiveHandle == GizmoHandle::RotateZ)
            {
                rotAxis = m_DragStartAxes[2];
                m_OverlayInfo.accentColor = kColorAxisZ;
                m_OverlayInfo.titleText = "Rotate Z";
            }
            else
            {
                rotAxis = -DeriveCameraForward(m_GizmoInfo);
                ringRadius = 1.15f * gizmoScale;
                m_OverlayInfo.accentColor = kColorScreen;
                m_OverlayInfo.titleText = "Rotate View";
            }

            m_OverlayInfo.primaryText = FormatSigned(angleDeg, 1) + " deg";
            m_OverlayInfo.secondaryText = "Delta Angle";
            m_OverlayInfo.badges.push_back(m_Mode == GizmoMode::LOCAL ? "Local" : "World");
            if (m_OverlayInfo.snapping)
            {
                m_OverlayInfo.badges.push_back("Snap " + FormatFloat(m_GizmoInfo.snapValue, 0) + " deg");
            }

            const glm::mat4 halfRot = glm::rotate(glm::mat4(1.0f), 0.5f * m_DragAppliedAngle, rotAxis);
            const glm::vec3 bisector = SafeNormalize(glm::vec3(halfRot * glm::vec4(m_DragStartVec, 0.0f)), m_DragStartVec);
            pushLabel(m_OverlayInfo.primaryText, m_DragStartPivot + bisector * (1.22f * ringRadius), m_OverlayInfo.accentColor, true);
            return;
        }

        if (m_Operation == GizmoOperation::SCALE)
        {
            glm::vec3 curPos, curScale;
            std::array<glm::vec3, 3> curAxes;
            DecomposeMatrixBasis(matrix, curPos, curAxes, curScale);
            const glm::vec3 factors = m_DragVisualScaleFactors;

            m_OverlayInfo.scaleFactors = factors;
            m_OverlayInfo.currentScale = curScale;

            if (m_ActiveHandle == GizmoHandle::ScaleX || m_ActiveHandle == GizmoHandle::ScaleY || m_ActiveHandle == GizmoHandle::ScaleZ)
            {
                const int idx = static_cast<int>(m_ActiveHandle) - static_cast<int>(GizmoHandle::ScaleX);
                m_OverlayInfo.accentColor = GetAxisColor(idx, false);
                m_OverlayInfo.titleText = std::string("Scale ") + kAxisNames[idx];
                m_OverlayInfo.primaryText = FormatFloat(factors[idx], 2) + "x  (d " + FormatSigned(factors[idx] - 1.0f, 2) + "x)";

                const float tipDist = glm::clamp(0.98f * factors[idx], 0.35f, 2.6f) * gizmoScale;
                pushLabel(FormatFloat(factors[idx], 2) + "x", currentPos + m_DragStartAxes[idx] * tipDist, m_OverlayInfo.accentColor, true);
            }
            else if (m_ActiveHandle == GizmoHandle::ScaleXY || m_ActiveHandle == GizmoHandle::ScaleXZ || m_ActiveHandle == GizmoHandle::ScaleYZ)
            {
                int a = 0, b = 1, missing = 2;
                const char *planeLabel = "XY";
                if (m_ActiveHandle == GizmoHandle::ScaleXZ) { a = 0; b = 2; missing = 1; planeLabel = "XZ"; }
                else if (m_ActiveHandle == GizmoHandle::ScaleYZ) { a = 1; b = 2; missing = 0; planeLabel = "YZ"; }

                m_OverlayInfo.accentColor = GetPlaneColor(missing, false);
                m_OverlayInfo.titleText = std::string("Scale ") + planeLabel;
                m_OverlayInfo.primaryText = std::string(kAxisNames[a]) + " " + FormatFloat(factors[a], 2) + "x  "
                    + kAxisNames[b] + " " + FormatFloat(factors[b], 2) + "x";
                const glm::vec3 dir = SafeNormalize(m_DragStartAxes[a] + m_DragStartAxes[b], m_DragStartAxes[a]);
                pushLabel(FormatFloat(factors[a], 2) + "x / " + FormatFloat(factors[b], 2) + "x", currentPos + dir * (0.55f * gizmoScale), m_OverlayInfo.accentColor, true);
                if (m_OverlayInfo.aspectLocked)
                {
                    m_OverlayInfo.badges.push_back("Uniform");
                }
            }
            else
            {
                m_OverlayInfo.accentColor = kColorScreen;
                m_OverlayInfo.titleText = "Scale XYZ";
                m_OverlayInfo.primaryText = FormatFloat(factors.x, 2) + "x  (d " + FormatSigned(factors.x - 1.0f, 2) + "x)";
                pushLabel(FormatFloat(factors.x, 2) + "x", currentPos + viewDir * (0.14f * gizmoScale), m_OverlayInfo.accentColor, true);
            }

            m_OverlayInfo.secondaryText = "Scale (" + FormatFloat(curScale.x, 2) + ", " + FormatFloat(curScale.y, 2) + ", " + FormatFloat(curScale.z, 2) + ")";
            m_OverlayInfo.badges.push_back("Local");
            if (m_OverlayInfo.snapping)
            {
                m_OverlayInfo.badges.push_back("Snap " + FormatFloat(m_GizmoInfo.snapValue, 2) + "x");
            }
            return;
        }

        if (m_Operation == GizmoOperation::BOUND_SIZING_2D)
        {
            const int u = m_DragBounds2DU;
            const int v = m_DragBounds2DV;
            const int handleIdx = static_cast<int>(m_ActiveHandle) - static_cast<int>(GizmoHandle::Bounds2D_0);
            const glm::ivec2 signUV = (handleIdx >= 0 && handleIdx < 8) ? kBounds2DSigns[handleIdx] : glm::ivec2(1, 1);

            glm::vec3 curPos, curScale;
            std::array<glm::vec3, 3> curAxes;
            DecomposeMatrixBasis(matrix, curPos, curAxes, curScale);
            const glm::vec3 absScale = glm::max(glm::abs(curScale), glm::vec3(1e-4f));
            const glm::vec3 localCenter = localBounds.GetCenter();
            const glm::vec3 localSize = glm::max(localBounds.GetSize(), glm::vec3(1e-3f));

            const float curHalfU = 0.5f * localSize[u] * absScale[u];
            const float curHalfV = 0.5f * localSize[v] * absScale[v];
            const glm::vec3 obbCenter = curPos
                + curAxes[0] * (localCenter.x * curScale.x)
                + curAxes[1] * (localCenter.y * curScale.y)
                + curAxes[2] * (localCenter.z * curScale.z);

            const float W = curHalfU * 2.0f;
            const float H = curHalfV * 2.0f;
            const float dW = W - m_DragStartOBBHalfExtents[u] * 2.0f;
            const float dH = H - m_DragStartOBBHalfExtents[v] * 2.0f;

            m_OverlayInfo.dimensions = glm::vec3(W, H, 0.0f);
            m_OverlayInfo.dimensionDelta = glm::vec3(dW, dH, 0.0f);
            m_OverlayInfo.accentColor = kColorBoundsFrame;
            m_OverlayInfo.titleText = "2D Bounds";
            m_OverlayInfo.primaryText = "W " + FormatFloat(W, 2) + "m (d" + FormatSigned(dW, 2)
                + ")  H " + FormatFloat(H, 2) + "m (d" + FormatSigned(dH, 2) + ")";
            m_OverlayInfo.secondaryText = "Center (" + FormatFloat(obbCenter.x, 2) + ", " + FormatFloat(obbCenter.y, 2) + ", " + FormatFloat(obbCenter.z, 2) + ")";

            if (m_OverlayInfo.symmetric) m_OverlayInfo.badges.push_back("Symmetric");
            if (m_OverlayInfo.aspectLocked) m_OverlayInfo.badges.push_back("Aspect");
            if (m_OverlayInfo.snapping) m_OverlayInfo.badges.push_back("Snap " + FormatFloat(m_GizmoInfo.snapValue, 2) + "m");

            const float dimOffset = 18.0f * worldPerPixel;
            const glm::vec3 wStart = obbCenter - curAxes[u] * curHalfU - curAxes[v] * (curHalfV + dimOffset);
            const glm::vec3 wEnd = obbCenter + curAxes[u] * curHalfU - curAxes[v] * (curHalfV + dimOffset);
            pushGuideLine(wStart, wEnd, kColorAxisX, false, false, true);
            pushLabel("W: " + FormatFloat(W, 2) + "m", 0.5f * (wStart + wEnd), kColorAxisX, signUV.x != 0);

            const glm::vec3 hStart = obbCenter + curAxes[u] * (curHalfU + dimOffset) - curAxes[v] * curHalfV;
            const glm::vec3 hEnd = obbCenter + curAxes[u] * (curHalfU + dimOffset) + curAxes[v] * curHalfV;
            pushGuideLine(hStart, hEnd, kColorAxisY, false, false, true);
            pushLabel("H: " + FormatFloat(H, 2) + "m", 0.5f * (hStart + hEnd), kColorAxisY, signUV.y != 0);
            return;
        }

        if (m_Operation == GizmoOperation::BOUND_SIZING_3D)
        {
            glm::vec3 curPos, curScale;
            std::array<glm::vec3, 3> curAxes;
            DecomposeMatrixBasis(matrix, curPos, curAxes, curScale);
            const glm::vec3 absScale = glm::max(glm::abs(curScale), glm::vec3(1e-4f));
            const glm::vec3 localCenter = localBounds.GetCenter();
            const glm::vec3 localSize = glm::max(localBounds.GetSize(), glm::vec3(1e-3f));
            const glm::vec3 obbCenter = curPos
                + curAxes[0] * (localCenter.x * curScale.x)
                + curAxes[1] * (localCenter.y * curScale.y)
                + curAxes[2] * (localCenter.z * curScale.z);

            const glm::vec3 dims = localSize * absScale;
            const glm::vec3 curHalf = dims * 0.5f;
            const glm::vec3 dDims = dims - (m_DragStartOBBHalfExtents * 2.0f);

            m_OverlayInfo.dimensions = dims;
            m_OverlayInfo.dimensionDelta = dDims;

            const glm::ivec3 s = m_DragBoundsSign;
            const int nonZeroCount = (s.x != 0 ? 1 : 0) + (s.y != 0 ? 1 : 0) + (s.z != 0 ? 1 : 0);
            if (nonZeroCount == 1)
            {
                const int k = (s.x != 0) ? 0 : (s.y != 0 ? 1 : 2);
                m_OverlayInfo.accentColor = GetAxisColor(k, false);
            }
            else
            {
                m_OverlayInfo.accentColor = kColorBoundsFrame;
            }

            m_OverlayInfo.titleText = "3D Bounds";
            m_OverlayInfo.primaryText = FormatFloat(dims.x, 2) + " x " + FormatFloat(dims.y, 2) + " x " + FormatFloat(dims.z, 2) + "m";

            for (int k = 0; k < 3; ++k)
            {
                if (s[k] != 0)
                {
                    if (!m_OverlayInfo.secondaryText.empty())
                        m_OverlayInfo.secondaryText += "  ";
                    m_OverlayInfo.secondaryText += std::string("d") + kDimNames[k] + " " + FormatSigned(dDims[k], 2) + "m";
                }
            }

            if (m_OverlayInfo.symmetric) m_OverlayInfo.badges.push_back("Symmetric");
            if (m_OverlayInfo.aspectLocked && nonZeroCount == 3) m_OverlayInfo.badges.push_back("Uniform");
            if (m_OverlayInfo.snapping) m_OverlayInfo.badges.push_back("Snap " + FormatFloat(m_GizmoInfo.snapValue, 2) + "m");

            const float dimOffset = 16.0f * worldPerPixel;
            for (int k = 0; k < 3; ++k)
            {
                const int a = (k + 1) % 3;
                const int b = (k + 2) % 3;
                const float sa = (glm::dot(curAxes[a], viewDir) >= 0.0f) ? 1.0f : -1.0f;
                const float sb = (glm::dot(curAxes[b], viewDir) >= 0.0f) ? 1.0f : -1.0f;
                const glm::vec3 outDir = SafeNormalize(curAxes[a] * sa + curAxes[b] * sb, curAxes[a]);
                const glm::vec3 edgeMid = obbCenter
                    + curAxes[a] * (sa * curHalf[a])
                    + curAxes[b] * (sb * curHalf[b])
                    + outDir * dimOffset;
                const glm::vec3 startW = edgeMid - curAxes[k] * curHalf[k];
                const glm::vec3 endW = edgeMid + curAxes[k] * curHalf[k];
                const glm::vec4 col = GetAxisColor(k, false);

                pushGuideLine(startW, endW, col, false, false, true);
                pushLabel(std::string(kDimNames[k]) + ": " + FormatFloat(dims[k], 2) + "m", edgeMid, col, s[k] != 0);
            }
        }
    }

    void Gizmo::DrawOverlay(ImDrawList *drawList) const
    {
        if (!m_OverlayInfo.active || ImGui::GetCurrentContext() == nullptr)
            return;

        if (!drawList)
            drawList = ImGui::GetWindowDrawList();
        if (!drawList)
            drawList = ImGui::GetForegroundDrawList();
        if (!drawList)
            return;

        const ImVec2 clipMin(m_GizmoInfo.viewRect.min.x, m_GizmoInfo.viewRect.min.y);
        const ImVec2 clipMax(
            std::max(clipMin.x + 1.0f, m_GizmoInfo.viewRect.max.x),
            std::max(clipMin.y + 1.0f, m_GizmoInfo.viewRect.max.y));

        drawList->PushClipRect(clipMin, clipMax, true);

        // 1. CAD dimension guide lines & displacement guide
        for (const GizmoOverlayGuideLine &line : m_OverlayInfo.guideLines)
        {
            if (!line.valid)
                continue;

            const ImVec2 p0(line.screenStart.x, line.screenStart.y);
            const ImVec2 p1(line.screenEnd.x, line.screenEnd.y);
            const ImU32 backingCol = IM_COL32(12, 14, 18, 175);
            const ImU32 lineCol = LinearColorToImU32(line.color, 0.92f);

            if (line.dashed)
            {
                DrawDashedLine2D(drawList, p0, p1, backingCol, 3.0f);
                DrawDashedLine2D(drawList, p0, p1, lineCol, 1.5f);
            }
            else
            {
                drawList->AddLine(p0, p1, backingCol, 3.0f);
                drawList->AddLine(p0, p1, lineCol, 1.5f);
            }

            if (line.showOriginDot)
            {
                drawList->AddCircleFilled(p0, 4.0f, backingCol);
                drawList->AddCircleFilled(p0, 2.75f, lineCol);
            }

            if (line.showEndTicks)
            {
                const float dx = p1.x - p0.x;
                const float dy = p1.y - p0.y;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > 2.0f)
                {
                    const float nx = -dy / len;
                    const float ny = dx / len;
                    const float tickHalf = 5.5f;
                    drawList->AddLine(ImVec2(p0.x - nx * tickHalf, p0.y - ny * tickHalf), ImVec2(p0.x + nx * tickHalf, p0.y + ny * tickHalf), backingCol, 3.0f);
                    drawList->AddLine(ImVec2(p0.x - nx * tickHalf, p0.y - ny * tickHalf), ImVec2(p0.x + nx * tickHalf, p0.y + ny * tickHalf), lineCol, 1.5f);
                    drawList->AddLine(ImVec2(p1.x - nx * tickHalf, p1.y - ny * tickHalf), ImVec2(p1.x + nx * tickHalf, p1.y + ny * tickHalf), backingCol, 3.0f);
                    drawList->AddLine(ImVec2(p1.x - nx * tickHalf, p1.y - ny * tickHalf), ImVec2(p1.x + nx * tickHalf, p1.y + ny * tickHalf), lineCol, 1.5f);
                }
            }
        }

        // 2. Floating world-anchored dimension / delta labels
        for (const GizmoOverlayLabel &lbl : m_OverlayInfo.labels)
        {
            if (!lbl.valid || lbl.text.empty())
                continue;

            const ImVec2 textSize = ImGui::CalcTextSize(lbl.text.c_str());
            const float padX = 7.0f;
            const float padY = 3.0f;
            const float boxW = textSize.x + padX * 2.0f + 4.0f;
            const float boxH = textSize.y + padY * 2.0f;

            const float minX = std::min(lbl.screenPos.x - boxW * 0.5f, clipMax.x - boxW - 6.0f);
            const float minY = std::min(lbl.screenPos.y - boxH * 0.5f, clipMax.y - boxH - 6.0f);
            const float boxX = std::max(clipMin.x + 6.0f, minX);
            const float boxY = std::max(clipMin.y + 6.0f, minY);

            const float alphaMult = lbl.emphasized ? 1.0f : 0.72f;
            const ImU32 bgCol = IM_COL32(18, 20, 25, static_cast<int>(218 * alphaMult));
            const ImU32 borderCol = LinearColorToImU32(lbl.accentColor, 0.75f * alphaMult);
            const ImU32 textCol = IM_COL32(242, 245, 250, static_cast<int>(255 * alphaMult));

            drawList->AddRectFilled(ImVec2(boxX + 1.5f, boxY + 2.0f), ImVec2(boxX + boxW + 1.5f, boxY + boxH + 2.0f), IM_COL32(0, 0, 0, static_cast<int>(110 * alphaMult)), 4.0f);
            drawList->AddRectFilled(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH), bgCol, 4.0f);
            drawList->AddRectFilled(ImVec2(boxX, boxY), ImVec2(boxX + 3.0f, boxY + boxH), LinearColorToImU32(lbl.accentColor, alphaMult), 4.0f, ImDrawFlags_RoundCornersLeft);
            drawList->AddRect(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH), borderCol, 4.0f, 0, 1.0f);
            drawList->AddText(ImVec2(boxX + padX + 3.0f, boxY + padY), textCol, lbl.text.c_str());
        }

        // 3. Primary cursor HUD info card (clamped cleanly inside viewport bounds)
        if (!m_OverlayInfo.primaryText.empty())
        {
            const float fontSize = ImGui::GetFontSize();
            const ImVec2 titleSize = ImGui::CalcTextSize(m_OverlayInfo.titleText.c_str());
            const ImVec2 primarySize = ImGui::CalcTextSize(m_OverlayInfo.primaryText.c_str());
            const ImVec2 secondarySize = m_OverlayInfo.secondaryText.empty() ? ImVec2(0.0f, 0.0f) : ImGui::CalcTextSize(m_OverlayInfo.secondaryText.c_str());

            float badgesWidth = 0.0f;
            for (const std::string &badge : m_OverlayInfo.badges)
            {
                const ImVec2 bSz = ImGui::CalcTextSize(badge.c_str());
                badgesWidth += bSz.x + 12.0f + 4.0f;
            }

            const float padX = 10.0f;
            const float padY = 7.0f;
            const float accentBarW = 3.5f;
            const float rowGap = 3.0f;
            const float headerWidth = titleSize.x + (badgesWidth > 0.0f ? (8.0f + badgesWidth) : 0.0f);
            const float contentW = std::max({ headerWidth, primarySize.x, secondarySize.x });
            const float cardW = accentBarW + padX * 2.0f + contentW;

            float cardH = padY * 2.0f + titleSize.y + rowGap + primarySize.y;
            if (!m_OverlayInfo.secondaryText.empty())
                cardH += rowGap + secondarySize.y;

            const float offsetX = 18.0f;
            const float offsetY = -14.0f - cardH;
            float cardX = m_OverlayInfo.primaryAnchorScreen.x + offsetX;
            float cardY = m_OverlayInfo.primaryAnchorScreen.y + offsetY;

            if (cardX + cardW > clipMax.x - 10.0f)
                cardX = m_OverlayInfo.primaryAnchorScreen.x - offsetX - cardW;
            if (cardY < clipMin.y + 10.0f)
                cardY = m_OverlayInfo.primaryAnchorScreen.y + 22.0f;

            cardX = glm::clamp(cardX, clipMin.x + 8.0f, std::max(clipMin.x + 8.0f, clipMax.x - cardW - 8.0f));
            cardY = glm::clamp(cardY, clipMin.y + 8.0f, std::max(clipMin.y + 8.0f, clipMax.y - cardH - 8.0f));

            const ImVec2 cardMin(cardX, cardY);
            const ImVec2 cardMax(cardX + cardW, cardY + cardH);

            drawList->AddRectFilled(ImVec2(cardMin.x + 2.0f, cardMin.y + 3.0f), ImVec2(cardMax.x + 2.0f, cardMax.y + 3.0f), IM_COL32(0, 0, 0, 125), 6.0f);
            drawList->AddRectFilled(cardMin, cardMax, IM_COL32(18, 20, 24, 234), 6.0f);
            drawList->AddRectFilled(cardMin, ImVec2(cardMin.x + accentBarW, cardMax.y), LinearColorToImU32(m_OverlayInfo.accentColor, 1.0f), 6.0f, ImDrawFlags_RoundCornersLeft);
            drawList->AddRect(cardMin, cardMax, IM_COL32(255, 255, 255, 44), 6.0f, 0, 1.0f);

            const float textX = cardMin.x + accentBarW + padX;
            float curY = cardMin.y + padY;

            // Header row: title + badges
            const ImU32 titleCol = LinearColorToImU32(glm::mix(m_OverlayInfo.accentColor, glm::vec4(1.0f), 0.30f), 1.0f);
            drawList->AddText(ImVec2(textX, curY), titleCol, m_OverlayInfo.titleText.c_str());

            float bx = textX + titleSize.x + 8.0f;
            for (const std::string &badge : m_OverlayInfo.badges)
            {
                const ImVec2 bSz = ImGui::CalcTextSize(badge.c_str());
                const float pillW = bSz.x + 10.0f;
                const float pillH = fontSize + 2.0f;
                drawList->AddRectFilled(ImVec2(bx, curY - 1.0f), ImVec2(bx + pillW, curY - 1.0f + pillH), IM_COL32(255, 255, 255, 24), 3.5f);
                drawList->AddText(ImVec2(bx + 5.0f, curY), IM_COL32(215, 222, 232, 235), badge.c_str());
                bx += pillW + 4.0f;
            }

            curY += titleSize.y + rowGap;
            drawList->AddText(ImVec2(textX, curY), IM_COL32(246, 248, 252, 255), m_OverlayInfo.primaryText.c_str());

            if (!m_OverlayInfo.secondaryText.empty())
            {
                curY += primarySize.y + rowGap;
                drawList->AddText(ImVec2(textX, curY), IM_COL32(168, 176, 188, 240), m_OverlayInfo.secondaryText.c_str());
            }
        }

        drawList->PopClipRect();
    }
}

