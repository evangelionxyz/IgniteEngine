// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite/asset/asset_manager.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/gizmo/gizmo.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/math/math.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/editor_camera.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"

#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>

namespace ignite::tests
{
    namespace
    {
        glm::vec4 SampleRGBA8Avg3x3(const std::vector<uint8_t> &bytes, uint32_t rowPitch, uint32_t width, uint32_t height, uint32_t cx, uint32_t cy)
        {
            glm::vec4 sum(0.0f);
            float count = 0.0f;
            for (int dy = -1; dy <= 1; ++dy)
            {
                const int py = static_cast<int>(cy) + dy;
                if (py < 0 || py >= static_cast<int>(height))
                    continue;
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const int px = static_cast<int>(cx) + dx;
                    if (px < 0 || px >= static_cast<int>(width))
                        continue;
                    const size_t offset = static_cast<size_t>(py) * static_cast<size_t>(rowPitch) + static_cast<size_t>(px) * 4u;
                    sum += glm::vec4(
                        static_cast<float>(bytes[offset + 0]) / 255.0f,
                        static_cast<float>(bytes[offset + 1]) / 255.0f,
                        static_cast<float>(bytes[offset + 2]) / 255.0f,
                        static_cast<float>(bytes[offset + 3]) / 255.0f);
                    count += 1.0f;
                }
            }
            return count > 0.0f ? (sum / count) : glm::vec4(0.0f);
        }

        bool ProjectWorldToScreenRect(const glm::mat4 &viewProj, const Rect &viewRect, const glm::vec3 &worldPos, glm::vec2 &outScreen)
        {
            const glm::vec4 clip = viewProj * glm::vec4(worldPos, 1.0f);
            if (clip.w <= 1e-5f)
                return false;
            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            const float u = ndc.x * 0.5f + 0.5f;
            const float v = 0.5f - ndc.y * 0.5f;
            const glm::vec2 sz = viewRect.GetSize();
            outScreen = viewRect.min + glm::vec2(u * sz.x, v * sz.y);
            return true;
        }

        bool ProjectWorldToPixel(ICamera &cam, const glm::vec3 &worldPos, uint32_t width, uint32_t height, uint32_t &outX, uint32_t &outY)
        {
            glm::vec2 screen(0.0f);
            const Rect rect(glm::vec2(0.0f), glm::vec2(static_cast<float>(width), static_cast<float>(height)));
            if (!ProjectWorldToScreenRect(cam.GetProjection() * cam.GetView(), rect, worldPos, screen))
                return false;
            if (screen.x < 1.0f || screen.x >= static_cast<float>(width - 1u) || screen.y < 1.0f || screen.y >= static_cast<float>(height - 1u))
                return false;
            outX = static_cast<uint32_t>(std::round(screen.x));
            outY = static_cast<uint32_t>(std::round(screen.y));
            return true;
        }

        float ComputeExpectedGizmoScale(const GizmoInfo &info, const glm::vec3 &pivot)
        {
            const glm::vec2 vpSize = glm::max(info.viewRect.GetSize(), glm::vec2(1.0f));
            const bool isOrtho = (info.cameraType == ProjectionType::Orthographic);
            float clipW = 1.0f;
            if (!isOrtho)
            {
                const glm::vec4 clip = info.cameraProjection * info.cameraView * glm::vec4(pivot, 1.0f);
                clipW = glm::max(clip.w, 0.05f);
            }
            const float projScaleY = glm::max(std::abs(info.cameraProjection[1][1]), 1e-4f);
            const float worldPerPixel = (2.0f * clipW) / (projScaleY * vpSize.y);
            return worldPerPixel * glm::max(info.screenSizePx, 24.0f);
        }

        GizmoInfo MakeGizmoInfoFromCamera(EditorCamera &cam, const Rect &viewRect)
        {
            GizmoInfo info;
            info.cameraView = cam.GetView();
            info.cameraProjection = cam.GetProjection();
            info.cameraPosition = cam.position;
            info.cameraForward = cam.GetForwardDirection();
            info.cameraType = cam.projectionType;
            info.viewRect = viewRect;
            info.hasExplicitInput = true;
            info.viewportHovered = true;
            info.screenSizePx = 105.0f;
            return info;
        }

        void SimulateDrag(Gizmo &gizmo, GizmoInfo baseInfo, glm::mat4 &mat, const AABB &bounds, const glm::vec2 &startPx, const glm::vec2 &endPx, int steps = 4)
        {
            // Hover frame
            GizmoInfo info = baseInfo;
            info.mousePos = startPx;
            info.mouseDown = false;
            info.mouseClicked = false;
            info.mouseReleased = false;
            gizmo.SetInfo(info);
            gizmo.Manipulate(mat, bounds);

            // Press frame
            info.mouseClicked = true;
            info.mouseDown = true;
            gizmo.SetInfo(info);
            gizmo.Manipulate(mat, bounds);

            // Drag steps
            info.mouseClicked = false;
            for (int i = 1; i <= steps; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                info.mousePos = glm::mix(startPx, endPx, t);
                info.mouseDown = true;
                gizmo.SetInfo(info);
                gizmo.Manipulate(mat, bounds);
            }

            // Release frame
            info.mousePos = endPx;
            info.mouseDown = false;
            info.mouseReleased = true;
            gizmo.SetInfo(info);
            gizmo.Manipulate(mat, bounds);
        }

        AssetHandle RegisterGizmoTestMaterial(AssetManager *am, uint64_t id, const glm::vec3 &baseColor, float baseWeight, const glm::vec3 &emissive)
        {
            Ref<Material> mat = CreateRef<Material>();
            mat->name = "GizmoTestMat_" + std::to_string(id);
            mat->gpuData.baseColorFactor = glm::vec4(baseColor, 1.0f);
            mat->gpuData.baseWeight = baseWeight;
            mat->gpuData.emissiveFactor = glm::vec4(emissive, 1.0f);
            mat->gpuData.emissionLuminance = 1.0f;
            mat->gpuData.metallicFactor = 0.0f;
            mat->gpuData.roughnessFactor = 1.0f;
            mat->gpuData.specularWeight = 0.0f;
            mat->SetType(MaterialType::Opaque);
            mat->SetReadyFlag(true);
            mat->SetDirtyFlag(true);
            mat->InvalidateBindingSet();

            const AssetHandle handle(id);
            am->AssignAsset(handle, mat);
            return handle;
        }

        Entity AddGizmoTestCube(Scene *scene, const std::string &name, const glm::vec3 &pos, const glm::vec3 &scale, AssetHandle matHandle)
        {
            Entity e = SceneManager::CreateEntity(scene, name, EntityType_Node);
            auto &tr = e.GetComponent<TransformComponent>();
            tr.local.translation = pos;
            tr.local.scale = scale;
            tr.world = tr.local;
            tr.dirty = true;

            auto &smc = e.AddComponent<StaticMeshComponent>();
            smc.handle = Renderer::GetDefaultMeshHandle(EMeshType::CUBE);
            smc.overrideMaterials[0] = matHandle;
            return e;
        }

