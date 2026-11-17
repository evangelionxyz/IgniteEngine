// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include "ignite/core/agent_controller.hpp"
#include "ignite/graphics/objects/mesh_builder.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/renderer/frame_packet.hpp"
#include "ignite/math/frustum.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/editor_camera.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/scene/scene_camera.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

using namespace ignite;

namespace
{
    void ValidateMeshGeometry(const MeshBuilder &builder, const glm::vec3 &expectedMin, const glm::vec3 &expectedMax, float tolerance = 0.05f)
    {
        const auto &verts = builder.GetVertices();
        const auto &indices = builder.GetIndices();
        ASSERT_FALSE(verts.empty());
        ASSERT_FALSE(indices.empty());
        EXPECT_EQ(indices.size() % 3u, 0u);

        for (uint32_t idx : indices)
        {
            ASSERT_LT(idx, verts.size());
        }

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            const auto &v0 = verts[indices[i]];
            const auto &v1 = verts[indices[i + 1]];
            const auto &v2 = verts[indices[i + 2]];
            const glm::vec3 faceCross = glm::cross(v1.position - v0.position, v2.position - v0.position);
            const float areaSq = glm::dot(faceCross, faceCross);
            if (areaSq > 1e-12f)
            {
                const glm::vec3 avgNormal = v0.normal + v1.normal + v2.normal;
                EXPECT_GT(glm::dot(faceCross, avgNormal), 0.0f);
            }
        }

        for (const auto &v : verts)
        {
            EXPECT_TRUE(std::isfinite(v.position.x) && std::isfinite(v.position.y) && std::isfinite(v.position.z));
            EXPECT_TRUE(std::isfinite(v.normal.x) && std::isfinite(v.normal.y) && std::isfinite(v.normal.z));
            EXPECT_TRUE(std::isfinite(v.uv.x) && std::isfinite(v.uv.y));
            const float nLen = glm::length(v.normal);
            EXPECT_NEAR(nLen, 1.0f, 0.05f);
        }

        const AABB bounds = builder.CalculateAABB();
        EXPECT_GE(bounds.min.x, expectedMin.x - tolerance);
        EXPECT_GE(bounds.min.y, expectedMin.y - tolerance);
        EXPECT_GE(bounds.min.z, expectedMin.z - tolerance);
        EXPECT_LE(bounds.max.x, expectedMax.x + tolerance);
        EXPECT_LE(bounds.max.y, expectedMax.y + tolerance);
        EXPECT_LE(bounds.max.z, expectedMax.z + tolerance);
    }
}

TEST(MeshBuilderAndRenderer, PrimitiveGeneratorsProduceValidManifolds)
{
    ValidateMeshGeometry(MeshBuilder::CreateCube(glm::vec3(2.0f)), glm::vec3(-1.0f), glm::vec3(1.0f));
    ValidateMeshGeometry(MeshBuilder::CreateSphere(0.5f, 16, 24), glm::vec3(-0.5f), glm::vec3(0.5f));
    ValidateMeshGeometry(MeshBuilder::CreateIcosphere(0.5f, 2), glm::vec3(-0.5f), glm::vec3(0.5f));
    ValidateMeshGeometry(MeshBuilder::CreateCylinder(0.5f, 2.0f, 20, true), glm::vec3(-0.5f, -1.0f, -0.5f), glm::vec3(0.5f, 1.0f, 0.5f));
    ValidateMeshGeometry(MeshBuilder::CreateCapsule(0.5f, 2.0f, 10, 20), glm::vec3(-0.5f, -1.5f, -0.5f), glm::vec3(0.5f, 1.5f, 0.5f));
    ValidateMeshGeometry(MeshBuilder::CreatePlane(glm::vec2(10.0f), 4, 4), glm::vec3(-5.0f, 0.0f, -5.0f), glm::vec3(5.0f, 0.0f, 5.0f));
    ValidateMeshGeometry(MeshBuilder::CreateCone(0.5f, 1.0f, 24, true), glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.5f, 0.5f, 0.5f));
    ValidateMeshGeometry(MeshBuilder::CreateTorus(0.5f, 0.15f, 24, 12), glm::vec3(-0.65f, -0.15f, -0.65f), glm::vec3(0.65f, 0.15f, 0.65f));
}

