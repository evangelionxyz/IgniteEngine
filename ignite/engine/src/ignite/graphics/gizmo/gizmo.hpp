// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_GRAPHICS_GIZMO_HPP
#define IGN_GRAPHICS_GIZMO_HPP

#include "ignite/core/base.hpp"
#include "ignite/math/aabb.hpp"
#include "ignite/math/math.hpp"
#include "ignite/scene/icamera.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct ImDrawList;

namespace ignite
{
    enum class GizmoOperation : int
    {
        NONE = -1,
        TRANSLATE = 0,
        ROTATE = 1,
        SCALE = 2,
        BOUND_SIZING_2D = 3,
        BOUND_SIZING_3D = 4
    };

    enum class GizmoMode : int
    {
        LOCAL = 0,
        WORLD = 1
    };

    enum class GizmoHandle : int
    {
        None = -1,

        // Translate handles
        TranslateX = 0,
        TranslateY,
        TranslateZ,
        TranslateXY,
        TranslateXZ,
        TranslateYZ,
        TranslateScreen,

        // Rotate handles
        RotateX,
        RotateY,
        RotateZ,
        RotateScreen,

        // Scale handles
        ScaleX,
        ScaleY,
        ScaleZ,
        ScaleXY,
        ScaleXZ,
        ScaleYZ,
        ScaleXYZ,

        // 2D Bound Sizing handles (indices 0..7 map to corner/edge slots)
        Bounds2D_0,
        Bounds2D_1,
        Bounds2D_2,
        Bounds2D_3,
        Bounds2D_4,
        Bounds2D_5,
        Bounds2D_6,
        Bounds2D_7,

        // 3D Bound Sizing handles: 26 handles mapped via (sx, sy, sz) in {-1, 0, +1}^3 \ {(0,0,0)}
        Bounds3D_Base,
        Bounds3D_End = Bounds3D_Base + 25
    };

    enum class GizmoMeshType : uint32_t
    {
        Cylinder = 0,
        Cone = 1,
        Cube = 2,
        Sphere = 3,
        Torus = 4,
        Octahedron = 5
    };

    struct IGN_API GizmoPrimitiveInstance
    {
        GizmoMeshType meshType = GizmoMeshType::Cube;
        glm::mat4 worldMatrix = glm::mat4(1.0f);
        glm::vec4 color = glm::vec4(1.0f);
        glm::vec4 ringCenterAndFlag = glm::vec4(0.0f); // xyz = ring center, w = 1 if horizon fade enabled
        float hoverFactor = 0.0f;
        uint32_t shadingMode = 0; // 0 = Lit3D, 1 = UnlitTranslucent, 2 = RingLit
        bool translucent = false;
    };