        float ComputeLuminance(const glm::vec4 &rgba)
        {
            return 0.2126f * rgba.r + 0.7152f * rgba.g + 0.0722f * rgba.b;
        }
    }

    TEST(GizmoCameraAndViews, PresetsOrthonormalPoleSafeAndCollinearAxisHiding)
    {
        EditorCamera cam("TestCamera");
        cam.UpdateProjection(1280, 720);
        cam.SetTarget(glm::vec3(0.0f));
        cam.SetDistance(12.0f);

        const EditorCamera::EditorViewPreset allPresets[] = {
            EditorCamera::EditorViewPreset::Perspective,
            EditorCamera::EditorViewPreset::Orthographic2_5D,
            EditorCamera::EditorViewPreset::Top,
            EditorCamera::EditorViewPreset::Bottom,
            EditorCamera::EditorViewPreset::Front,
            EditorCamera::EditorViewPreset::Back,
            EditorCamera::EditorViewPreset::Left,
            EditorCamera::EditorViewPreset::Right,
            EditorCamera::EditorViewPreset::Mode2D
        };

        for (const auto preset : allPresets)
        {
            cam.SetViewPreset(preset);
            const glm::vec3 fwd = cam.GetForwardDirection();
            const glm::vec3 right = cam.GetRightDirection();
            const glm::vec3 up = cam.GetUpDirection();

            EXPECT_NEAR(glm::length(fwd), 1.0f, 1e-3f);
            EXPECT_NEAR(glm::length(right), 1.0f, 1e-3f);
            EXPECT_NEAR(glm::length(up), 1.0f, 1e-3f);
            EXPECT_NEAR(glm::dot(fwd, right), 0.0f, 1e-3f);
            EXPECT_NEAR(glm::dot(fwd, up), 0.0f, 1e-3f);
            EXPECT_NEAR(glm::dot(right, up), 0.0f, 1e-3f);

            const glm::mat4 &v = cam.GetView();
            const glm::mat4 &p = cam.GetProjection();
            for (int c = 0; c < 4; ++c)
            {
                for (int r = 0; r < 4; ++r)
                {
                    EXPECT_TRUE(std::isfinite(v[c][r]));
                    EXPECT_TRUE(std::isfinite(p[c][r]));
                }
            }

            if (preset == EditorCamera::EditorViewPreset::Perspective)
            {
                EXPECT_EQ(cam.projectionType, ProjectionType::Perspective);
            }
            else
            {
                EXPECT_EQ(cam.projectionType, ProjectionType::Orthographic);
            }
        }

        // Top and Bottom must keep camera far enough from target in ortho so near objects aren't clipped
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Top);
        EXPECT_LT(cam.GetForwardDirection().y, -0.99f);
        EXPECT_GT(cam.position.y, 10.0f);

        cam.SetViewPreset(EditorCamera::EditorViewPreset::Bottom);
        EXPECT_GT(cam.GetForwardDirection().y, 0.99f);
        EXPECT_LT(cam.position.y, -10.0f);

        // SetOrientationFromView round-trip
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Orthographic2_5D);
        const glm::mat4 savedView = cam.GetView();
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Front);
        cam.SetOrientationFromView(savedView);
        EXPECT_GT(std::abs(cam.GetForwardDirection().x), 0.1f);
        EXPECT_GT(std::abs(cam.GetForwardDirection().y), 0.1f);

        // Verify collinear axis hiding in Gizmo::BuildDrawList:
        // Translate emits center sphere (1) + 2 instances (cylinder+cone) per visible axis + planar bracket quads/borders.
        // In cardinal Front view (viewDir ~ -Z): Z axis is hidden (|dot| > 0.985), X & Y + XY plane are drawn,
        // while Perspective & Orthographic2_5D draw all 3 axis cones/cylinders.
        const Rect rect(glm::vec2(0.0f), glm::vec2(1280.0f, 720.0f));
        Gizmo gizmo;
        gizmo.SetOperation(GizmoOperation::TRANSLATE);
        gizmo.SetMode(GizmoMode::WORLD);

        auto countCones = [](const GizmoDrawList &dl)
        {
            int count = 0;
            for (const auto &inst : dl.opaqueInstances)
                if (inst.meshType == GizmoMeshType::Cone) ++count;
            for (const auto &inst : dl.translucentInstances)
                if (inst.meshType == GizmoMeshType::Cone) ++count;
            return count;
        };

        cam.SetViewPreset(EditorCamera::EditorViewPreset::Front);
        gizmo.SetInfo(MakeGizmoInfoFromCamera(cam, rect));
        gizmo.BuildDrawList(glm::mat4(1.0f));
        EXPECT_EQ(countCones(gizmo.GetDrawList()), 2);

        cam.SetViewPreset(EditorCamera::EditorViewPreset::Right);
        gizmo.SetInfo(MakeGizmoInfoFromCamera(cam, rect));
        gizmo.BuildDrawList(glm::mat4(1.0f));
        EXPECT_EQ(countCones(gizmo.GetDrawList()), 2);

        cam.SetViewPreset(EditorCamera::EditorViewPreset::Top);
        gizmo.SetInfo(MakeGizmoInfoFromCamera(cam, rect));
        gizmo.BuildDrawList(glm::mat4(1.0f));
        EXPECT_EQ(countCones(gizmo.GetDrawList()), 2);

        cam.SetViewPreset(EditorCamera::EditorViewPreset::Orthographic2_5D);
        gizmo.SetInfo(MakeGizmoInfoFromCamera(cam, rect));
        gizmo.BuildDrawList(glm::mat4(1.0f));
        EXPECT_EQ(countCones(gizmo.GetDrawList()), 3);

        cam.SetViewPreset(EditorCamera::EditorViewPreset::Perspective);
        gizmo.SetInfo(MakeGizmoInfoFromCamera(cam, rect));
        gizmo.BuildDrawList(glm::mat4(1.0f));
        EXPECT_EQ(countCones(gizmo.GetDrawList()), 3);
    }

    TEST(GizmoTRSManipulation, TranslateRotateScalePerspectiveAndOrthoEdgeOn)
    {
        const Rect rect(glm::vec2(0.0f), glm::vec2(1280.0f, 720.0f));
        EditorCamera cam("TRS_Cam");
        cam.UpdateProjection(1280, 720);
        cam.SetTarget(glm::vec3(0.0f));
        cam.SetDistance(10.0f);

        // 1. TRANSLATE along X in Front Ortho view, with snapping
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Front);
        {
            Gizmo gizmo;
            gizmo.SetOperation(GizmoOperation::TRANSLATE);
            gizmo.SetMode(GizmoMode::WORLD);

            GizmoInfo info = MakeGizmoInfoFromCamera(cam, rect);
            info.isSnapping = true;
            info.snapValue = 0.5f;

            glm::mat4 mat(1.0f);
            const float gScale = ComputeExpectedGizmoScale(info, glm::vec3(0.0f));
            glm::vec2 startPx, endPx;
            const glm::mat4 vp = info.cameraProjection * info.cameraView;
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(0.5f * gScale, 0.0f, 0.0f), startPx));
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(0.5f * gScale + 1.35f, 0.0f, 0.0f), endPx));

            SimulateDrag(gizmo, info, mat, AABB(glm::vec3(0.0f), glm::vec3(1.0f)), startPx, endPx);

            // Snapped to multiple of 0.5 near 1.5, Y and Z untouched
            EXPECT_NEAR(mat[3].x, 1.5f, 0.05f);
            EXPECT_NEAR(mat[3].y, 0.0f, 1e-3f);
            EXPECT_NEAR(mat[3].z, 0.0f, 1e-3f);
        }

        // 2. TRANSLATE XZ plane in Top Ortho view
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Top);
        {
            Gizmo gizmo;
            gizmo.SetOperation(GizmoOperation::TRANSLATE);
            gizmo.SetMode(GizmoMode::WORLD);
            GizmoInfo info = MakeGizmoInfoFromCamera(cam, rect);
            glm::mat4 mat(1.0f);
            const float gScale = ComputeExpectedGizmoScale(info, glm::vec3(0.0f));
            glm::vec2 startPx, endPx;
            const glm::mat4 vp = info.cameraProjection * info.cameraView;
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(0.34f * gScale, 0.0f, 0.34f * gScale), startPx));
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(0.34f * gScale + 1.2f, 0.0f, 0.34f * gScale - 0.8f), endPx));

            SimulateDrag(gizmo, info, mat, AABB(glm::vec3(0.0f), glm::vec3(1.0f)), startPx, endPx);

            EXPECT_NEAR(mat[3].x, 1.2f, 0.08f);
            EXPECT_NEAR(mat[3].y, 0.0f, 1e-3f);
            EXPECT_NEAR(mat[3].z, -0.8f, 0.08f);
        }

        // 3. TRANSLATE LOCAL vs WORLD on 90-degree rotated entity in Perspective
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Perspective);
        {
            Gizmo gizmo;
            gizmo.SetOperation(GizmoOperation::TRANSLATE);
            gizmo.SetMode(GizmoMode::LOCAL);
            GizmoInfo info = MakeGizmoInfoFromCamera(cam, rect);

            // 90 deg around Y -> local +X points along world -Z
            glm::mat4 mat = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            const float gScale = ComputeExpectedGizmoScale(info, glm::vec3(0.0f));
            const glm::vec3 localXAxis = glm::normalize(glm::vec3(mat[0]));
            glm::vec2 startPx, endPx;
            const glm::mat4 vp = info.cameraProjection * info.cameraView;
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, localXAxis * (0.55f * gScale), startPx));
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, localXAxis * (0.55f * gScale + 1.0f), endPx));

            SimulateDrag(gizmo, info, mat, AABB(glm::vec3(0.0f), glm::vec3(1.0f)), startPx, endPx);

            EXPECT_NEAR(mat[3].x, 0.0f, 0.05f);
            EXPECT_LT(mat[3].z, -0.6f);
        }

        // 4. ROTATE edge-on ring in cardinal Front Ortho view (X ring seen completely edge-on) + active wedge triangles
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Front);
        {
            Gizmo gizmo;
            gizmo.SetOperation(GizmoOperation::ROTATE);
            gizmo.SetMode(GizmoMode::WORLD);
            GizmoInfo info = MakeGizmoInfoFromCamera(cam, rect);
            glm::mat4 mat(1.0f);
            const float gScale = ComputeExpectedGizmoScale(info, glm::vec3(0.0f));
            const glm::mat4 vp = info.cameraProjection * info.cameraView;

            // Point on X ring nearest camera (in Front view viewDir=(0,0,-1), +Z side of X ring)
            glm::vec2 startPx;
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(0.0f, 0.0f, 1.0f * gScale), startPx));
            // Drag 90px vertically to rotate around X axis even when viewed edge-on
            const glm::vec2 endPx = startPx + glm::vec2(0.0f, -90.0f);

            // Hover + click + midpoint drag so we can inspect active wedge triangles mid-drag
            GizmoInfo stepInfo = info;
            stepInfo.mousePos = startPx;
            gizmo.SetInfo(stepInfo);
            gizmo.Manipulate(mat);

            stepInfo.mouseClicked = true;
            stepInfo.mouseDown = true;
            gizmo.SetInfo(stepInfo);
            gizmo.Manipulate(mat);

            stepInfo.mouseClicked = false;
            stepInfo.mousePos = endPx;
            gizmo.SetInfo(stepInfo);
            EXPECT_TRUE(gizmo.Manipulate(mat));
            EXPECT_FALSE(gizmo.GetDrawList().dynamicTranslucentTriangles.empty());

            // Y/Z rotated while X axis stayed unit (1,0,0)
            const glm::vec3 xCol = glm::normalize(glm::vec3(mat[0]));
            const glm::vec3 yCol = glm::normalize(glm::vec3(mat[1]));
            EXPECT_NEAR(xCol.x, 1.0f, 1e-3f);
            EXPECT_GT(std::abs(yCol.z), 0.15f);

            stepInfo.mouseDown = false;
            stepInfo.mouseReleased = true;
            gizmo.SetInfo(stepInfo);
            EXPECT_FALSE(gizmo.Manipulate(mat));
        }

        // 5. SCALE single-axis & uniform XYZ in Orthographic2_5D
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Orthographic2_5D);
        {
            Gizmo gizmo;
            gizmo.SetOperation(GizmoOperation::SCALE);
            GizmoInfo info = MakeGizmoInfoFromCamera(cam, rect);
            const glm::mat4 vp = info.cameraProjection * info.cameraView;

            // Single axis Y scale
            glm::mat4 mat(1.0f);
            const float gScale = ComputeExpectedGizmoScale(info, glm::vec3(0.0f));
            glm::vec2 startPx, endPx;
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(0.0f, 0.55f * gScale, 0.0f), startPx));
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(0.0f, 0.55f * gScale + 0.5f * gScale, 0.0f), endPx));
            SimulateDrag(gizmo, info, mat, AABB(glm::vec3(0.0f), glm::vec3(1.0f)), startPx, endPx);

            glm::vec3 t, r, s;
            Math::DecomposeTransformEuler(mat, t, r, s);
            EXPECT_NEAR(s.x, 1.0f, 1e-3f);
            EXPECT_GT(s.y, 1.35f);
            EXPECT_NEAR(s.z, 1.0f, 1e-3f);

            // Uniform XYZ center scale
            glm::mat4 matUniform(1.0f);
            glm::vec2 centerPx;
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(0.0f), centerPx));
            SimulateDrag(gizmo, info, matUniform, AABB(glm::vec3(0.0f), glm::vec3(1.0f)), centerPx, centerPx + glm::vec2(60.0f, -40.0f));

            Math::DecomposeTransformEuler(matUniform, t, r, s);
            EXPECT_GT(s.x, 1.8f);
            EXPECT_NEAR(s.x, s.y, 1e-3f);
            EXPECT_NEAR(s.y, s.z, 1e-3f);
        }
    }

    TEST(GizmoBoundSizing2DAnd3D, OppositeAnchorInvarianceModifiersAndRotatedOBB)
    {
        const Rect rect(glm::vec2(0.0f), glm::vec2(1280.0f, 720.0f));
        EditorCamera cam("Bounds_Cam");
        cam.UpdateProjection(1280, 720);
        cam.SetTarget(glm::vec3(0.0f));
        cam.SetDistance(10.0f);

        // 1. BOUND_SIZING_2D in Mode2D with offset localBounds: drag +X,+Y corner; verify opposite -X,-Y corner stays invariant
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Mode2D);
        {
            Gizmo gizmo;
            gizmo.SetOperation(GizmoOperation::BOUND_SIZING_2D);
            GizmoInfo info = MakeGizmoInfoFromCamera(cam, rect);
            const glm::mat4 vp = info.cameraProjection * info.cameraView;

            // Non-centered local bounds [0.0..1.0] x [0.0..1.0] -> center (0.5, 0.5, 0), size (1, 1, 0)
            const AABB offsetBounds = AABB::FromMinMax(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f));
            glm::mat4 mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.2f, 0.0f))
                * glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 1.0f, 1.0f));

            const glm::vec3 expectedAnchorWorld = glm::vec3(mat * glm::vec4(offsetBounds.min.x, offsetBounds.min.y, 0.0f, 1.0f));
            const glm::vec3 startCornerWorld = glm::vec3(mat * glm::vec4(offsetBounds.max.x, offsetBounds.max.y, 0.0f, 1.0f));
            const glm::vec3 targetCornerWorld = startCornerWorld + glm::vec3(1.0f, 0.5f, 0.0f);

            glm::vec2 startPx, endPx;
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, startCornerWorld, startPx));
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, targetCornerWorld, endPx));

            SimulateDrag(gizmo, info, mat, offsetBounds, startPx, endPx);

            const glm::vec3 actualAnchorWorld = glm::vec3(mat * glm::vec4(offsetBounds.min.x, offsetBounds.min.y, 0.0f, 1.0f));
            const glm::vec3 actualCornerWorld = glm::vec3(mat * glm::vec4(offsetBounds.max.x, offsetBounds.max.y, 0.0f, 1.0f));
            EXPECT_NEAR(glm::distance(actualAnchorWorld, expectedAnchorWorld), 0.0f, 1e-2f);
            EXPECT_NEAR(glm::distance(actualCornerWorld, targetCornerWorld), 0.0f, 0.05f);

            // Alt symmetric + Shift aspect lock test
            GizmoInfo modInfo = info;
            modInfo.altDown = true;
            modInfo.shiftDown = true;
            glm::mat4 matMod = glm::mat4(1.0f);
            const AABB symBounds(glm::vec3(0.0f), glm::vec3(2.0f, 1.0f, 0.0f));
            const glm::vec3 modCornerStart(1.0f, 0.5f, 0.0f);
            const glm::vec3 modCornerEnd(2.0f, 0.5f, 0.0f);
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, modCornerStart, startPx));
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, modCornerEnd, endPx));

            SimulateDrag(gizmo, modInfo, matMod, symBounds, startPx, endPx);
            // Center must remain at (0,0,0) under Alt symmetric resize
            EXPECT_NEAR(glm::length(glm::vec3(matMod[3])), 0.0f, 1e-3f);
            // Aspect ratio (2:1 world bounds -> uniform scale) preserved under Shift
            const float sx = glm::length(glm::vec3(matMod[0]));
            const float sy = glm::length(glm::vec3(matMod[1]));
            EXPECT_NEAR(sx, sy, 0.05f);
            EXPECT_GT(sx, 1.2f);
        }

        // 2. BOUND_SIZING_3D in Perspective on a rotated entity: face handle, edge handle, corner handle, Alt symmetric
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Perspective);
        {
            Gizmo gizmo;
            gizmo.SetOperation(GizmoOperation::BOUND_SIZING_3D);
            GizmoInfo info = MakeGizmoInfoFromCamera(cam, rect);
            const glm::mat4 vp = info.cameraProjection * info.cameraView;
            const AABB unitBounds(glm::vec3(0.0f), glm::vec3(1.0f));

            const glm::mat4 rotMat =
                glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                * glm::rotate(glm::mat4(1.0f), glm::radians(15.0f), glm::vec3(1.0f, 0.0f, 0.0f));

            // 2a. +X face handle on rotated OBB -> opposite -X face must remain fixed, scale.y/z unchanged
            {
                glm::mat4 mat = rotMat;
                const glm::vec3 axisX = glm::normalize(glm::vec3(mat[0]));
                const glm::vec3 startFace = glm::vec3(mat * glm::vec4(0.5f, 0.0f, 0.0f, 1.0f));
                const glm::vec3 oppFaceBefore = glm::vec3(mat * glm::vec4(-0.5f, 0.0f, 0.0f, 1.0f));
                glm::vec2 startPx, endPx;
                ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, startFace, startPx));
                ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, startFace + axisX * 1.0f, endPx));

                SimulateDrag(gizmo, info, mat, unitBounds, startPx, endPx);

                const glm::vec3 oppFaceAfter = glm::vec3(mat * glm::vec4(-0.5f, 0.0f, 0.0f, 1.0f));
                EXPECT_NEAR(glm::distance(oppFaceBefore, oppFaceAfter), 0.0f, 0.02f);
                EXPECT_NEAR(glm::length(glm::vec3(mat[0])), 2.0f, 0.08f);
                EXPECT_NEAR(glm::length(glm::vec3(mat[1])), 1.0f, 1e-3f);
                EXPECT_NEAR(glm::length(glm::vec3(mat[2])), 1.0f, 1e-3f);
            }

            // 2b. Edge handle (+X, +Y, 0): opposite (-0.5, -0.5, 0) stays anchored, scale.z unchanged
            {
                glm::mat4 mat = glm::mat4(1.0f);
                const glm::vec3 oppEdgeBefore = glm::vec3(mat * glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f));
                glm::vec2 startPx, endPx;
                ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(0.5f, 0.5f, 0.0f), startPx));
                ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(1.1f, 0.9f, 0.0f), endPx));

                SimulateDrag(gizmo, info, mat, unitBounds, startPx, endPx);

                const glm::vec3 oppEdgeAfter = glm::vec3(mat * glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f));
                EXPECT_NEAR(glm::distance(oppEdgeBefore, oppEdgeAfter), 0.0f, 0.03f);
                EXPECT_GT(glm::length(glm::vec3(mat[0])), 1.3f);
                EXPECT_GT(glm::length(glm::vec3(mat[1])), 1.2f);
                EXPECT_NEAR(glm::length(glm::vec3(mat[2])), 1.0f, 1e-3f);
            }

            // 2c. Corner handle (+1,+1,+1) with Shift uniform ratio
            {
                glm::mat4 mat = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 1.0f, 1.0f));
                GizmoInfo cornerInfo = info;
                cornerInfo.shiftDown = true;
                const glm::vec3 oppCornerBefore = glm::vec3(mat * glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f));
                glm::vec2 startPx, endPx;
                ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(1.0f, 0.5f, 0.5f), startPx));
                ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(1.8f, 0.9f, 0.9f), endPx));

                SimulateDrag(gizmo, cornerInfo, mat, unitBounds, startPx, endPx);

                const glm::vec3 oppCornerAfter = glm::vec3(mat * glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f));
                EXPECT_NEAR(glm::distance(oppCornerBefore, oppCornerAfter), 0.0f, 0.04f);
                const float sx = glm::length(glm::vec3(mat[0]));
                const float sy = glm::length(glm::vec3(mat[1]));
                const float sz = glm::length(glm::vec3(mat[2]));
                EXPECT_NEAR(sx / sy, 2.0f, 0.08f);
                EXPECT_NEAR(sy, sz, 0.05f);
            }
        }
    }

    TEST(GizmoGPURenderAndWorldReaction, DepthOcclusionDimmingWorldLightingOrthoAndResize)
    {
        const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteGizmoGPURenderTest";
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
        std::filesystem::create_directories(tempDir / "Assets", ec);

        ProjectInfo info;
        info.name = "GizmoGPURenderTest";
        info.rootDirectory = tempDir;
        info.filepath = tempDir / "GizmoGPURenderTest.ixproj";
        Ref<Project> project = Project::Create(info);
        ASSERT_NE(project, nullptr);

        AssetManager *assetManager = AssetManager::GetInstance();
        ASSERT_NE(assetManager, nullptr);
        assetManager->SetActiveProject(project);

        Ref<Scene> scene = CreateRef<Scene>(project.get());
        project->SetActiveScene(scene);

        const AssetHandle wallMat = RegisterGizmoTestMaterial(assetManager, 9800001ULL, glm::vec3(0.08f, 0.08f, 0.09f), 1.0f, glm::vec3(0.0f));
        const AssetHandle floorMat = RegisterGizmoTestMaterial(assetManager, 9800002ULL, glm::vec3(0.35f, 0.35f, 0.38f), 1.0f, glm::vec3(0.0f));

        AddGizmoTestCube(scene.get(), "Floor", glm::vec3(0.0f, -2.0f, 0.0f), glm::vec3(12.0f, 0.5f, 12.0f), floorMat);
        Entity occluderWall = AddGizmoTestCube(scene.get(), "OccluderWall", glm::vec3(0.0f, 0.0f, 1.5f), glm::vec3(5.0f, 5.0f, 0.25f), wallMat);

        Entity sunEnt = SceneManager::CreateEntity(scene.get(), "SunLight", EntityType_Node);
        {
            auto &ltr = sunEnt.GetComponent<TransformComponent>();
            ltr.local.rotation = glm::angleAxis(glm::radians(-25.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            ltr.world = ltr.local;
            ltr.dirty = true;
            auto &dl = sunEnt.AddComponent<DirectionalLightComponent>();
            dl.color = glm::vec4(1.0f, 0.96f, 0.90f, 1.0f);
            dl.intensity = 2.5f;
            dl.cascadeShadow = false;
        }

        Entity pointEnt = SceneManager::CreateEntity(scene.get(), "FillPointLight", EntityType_Node);
        {
            auto &ptr = pointEnt.GetComponent<TransformComponent>();
            ptr.local.translation = glm::vec3(0.0f, 0.5f, 2.2f);
            ptr.world = ptr.local;
            ptr.dirty = true;
            auto &pl = pointEnt.AddComponent<PointLightComponent>();
            pl.color = glm::vec4(1.0f, 0.5f, 0.2f, 1.0f);
            pl.intensity = 0.0f;
            pl.range = 12.0f;
            pl.enabled = true;
        }

        Entity envEnt = SceneManager::CreateWorldEnvironment(scene.get(), "Env");
        auto &envComp = envEnt.GetComponent<WorldEnvironment>();
        envComp.skyType = SkyType::ProceduralSky;
        envComp.enableIBL = true;
        envComp.dirtyEnvironment = true;

        scene->UpdateTransforms(0.016f);

        ICamera cam;
        cam.fov = 50.0f;
        cam.nearPlane = 0.1f;
        cam.farPlane = 100.0f;
        cam.position = glm::vec3(0.0f, 0.0f, 4.5f);
        cam.SetView(glm::lookAt(cam.position, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
        cam.postProcessing.enableSSAO = false;
        cam.postProcessing.taaProperties.enable = false;

        Ref<SceneRenderer> sceneRenderer = CreateRef<SceneRenderer>();
        sceneRenderer->SetActiveScene(scene);
        sceneRenderer->sceneRenderSettings.enableDepthPrepass = true;
        sceneRenderer->sceneRenderSettings.enableFXAA = true;
        sceneRenderer->sceneRenderSettings.SetSampleMethod(SampleMethod::FXAA);

        Renderer *renderer = Application::GetInstance()->GetRenderer();
        ASSERT_NE(renderer, nullptr);
        uint64_t frameIdx = 1;

        auto renderFrames = [&](uint32_t w, uint32_t h, int count)
        {
            cam.UpdateProjection(w, h);
            sceneRenderer->ResizeFramebuffer(&cam, w, h);
            for (int i = 0; i < count; ++i)
            {
                renderer->BeginFrame(frameIdx++);
                sceneRenderer->BeginFrame();
                sceneRenderer->Render(&cam, Renderer::GetCurrentFrameContext(), true);
            }
        };

        auto setCameraFrontView = [&](bool occludedWallVisible)
        {
            occluderWall.GetComponent<TransformComponent>().local.translation = occludedWallVisible
                ? glm::vec3(0.0f, 0.0f, 1.5f)
                : glm::vec3(0.0f, 0.0f, -15.0f);
            occluderWall.GetComponent<TransformComponent>().dirty = true;
            scene->UpdateTransforms(0.016f);
        };

        // 1. Build TRANSLATE gizmo at origin; verify resize + unoccluded X(Red) / Y(Green) / Z(Blue) channels in gizmoRT
        cam.UpdateProjection(384, 256);
        const Rect vpRect(glm::vec2(0.0f), glm::vec2(384.0f, 256.0f));
        Gizmo gizmo;
        gizmo.SetOperation(GizmoOperation::TRANSLATE);
        gizmo.SetMode(GizmoMode::WORLD);

        GizmoInfo gInfo;
        gInfo.cameraView = cam.GetView();
        gInfo.cameraProjection = cam.GetProjection();
        gInfo.cameraPosition = cam.position;
        gInfo.cameraForward = cam.GetForwardDirection();
        gInfo.cameraType = cam.projectionType;
        gInfo.viewRect = vpRect;
        gInfo.hasExplicitInput = true;
        gizmo.SetInfo(gInfo);
        gizmo.BuildDrawList(glm::mat4(1.0f));
        const float gScale = ComputeExpectedGizmoScale(gInfo, glm::vec3(0.0f));

        // Ensure resize path exercises gizmoRT + composite bindings safely
        setCameraFrontView(false);
        sceneRenderer->SetGizmoDrawList(gizmo.GetDrawList());
        renderFrames(320, 240, 1);
        renderFrames(384, 256, 2);
        GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());

        Ref<CameraRenderTarget> target = sceneRenderer->GetRenderTarget(&cam);
        ASSERT_NE(target, nullptr);
        ASSERT_NE(target->gizmoRT, nullptr);
        ASSERT_NE(target->compositeRT, nullptr);

        uint32_t xShaftPxX = 0, xShaftPxY = 0;
        uint32_t yShaftPxX = 0, yShaftPxY = 0;
        ASSERT_TRUE(ProjectWorldToPixel(cam, glm::vec3(0.83f * gScale, 0.0f, 0.0f), 384, 256, xShaftPxX, xShaftPxY));
        ASSERT_TRUE(ProjectWorldToPixel(cam, glm::vec3(0.0f, 0.83f * gScale, 0.0f), 384, 256, yShaftPxX, yShaftPxY));

        glm::vec4 unoccX(0.0f), unoccY(0.0f);
        {
            std::vector<uint8_t> gizmoBytes;
            uint32_t rowPitch = 0, slicePitch = 0;
            Ref<Texture> gizmoColor = target->gizmoRT->GetColorAttachment(0);
            ASSERT_NE(gizmoColor, nullptr);
            ASSERT_TRUE(Texture::ReadbackTextureSubresource(gizmoColor, 0, 0, gizmoBytes, rowPitch, slicePitch));

            unoccX = SampleRGBA8Avg3x3(gizmoBytes, rowPitch, 384, 256, xShaftPxX, xShaftPxY);
            unoccY = SampleRGBA8Avg3x3(gizmoBytes, rowPitch, 384, 256, yShaftPxX, yShaftPxY);

            EXPECT_GT(unoccX.a, 0.65f);
            EXPECT_GT(unoccX.r, unoccX.g + 0.22f);
            EXPECT_GT(unoccX.r, unoccX.b + 0.20f);

            EXPECT_GT(unoccY.a, 0.65f);
            EXPECT_GT(unoccY.g, unoccY.r + 0.20f);
            EXPECT_GT(unoccY.g, unoccY.b + 0.20f);
        }

        // 2. Occluded gizmo vs Wall-only: Unreal-style dimmed & semi-translucent through wall, yet still visible in composite
        setCameraFrontView(true);
        sceneRenderer->ClearGizmoDrawList();
        renderFrames(384, 256, 2);
        GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());

        glm::vec4 compWallOnly(0.0f);
        {
            std::vector<uint8_t> compBytes;
            uint32_t rowPitch = 0, slicePitch = 0;
            ASSERT_TRUE(Texture::ReadbackTextureSubresource(target->compositeRT->GetColorAttachment(0), 0, 0, compBytes, rowPitch, slicePitch));
            compWallOnly = SampleRGBA8Avg3x3(compBytes, rowPitch, 384, 256, xShaftPxX, xShaftPxY);
        }

        sceneRenderer->SetGizmoDrawList(gizmo.GetDrawList());
        renderFrames(384, 256, 2);
        GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());

        {
            std::vector<uint8_t> gizmoBytes, compBytes;
            uint32_t gPitch = 0, gSlice = 0, cPitch = 0, cSlice = 0;
            ASSERT_TRUE(Texture::ReadbackTextureSubresource(target->gizmoRT->GetColorAttachment(0), 0, 0, gizmoBytes, gPitch, gSlice));
            ASSERT_TRUE(Texture::ReadbackTextureSubresource(target->compositeRT->GetColorAttachment(0), 0, 0, compBytes, cPitch, cSlice));

            const glm::vec4 occX = SampleRGBA8Avg3x3(gizmoBytes, gPitch, 384, 256, xShaftPxX, xShaftPxY);
            const glm::vec4 compOcc = SampleRGBA8Avg3x3(compBytes, cPitch, 384, 256, xShaftPxX, xShaftPxY);

            // Occluded gizmo has reduced alpha and dimmer radiance than unoccluded, but > 0 visibility
            EXPECT_GT(occX.a, 0.12f);
            EXPECT_LT(occX.a, unoccX.a - 0.15f);
            EXPECT_LT(ComputeLuminance(occX), ComputeLuminance(unoccX) * 0.72f);

            // Composite with occluded gizmo visibly tints over wall-only pixel
            EXPECT_GT(compOcc.r - compWallOnly.r, 0.03f);
        }

        // 3. World lighting reaction: dark scene vs bright warm sun + orange point light in front of a neutral lit sphere handle
        setCameraFrontView(false);
        GizmoDrawList litProbeList;
        GizmoPrimitiveInstance probeInst;
        probeInst.meshType = GizmoMeshType::Sphere;
        probeInst.worldMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.55f));
        probeInst.color = glm::vec4(0.22f, 0.22f, 0.22f, 1.0f);
        probeInst.shadingMode = 0;
        litProbeList.opaqueInstances.push_back(probeInst);
        sceneRenderer->SetGizmoDrawList(litProbeList);

        uint32_t centerPxX = 0, centerPxY = 0;
        ASSERT_TRUE(ProjectWorldToPixel(cam, glm::vec3(0.0f, 0.0f, 0.25f), 384, 256, centerPxX, centerPxY));

        // Low-light pass
        envComp.enableIBL = false;
        envComp.dirtyEnvironment = true;
        sunEnt.GetComponent<DirectionalLightComponent>().intensity = 0.0f;
        pointEnt.GetComponent<PointLightComponent>().intensity = 0.0f;
        renderFrames(384, 256, 2);
        GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());

        glm::vec4 darkLitColor(0.0f);
        {
            std::vector<uint8_t> gizmoBytes;
            uint32_t pitch = 0, slice = 0;
            ASSERT_TRUE(Texture::ReadbackTextureSubresource(target->gizmoRT->GetColorAttachment(0), 0, 0, gizmoBytes, pitch, slice));
            darkLitColor = SampleRGBA8Avg3x3(gizmoBytes, pitch, 384, 256, centerPxX, centerPxY);
        }

        // Bright sun + orange-red point light in front of gizmo
        sunEnt.GetComponent<DirectionalLightComponent>().intensity = 5.0f;
        pointEnt.GetComponent<PointLightComponent>().color = glm::vec4(1.0f, 0.45f, 0.15f, 1.0f);
        pointEnt.GetComponent<PointLightComponent>().intensity = 40.0f;
        renderFrames(384, 256, 2);
        GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());

        glm::vec4 brightLitColor(0.0f);
        {
            std::vector<uint8_t> gizmoBytes;
            uint32_t pitch = 0, slice = 0;
            ASSERT_TRUE(Texture::ReadbackTextureSubresource(target->gizmoRT->GetColorAttachment(0), 0, 0, gizmoBytes, pitch, slice));
            brightLitColor = SampleRGBA8Avg3x3(gizmoBytes, pitch, 384, 256, centerPxX, centerPxY);
        }

        EXPECT_GT(ComputeLuminance(brightLitColor), ComputeLuminance(darkLitColor) * 1.25f);
        EXPECT_GT(brightLitColor.r, brightLitColor.b + 0.08f);

        // 4. BOUND_SIZING_3D + Cardinal Front Orthographic GPU render pass
        gizmo.SetOperation(GizmoOperation::BOUND_SIZING_3D);
        gizmo.BuildDrawList(glm::mat4(1.0f), AABB(glm::vec3(0.0f), glm::vec3(1.5f)));
        sceneRenderer->SetGizmoDrawList(gizmo.GetDrawList());
        renderFrames(384, 256, 2);
        GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());

        {
            EditorCamera orthoCam("OrthoRenderTest");
            orthoCam.UpdateProjection(384, 256);
            orthoCam.SetTarget(glm::vec3(0.0f));
            orthoCam.SetDistance(10.0f);
            orthoCam.SetViewPreset(EditorCamera::EditorViewPreset::Front);

            Gizmo orthoGizmo;
            orthoGizmo.SetOperation(GizmoOperation::TRANSLATE);
            orthoGizmo.SetInfo(MakeGizmoInfoFromCamera(orthoCam, vpRect));
            orthoGizmo.BuildDrawList(glm::mat4(1.0f));
            sceneRenderer->SetGizmoDrawList(orthoGizmo.GetDrawList());

            sceneRenderer->ResizeFramebuffer(&orthoCam, 384, 256);
            renderer->BeginFrame(frameIdx++);
            sceneRenderer->BeginFrame();
            sceneRenderer->Render(&orthoCam, Renderer::GetCurrentFrameContext(), true);
            GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());

            Ref<CameraRenderTarget> orthoTarget = sceneRenderer->GetRenderTarget(&orthoCam);
            ASSERT_NE(orthoTarget, nullptr);
            ASSERT_NE(orthoTarget->gizmoRT, nullptr);
            std::vector<uint8_t> orthoGizmoBytes;
            uint32_t oPitch = 0, oSlice = 0;
            ASSERT_TRUE(Texture::ReadbackTextureSubresource(orthoTarget->gizmoRT->GetColorAttachment(0), 0, 0, orthoGizmoBytes, oPitch, oSlice));

            uint32_t ox = 0, oy = 0;
            const float oScale = ComputeExpectedGizmoScale(MakeGizmoInfoFromCamera(orthoCam, vpRect), glm::vec3(0.0f));
            ASSERT_TRUE(ProjectWorldToPixel(orthoCam, glm::vec3(0.83f * oScale, 0.0f, 0.0f), 384, 256, ox, oy));
            const glm::vec4 orthoSample = SampleRGBA8Avg3x3(orthoGizmoBytes, oPitch, 384, 256, ox, oy);
            EXPECT_GT(orthoSample.a, 0.65f);
            EXPECT_GT(orthoSample.r, orthoSample.g + 0.22f);
        }

        GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());
        sceneRenderer.reset();
        project->SetActiveScene(nullptr);
        scene.reset();
        assetManager->Reset();
        Project::CloseActive();
        std::filesystem::remove_all(tempDir, ec);
    }

    TEST(GizmoManipulationOverlay, TelemetryDeltaDimensionsGuideLinesAndLabels)
    {
        const Rect rect(glm::vec2(0.0f), glm::vec2(1280.0f, 720.0f));
        EditorCamera cam("Overlay_Cam");
        cam.UpdateProjection(1280, 720);
        cam.SetTarget(glm::vec3(0.0f));
        cam.SetDistance(10.0f);

        auto beginAndStepDrag = [](Gizmo &gizmo, GizmoInfo baseInfo, glm::mat4 &mat, const AABB &bounds, const glm::vec2 &startPx, const glm::vec2 &endPx)
        {
            GizmoInfo info = baseInfo;
            info.mousePos = startPx;
            info.mouseDown = false;
            info.mouseClicked = false;
            info.mouseReleased = false;
            gizmo.SetInfo(info);
            gizmo.Manipulate(mat, bounds);

            info.mouseClicked = true;
            info.mouseDown = true;
            gizmo.SetInfo(info);
            gizmo.Manipulate(mat, bounds);

            info.mouseClicked = false;
            info.mousePos = endPx;
            info.mouseDown = true;
            gizmo.SetInfo(info);
            return gizmo.Manipulate(mat, bounds);
        };

        auto releaseDrag = [](Gizmo &gizmo, GizmoInfo baseInfo, glm::mat4 &mat, const AABB &bounds, const glm::vec2 &endPx)
        {
            GizmoInfo info = baseInfo;
            info.mousePos = endPx;
            info.mouseDown = false;
            info.mouseReleased = true;
            gizmo.SetInfo(info);
            return gizmo.Manipulate(mat, bounds);
        };

        // 1. TRANSLATE snapped +X drag -> verify live delta text, guide line, label, and clear on release
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Front);
        {
            Gizmo gizmo;
            EXPECT_FALSE(gizmo.GetOverlayInfo().active);
            gizmo.SetOperation(GizmoOperation::TRANSLATE);
            gizmo.SetMode(GizmoMode::WORLD);

            GizmoInfo info = MakeGizmoInfoFromCamera(cam, rect);
            info.isSnapping = true;
            info.snapValue = 0.5f;

            glm::mat4 mat(1.0f);
            const AABB unitBounds(glm::vec3(0.0f), glm::vec3(1.0f));
            const float gScale = ComputeExpectedGizmoScale(info, glm::vec3(0.0f));
            const glm::mat4 vp = info.cameraProjection * info.cameraView;
            glm::vec2 startPx, endPx;
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(0.5f * gScale, 0.0f, 0.0f), startPx));
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(0.5f * gScale + 1.4f, 0.0f, 0.0f), endPx));

            ASSERT_TRUE(beginAndStepDrag(gizmo, info, mat, unitBounds, startPx, endPx));
            const auto &ov = gizmo.GetOverlayInfo();
            EXPECT_TRUE(ov.active);
            EXPECT_TRUE(ov.snapping);
            EXPECT_NEAR(ov.translationDelta.x, 1.5f, 0.05f);
            EXPECT_NEAR(ov.translationDistance, 1.5f, 0.05f);
            EXPECT_EQ(ov.titleText, "Move X");
            EXPECT_NE(ov.primaryText.find("dX +1.50m"), std::string::npos);
            ASSERT_EQ(ov.guideLines.size(), 1u);
            EXPECT_TRUE(ov.guideLines[0].dashed);
            EXPECT_TRUE(ov.guideLines[0].showOriginDot);
            EXPECT_TRUE(ov.guideLines[0].valid);
            ASSERT_EQ(ov.labels.size(), 1u);
            EXPECT_TRUE(ov.labels[0].valid);

            EXPECT_FALSE(releaseDrag(gizmo, info, mat, unitBounds, endPx));
            EXPECT_FALSE(gizmo.GetOverlayInfo().active);
            EXPECT_TRUE(gizmo.GetOverlayInfo().guideLines.empty());
        }

        // 2. ROTATE snapped 15-deg drag -> verify angle telemetry & arc label
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Front);
        {
            Gizmo gizmo;
            gizmo.SetOperation(GizmoOperation::ROTATE);
            GizmoInfo info = MakeGizmoInfoFromCamera(cam, rect);
            info.isSnapping = true;
            info.snapValue = 15.0f;

            glm::mat4 mat(1.0f);
            const AABB unitBounds(glm::vec3(0.0f), glm::vec3(1.0f));
            const float gScale = ComputeExpectedGizmoScale(info, glm::vec3(0.0f));
            const glm::mat4 vp = info.cameraProjection * info.cameraView;
            glm::vec2 startPx, endPx;
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(1.0f * gScale, 0.0f, 0.0f), startPx));
            const float rad30 = glm::radians(30.0f);
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(std::cos(rad30) * gScale, std::sin(rad30) * gScale, 0.0f), endPx));

            ASSERT_TRUE(beginAndStepDrag(gizmo, info, mat, unitBounds, startPx, endPx));
            const auto &ov = gizmo.GetOverlayInfo();
            EXPECT_TRUE(ov.active);
            EXPECT_TRUE(ov.snapping);
            EXPECT_NEAR(std::abs(ov.rotationAngleDeg), 30.0f, 1.0f);
            EXPECT_NE(ov.primaryText.find("deg"), std::string::npos);
            ASSERT_EQ(ov.labels.size(), 1u);
            EXPECT_TRUE(ov.labels[0].valid);
            releaseDrag(gizmo, info, mat, unitBounds, endPx);
        }

        // 3. SCALE single-axis +Y drag -> verify factor multiplier & tip label
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Orthographic2_5D);
        {
            Gizmo gizmo;
            gizmo.SetOperation(GizmoOperation::SCALE);
            GizmoInfo info = MakeGizmoInfoFromCamera(cam, rect);
            glm::mat4 mat(1.0f);
            const AABB unitBounds(glm::vec3(0.0f), glm::vec3(1.0f));
            const float gScale = ComputeExpectedGizmoScale(info, glm::vec3(0.0f));
            const glm::mat4 vp = info.cameraProjection * info.cameraView;
            glm::vec2 startPx, endPx;
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(0.0f, 0.55f * gScale, 0.0f), startPx));
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(0.0f, 0.55f * gScale + 0.5f * gScale, 0.0f), endPx));

            ASSERT_TRUE(beginAndStepDrag(gizmo, info, mat, unitBounds, startPx, endPx));
            const auto &ov = gizmo.GetOverlayInfo();
            EXPECT_TRUE(ov.active);
            EXPECT_EQ(ov.titleText, "Scale Y");
            EXPECT_GT(ov.scaleFactors.y, 1.35f);
            EXPECT_NE(ov.primaryText.find("x"), std::string::npos);
            ASSERT_EQ(ov.labels.size(), 1u);
            EXPECT_TRUE(ov.labels[0].valid);
            releaseDrag(gizmo, info, mat, unitBounds, endPx);
        }

        // 4. BOUND_SIZING_2D with Alt symmetric + Shift aspect lock -> verify W/H dimensions & 2 CAD guide lines
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Mode2D);
        {
            Gizmo gizmo;
            gizmo.SetOperation(GizmoOperation::BOUND_SIZING_2D);
            GizmoInfo info = MakeGizmoInfoFromCamera(cam, rect);
            info.altDown = true;
            info.shiftDown = true;

            glm::mat4 mat(1.0f);
            const AABB bounds2D(glm::vec3(0.0f), glm::vec3(2.0f, 1.0f, 0.0f));
            const glm::mat4 vp = info.cameraProjection * info.cameraView;
            glm::vec2 startPx, endPx;
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(1.0f, 0.5f, 0.0f), startPx));
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(2.0f, 0.5f, 0.0f), endPx));

            ASSERT_TRUE(beginAndStepDrag(gizmo, info, mat, bounds2D, startPx, endPx));
            const auto &ov = gizmo.GetOverlayInfo();
            EXPECT_TRUE(ov.active);
            EXPECT_TRUE(ov.symmetric);
            EXPECT_TRUE(ov.aspectLocked);
            EXPECT_EQ(ov.titleText, "2D Bounds");
            EXPECT_GT(ov.dimensions.x, 3.0f);
            EXPECT_GT(ov.dimensions.y, 1.5f);
            EXPECT_GT(ov.dimensionDelta.x, 1.0f);
            EXPECT_NE(ov.primaryText.find("W "), std::string::npos);
            EXPECT_NE(ov.primaryText.find("H "), std::string::npos);
            ASSERT_EQ(ov.guideLines.size(), 2u);
            ASSERT_EQ(ov.labels.size(), 2u);
            EXPECT_TRUE(ov.guideLines[0].showEndTicks);
            EXPECT_TRUE(ov.guideLines[1].showEndTicks);
            releaseDrag(gizmo, info, mat, bounds2D, endPx);
        }

        // 5. BOUND_SIZING_3D +X face handle -> verify W x H x D dimensions, delta, & 3 CAD dimension lines
        cam.yaw = glm::radians(45.0f);
        cam.pitch = glm::radians(25.0f);
        cam.SetViewPreset(EditorCamera::EditorViewPreset::Perspective);
        {
            Gizmo gizmo;
            gizmo.SetOperation(GizmoOperation::BOUND_SIZING_3D);
            GizmoInfo info = MakeGizmoInfoFromCamera(cam, rect);

            glm::mat4 mat(1.0f);
            const AABB unitBounds(glm::vec3(0.0f), glm::vec3(1.0f));
            const glm::mat4 vp = info.cameraProjection * info.cameraView;
            glm::vec2 startPx, endPx;
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(0.5f, 0.0f, 0.0f), startPx));
            ASSERT_TRUE(ProjectWorldToScreenRect(vp, rect, glm::vec3(1.5f, 0.0f, 0.0f), endPx));

            ASSERT_TRUE(beginAndStepDrag(gizmo, info, mat, unitBounds, startPx, endPx));
            const auto &ov = gizmo.GetOverlayInfo();
            EXPECT_TRUE(ov.active);
            EXPECT_EQ(ov.titleText, "3D Bounds");
            EXPECT_NEAR(ov.dimensions.x, 2.0f, 0.08f);
            EXPECT_NEAR(ov.dimensions.y, 1.0f, 1e-3f);
            EXPECT_NEAR(ov.dimensions.z, 1.0f, 1e-3f);
            EXPECT_NEAR(ov.dimensionDelta.x, 1.0f, 0.08f);
            EXPECT_NE(ov.primaryText.find("2.00 x 1.00 x 1.00m"), std::string::npos);
            EXPECT_NE(ov.secondaryText.find("dW +1.00m"), std::string::npos);
            ASSERT_EQ(ov.guideLines.size(), 3u);
            ASSERT_EQ(ov.labels.size(), 3u);
            EXPECT_TRUE(ov.labels[0].emphasized);
            EXPECT_FALSE(ov.labels[1].emphasized);
            EXPECT_FALSE(ov.labels[2].emphasized);
            releaseDrag(gizmo, info, mat, unitBounds, endPx);
        }
    }
}