TEST(MeshBuilderAndRenderer, ModifiersAndStaticMeshBuild)
{
    MeshBuilder plane = MeshBuilder::CreatePlane(glm::vec2(4.0f), 2, 2);
    const size_t initialVerts = plane.GetVertices().size();
    const size_t initialIndices = plane.GetIndices().size();

    plane.Subdivide(1);
    EXPECT_GT(plane.GetVertices().size(), initialVerts);
    EXPECT_EQ(plane.GetIndices().size(), initialIndices * 4u);

    plane.ApplyPerlinDisplacement(0.3f, 2.0f, 2, 42u);
    plane.RecalculateNormals();
    plane.RecalculateTangents();
    const AABB bounds = plane.CalculateAABB();
    EXPECT_GT(bounds.max.y - bounds.min.y, 0.001f);

    MeshBuilder box = MeshBuilder::CreateCube(glm::vec3(1.0f));
    const size_t beforeAppend = plane.GetIndices().size();
    plane.Append(box, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f)));
    EXPECT_EQ(plane.GetIndices().size(), beforeAppend + box.GetIndices().size());

    Ref<StaticMesh> mesh = plane.BuildStaticMesh("UnitTestTerrainCube", AssetHandle(0), true);
    ASSERT_NE(mesh, nullptr);
    ASSERT_FALSE(mesh->GetMeshInstances().empty());
    EXPECT_EQ(mesh->GetMeshInstances()[0]->GetName(), "UnitTestTerrainCube");
    ASSERT_NE(mesh->GetMeshInstances()[0]->GetPrimitive(), nullptr);
    EXPECT_NE(mesh->GetMeshInstances()[0]->GetPrimitive()->vertexBuffer, nullptr);
    EXPECT_NE(mesh->GetMeshInstances()[0]->GetPrimitive()->indexBuffer, nullptr);
}

TEST(MeshBuilderAndRenderer, RendererDefaultPrimitivesAndHandles)
{
    const EMeshType types[] = {
        EMeshType::CUBE,
        EMeshType::SPHERE,
        EMeshType::PLANE,
        EMeshType::CYLINDER,
        EMeshType::CONE,
        EMeshType::TORUS,
    };

    for (EMeshType t : types)
    {
        Ref<StaticMesh> mesh = Renderer::GetDefaultMesh(t);
        ASSERT_NE(mesh, nullptr);
        EXPECT_FALSE(mesh->GetMeshInstances().empty());

        const AssetHandle h = Renderer::GetDefaultMeshHandle(t);
        EXPECT_NE(h, AssetHandle(0));

        EMeshType parsedType = EMeshType::CUBE;
        EXPECT_TRUE(Renderer::IsDefaultMeshHandle(h, &parsedType));
        EXPECT_EQ(parsedType, t);
        EXPECT_EQ(Renderer::GetDefaultMeshByHandle(h), mesh);
    }

    EMeshType parsedByName = EMeshType::CUBE;
    EXPECT_TRUE(Renderer::ParseMeshType("Torus", parsedByName));
    EXPECT_EQ(parsedByName, EMeshType::TORUS);
    EXPECT_TRUE(Renderer::ParseMeshType("cone", parsedByName));
    EXPECT_EQ(parsedByName, EMeshType::CONE);
}