    struct IGN_API GizmoVertex
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec4 color = glm::vec4(1.0f);
        float hoverFactor = 0.0f;
        uint32_t shadingMode = 1;
    };

    struct IGN_API GizmoDrawList
    {
        std::vector<GizmoPrimitiveInstance> opaqueInstances;
        std::vector<GizmoPrimitiveInstance> translucentInstances;
        std::vector<GizmoVertex> dynamicOpaqueTriangles;
        std::vector<GizmoVertex> dynamicTranslucentTriangles;

        float occludedDimScale = 0.48f;
        float occludedAlphaScale = 0.42f;
        float depthBias = 0.00025f;

        void Clear()
        {
            opaqueInstances.clear();
            translucentInstances.clear();
            dynamicOpaqueTriangles.clear();
            dynamicTranslucentTriangles.clear();
        }

        bool Empty() const
        {
            return opaqueInstances.empty()
                && translucentInstances.empty()
                && dynamicOpaqueTriangles.empty()
                && dynamicTranslucentTriangles.empty();
        }
    };

    struct IGN_API GizmoInfo
    {
        glm::mat4 cameraView = glm::mat4(1.0f);
        glm::mat4 cameraProjection = glm::mat4(1.0f);
        glm::vec3 cameraPosition = glm::vec3(0.0f);
        glm::vec3 cameraForward = glm::vec3(0.0f, 0.0f, -1.0f);
        ProjectionType cameraType = ProjectionType::Perspective;

        Rect viewRect;

        glm::vec2 mousePos = glm::vec2(0.0f);
        bool mouseDown = false;
        bool mouseClicked = false;
        bool mouseReleased = false;
        bool viewportHovered = true;
        bool shiftDown = false;
        bool ctrlDown = false;
        bool altDown = false;
        bool hasExplicitInput = false;

        float snapValue = 0.05f;
        bool isSnapping = false;
        float screenSizePx = 105.0f;
    };

    class IGN_API Gizmo
    {
    public:
        Gizmo() = default;
        ~Gizmo();

        void SetInfo(const GizmoInfo &info);

        void SetOperation(GizmoOperation op);
        void SetMode(GizmoMode mode);

        GizmoMode GetMode() const { return m_Mode; }
        GizmoOperation GetOperation() const { return m_Operation; }

        bool Manipulate(glm::mat4 &inOutMatrix, const AABB &localBounds = AABB(glm::vec3(0.0f), glm::vec3(1.0f)));
        void BuildDrawList(const glm::mat4 &matrix, const AABB &localBounds = AABB(glm::vec3(0.0f), glm::vec3(1.0f)));

        const GizmoDrawList &GetDrawList() const { return m_DrawList; }
        void ClearDrawList() { m_DrawList.Clear(); }

        bool IsManipulating() const { return m_IsManipulating; }
        bool IsHovered() const { return m_HoveredHandle != GizmoHandle::None; }
        GizmoHandle GetHoveredHandle() const { return m_HoveredHandle; }
        GizmoHandle GetActiveHandle() const { return m_ActiveHandle; }

        struct GizmoOverlayLabel
        {
            std::string text;
            glm::vec3 worldPos = glm::vec3(0.0f);
            glm::vec2 screenPos = glm::vec2(0.0f);
            glm::vec4 accentColor = glm::vec4(1.0f);
            bool emphasized = true;
            bool valid = false;
        };

        struct GizmoOverlayGuideLine
        {
            glm::vec3 worldStart = glm::vec3(0.0f);
            glm::vec3 worldEnd = glm::vec3(0.0f);
            glm::vec2 screenStart = glm::vec2(0.0f);
            glm::vec2 screenEnd = glm::vec2(0.0f);
            glm::vec4 color = glm::vec4(1.0f);
            bool dashed = false;
            bool showOriginDot = false;
            bool showEndTicks = false;
            bool valid = false;
        };

        struct GizmoOverlayInfo
        {
            bool active = false;
            GizmoOperation operation = GizmoOperation::NONE;
            GizmoHandle activeHandle = GizmoHandle::None;
            GizmoMode mode = GizmoMode::LOCAL;
            bool snapping = false;
            float snapValue = 0.0f;
            bool symmetric = false;
            bool aspectLocked = false;

            glm::vec3 translationDelta = glm::vec3(0.0f);
            glm::vec3 worldTranslationDelta = glm::vec3(0.0f);
            float translationDistance = 0.0f;
            glm::vec3 currentPosition = glm::vec3(0.0f);

            float rotationAngleDeg = 0.0f;

            glm::vec3 scaleFactors = glm::vec3(1.0f);
            glm::vec3 currentScale = glm::vec3(1.0f);

            glm::vec3 dimensions = glm::vec3(0.0f);
            glm::vec3 dimensionDelta = glm::vec3(0.0f);

            glm::vec4 accentColor = glm::vec4(1.0f, 0.84f, 0.12f, 1.0f);
            glm::vec2 primaryAnchorScreen = glm::vec2(0.0f);

            std::string titleText;
            std::string primaryText;
            std::string secondaryText;
            std::vector<std::string> badges;
            std::vector<GizmoOverlayLabel> labels;
            std::vector<GizmoOverlayGuideLine> guideLines;

            void Clear()
            {
                active = false;
                operation = GizmoOperation::NONE;
                activeHandle = GizmoHandle::None;
                mode = GizmoMode::LOCAL;
                snapping = false;
                snapValue = 0.0f;
                symmetric = false;
                aspectLocked = false;
                translationDelta = glm::vec3(0.0f);
                worldTranslationDelta = glm::vec3(0.0f);
                translationDistance = 0.0f;
                currentPosition = glm::vec3(0.0f);
                rotationAngleDeg = 0.0f;
                scaleFactors = glm::vec3(1.0f);
                currentScale = glm::vec3(1.0f);
                dimensions = glm::vec3(0.0f);
                dimensionDelta = glm::vec3(0.0f);
                accentColor = glm::vec4(1.0f, 0.84f, 0.12f, 1.0f);
                primaryAnchorScreen = glm::vec2(0.0f);
                titleText.clear();
                primaryText.clear();
                secondaryText.clear();
                badges.clear();
                labels.clear();
                guideLines.clear();
            }
        };

        const GizmoOverlayInfo &GetOverlayInfo() const { return m_OverlayInfo; }
        void DrawOverlay(ImDrawList *drawList = nullptr) const;

        void ResetInteraction();

        static bool IsAnyManipulating();

    private:
        void ResolveInputState();
        Ray ComputeMouseRay(const glm::vec2 &screenPos) const;
        float ComputeGizmoScale(const glm::vec3 &pivot, float *outWorldPerPixel = nullptr) const;
        void ComputeGizmoAxes(const glm::mat4 &matrix, bool forceLocal, std::array<glm::vec3, 3> &outAxes) const;
        void Select2DBoundsAxes(const std::array<glm::vec3, 3> &localAxes, const glm::vec3 &viewDir, const AABB &localBounds, int &outU, int &outV, int &outW) const;
        float ComputeAxisVisibility(const glm::vec3 &axisDir, const glm::vec3 &viewDir) const;

        GizmoHandle HitTest(const glm::mat4 &matrix, const AABB &localBounds, float gizmoScale, float worldPerPixel) const;
        void UpdateDrag(glm::mat4 &inOutMatrix, const AABB &localBounds, float gizmoScale, float worldPerPixel);

        void BuildTranslateDrawList(const glm::vec3 &pivot, const std::array<glm::vec3, 3> &axes, const glm::vec3 &viewDir, float gizmoScale);
        void BuildRotateDrawList(const glm::vec3 &pivot, const std::array<glm::vec3, 3> &axes, const glm::vec3 &viewDir, float gizmoScale);
        void BuildScaleDrawList(const glm::vec3 &pivot, const std::array<glm::vec3, 3> &axes, const glm::vec3 &viewDir, float gizmoScale);
        void BuildBounds2DDrawList(const glm::mat4 &matrix, const AABB &localBounds, const glm::vec3 &viewDir, float gizmoScale, float worldPerPixel);
        void BuildBounds3DDrawList(const glm::mat4 &matrix, const AABB &localBounds, const glm::vec3 &viewDir, float gizmoScale, float worldPerPixel);
        void UpdateOverlayInfo(const glm::mat4 &matrix, const AABB &localBounds, float gizmoScale, float worldPerPixel);

        void SetManipulatingState(bool manipulating);

    private:
        GizmoInfo m_GizmoInfo;
        GizmoMode m_Mode = GizmoMode::LOCAL;
        GizmoOperation m_Operation = GizmoOperation::NONE;

        GizmoDrawList m_DrawList;

        GizmoHandle m_HoveredHandle = GizmoHandle::None;
        GizmoHandle m_ActiveHandle = GizmoHandle::None;
        bool m_IsManipulating = false;
        bool m_PrevMouseDown = false;

        // Cached drag state
        glm::mat4 m_DragStartMatrix = glm::mat4(1.0f);
        glm::vec3 m_DragStartTranslation = glm::vec3(0.0f);
        glm::mat4 m_DragStartRotation = glm::mat4(1.0f);
        glm::vec3 m_DragStartScale = glm::vec3(1.0f);
        glm::vec3 m_DragStartPivot = glm::vec3(0.0f);
        std::array<glm::vec3, 3> m_DragStartAxes = { glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1) };
        glm::vec3 m_DragStartHitPoint = glm::vec3(0.0f);
        glm::vec3 m_DragStartVec = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 m_DragPrevVec = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec2 m_DragStartMousePos = glm::vec2(0.0f);
        glm::vec2 m_DragPrevMousePos = glm::vec2(0.0f);
        float m_DragStartAxisParam = 0.0f;
        float m_DragStartGizmoScale = 1.0f;
        float m_DragStartWorldPerPixel = 0.01f;
        float m_DragAccumAngle = 0.0f;
        float m_DragAppliedAngle = 0.0f;
        float m_DragVisualAxisScale = 1.0f;
        glm::vec3 m_DragVisualScaleFactors = glm::vec3(1.0f);
        int m_DragBounds2DU = 0;
        int m_DragBounds2DV = 1;
        int m_DragBounds2DW = 2;
        AABB m_DragStartLocalBounds = AABB(glm::vec3(0.0f), glm::vec3(1.0f));
        glm::vec3 m_DragStartOBBCenter = glm::vec3(0.0f);
        glm::vec3 m_DragStartOBBHalfExtents = glm::vec3(0.5f);
        glm::vec3 m_DragBoundsAnchorWorld = glm::vec3(0.0f);
        glm::ivec3 m_DragBoundsSign = glm::ivec3(0);

        GizmoOverlayInfo m_OverlayInfo;
    };
}

#endif

