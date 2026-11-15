// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include "ignite/graphics/objects/mesh_builder.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/renderer/frame_packet.hpp"
#include "ignite/scene/icamera.hpp"

#include <glm/glm.hpp>
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
    cam.UpdateProjection(1280.0f, 720.0f);
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