TEST(MeshBuilderAndRenderer, FramePacketReuseAndCameraBasis)
{
    FramePacket packet;
    packet.Reserve(128, 32);
    const size_t cap = packet.staticMeshes.capacity();
    EXPECT_GE(cap, 128u);

    StaticMeshPacketItem item;
    item.objectID = 7;
    packet.staticMeshes.push_back(item);
    packet.staticMeshAABBs.push_back(AABB(glm::vec3(-1.0f), glm::vec3(1.0f)));
    packet.Clear();
    EXPECT_TRUE(packet.staticMeshes.empty());
    EXPECT_TRUE(packet.staticMeshAABBs.empty());
    EXPECT_EQ(packet.staticMeshes.capacity(), cap);

    ICamera cam;
    cam.position = glm::vec3(0.0f, 2.0f, 5.0f);
    cam.pitch = 0.0f;
    cam.yaw = 0.0f;
    cam.UpdateProjection(1280, 720);
    cam.UpdateView();

    const glm::vec3 fwd = cam.GetForwardDirection();
    const glm::vec3 right = cam.GetRightDirection();
    const glm::vec3 up = cam.GetUpDirection();
    EXPECT_NEAR(glm::length(fwd), 1.0f, 0.001f);
    EXPECT_NEAR(glm::length(right), 1.0f, 0.001f);
    EXPECT_NEAR(glm::length(up), 1.0f, 0.001f);
    EXPECT_NEAR(glm::dot(fwd, right), 0.0f, 0.001f);
    EXPECT_NEAR(glm::dot(fwd, up), 0.0f, 0.001f);
}


TEST(RendererShadowAndCamera, CascadedShadowMapNoNearPlaneClipAndUpInvariant)
{
    SceneCamera cam;
    cam.fov = 45.0f;
    cam.nearPlane = 0.1f;
    cam.farPlane = 500.0f;
    cam.position = glm::vec3(10.0f, 5.0f, 20.0f);
    const glm::mat4 camView = glm::lookAt(cam.position, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    cam.SetTransform(glm::inverse(camView));
    cam.UpdateProjection(1280, 720);

    const glm::vec3 toSun = glm::normalize(glm::vec3(-0.4f, 0.8f, -0.45f));
    CascadedShadowMap csm(ShadowMapQuality::LOW);
    csm.ComputeMatrices(&cam, toSun, 200.0f);

    const CSM_GPUData &gpu = csm.GetGPUData();
    EXPECT_EQ(gpu.softShadows, 1);
    EXPECT_FLOAT_EQ(gpu.pcfRadius, 1.5f);
    EXPECT_FLOAT_EQ(gpu.shadowStrength, 1.0f);

    // Every cascade must include upstream shadow casters >= 100m toward the sun without near-plane clipping
    for (int i = 0; i < NUM_CASCADES; ++i)
    {
        const glm::mat4 &vp = gpu.lightViewProj[i];
        for (int col = 0; col < 4; ++col)
        {
            for (int row = 0; row < 4; ++row)
            {
                EXPECT_TRUE(std::isfinite(vp[col][row]));
            }
        }

        const glm::mat4 invVP = glm::inverse(vp);
        glm::vec4 worldCenterH = invVP * glm::vec4(0.0f, 0.0f, 0.5f, 1.0f);
        const glm::vec3 cascadeCenter = glm::vec3(worldCenterH) / worldCenterH.w;

        // Displace 100m upstream toward the sun along +toSun
        const glm::vec3 upstreamCaster = cascadeCenter + toSun * 100.0f;
        glm::vec4 clipCenter = vp * glm::vec4(cascadeCenter, 1.0f);
        glm::vec4 clipUpstream = vp * glm::vec4(upstreamCaster, 1.0f);
        const glm::vec3 ndcCenter = glm::vec3(clipCenter) / clipCenter.w;
        const glm::vec3 ndcUpstream = glm::vec3(clipUpstream) / clipUpstream.w;

        EXPECT_NEAR(ndcUpstream.x, ndcCenter.x, 1e-3f);
        EXPECT_NEAR(ndcUpstream.y, ndcCenter.y, 1e-3f);
        EXPECT_GE(ndcUpstream.z, 0.0f);
        EXPECT_LE(ndcUpstream.z, 1.0f);
        EXPECT_LT(ndcUpstream.z, ndcCenter.z);

        Frustum cascadeFrustum(vp);
        EXPECT_TRUE(cascadeFrustum.IsPointVisible(upstreamCaster));
    }

    // Light view-projection must not depend on stale SceneCamera pitch/yaw values
    cam.pitch = 1.25f;
    cam.yaw = -2.1f;
    csm.ComputeMatrices(&cam, toSun, 200.0f);
    const CSM_GPUData &gpuAfterAngles = csm.GetGPUData();
    for (int i = 0; i < NUM_CASCADES; ++i)
    {
        for (int col = 0; col < 4; ++col)
        {
            for (int row = 0; row < 4; ++row)
            {
                EXPECT_NEAR(gpuAfterAngles.lightViewProj[i][col][row], gpu.lightViewProj[i][col][row], 1e-4f);
            }
        }
    }

    // Overhead vertical sun must fall back cleanly without NaNs
    csm.ComputeMatrices(&cam, glm::vec3(0.0f, 1.0f, 0.0f), 200.0f);
    for (int i = 0; i < NUM_CASCADES; ++i)
    {
        for (int col = 0; col < 4; ++col)
        {
            for (int row = 0; row < 4; ++row)
            {
                EXPECT_TRUE(std::isfinite(csm.GetGPUData().lightViewProj[i][col][row]));
            }
        }
    }
}

TEST(RendererShadowAndCamera, ReferencePipelineComponentDefaults)
{
    DirectionalLightComponent dirLight;
    EXPECT_TRUE(dirLight.cascadeShadow);
    EXPECT_TRUE(dirLight.softShadows);
    EXPECT_FLOAT_EQ(dirLight.intensity, 3.0f);
    EXPECT_FLOAT_EQ(dirLight.shadowStrength, 1.0f);
    EXPECT_FLOAT_EQ(dirLight.shadowMinBias, 0.0002f);
    EXPECT_FLOAT_EQ(dirLight.shadowMaxBias, 0.002f);
    EXPECT_LT(dirLight.shadowMinBias, dirLight.shadowMaxBias);
    EXPECT_FLOAT_EQ(dirLight.pcfRadius, 1.5f);

    WorldEnvironment worldEnv;
    EXPECT_TRUE(worldEnv.enableIBL);
    EXPECT_FLOAT_EQ(worldEnv.exposure, 1.0f);
    EXPECT_FLOAT_EQ(worldEnv.gamma, 2.2f);

    CameraComponent camComp;
    EXPECT_TRUE(camComp.camera.postProcessing.enableSSAO);
    EXPECT_FLOAT_EQ(camComp.camera.postProcessing.aoRadius, 0.65f);
    EXPECT_FLOAT_EQ(camComp.camera.postProcessing.aoIntensity, 1.35f);
    EXPECT_FLOAT_EQ(camComp.camera.postProcessing.aoBias, 0.025f);
    EXPECT_FLOAT_EQ(camComp.camera.postProcessing.aoPower, 1.0f);
    EXPECT_EQ(camComp.camera.postProcessing.tonemapMode, TonemapMode::Filmic);
    EXPECT_FLOAT_EQ(camComp.camera.lens.exposure, 1.0f);
    EXPECT_FLOAT_EQ(camComp.camera.lens.gamma, 2.2f);
}

TEST(RendererShadowAndCamera, EditorCameraSetTargetAndDistancePreserved)
{
    EditorCamera editorCam("TestEditorCam");
    editorCam.SetNavigationMode(EditorCamera::NavigationMode::Orbit);

    AgentExecutionContext ctx;
    ctx.camera = &editorCam;

    nlohmann::ordered_json cmd = {
        { "action", "set_camera" },
        { "position", { 12.0f, 6.0f, 18.0f } },
        { "target", { 1.0f, 2.0f, -3.0f } }
    };
    const auto res = AgentController::ExecuteCommand(cmd, ctx);
    ASSERT_TRUE(res.value("ok", false));

    // Simulate subsequent editor frames updating orbit camera from target/distance/pitch/yaw
    editorCam.UpdateCameraPosition(0.016f);
    editorCam.UpdateView();

    EXPECT_NEAR(editorCam.GetTarget().x, 1.0f, 1e-4f);
    EXPECT_NEAR(editorCam.GetTarget().y, 2.0f, 1e-4f);
    EXPECT_NEAR(editorCam.GetTarget().z, -3.0f, 1e-4f);
    EXPECT_NEAR(editorCam.position.x, 12.0f, 1e-3f);
    EXPECT_NEAR(editorCam.position.y, 6.0f, 1e-3f);
    EXPECT_NEAR(editorCam.position.z, 18.0f, 1e-3f);
}
