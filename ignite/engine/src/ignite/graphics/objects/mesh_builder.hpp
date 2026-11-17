// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_MESH_BUILDER_HPP
#define IGN_MESH_BUILDER_HPP

#include "ignite/core/base.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <functional>

namespace ignite
{
    class IGN_API MeshBuilder
    {
    public:
        MeshBuilder() = default;
        ~MeshBuilder() = default;

        // Vertex & Primitive Construction
        uint32_t AddVertex(const VertexMeshStatic &vertex);
        uint32_t AddVertex(const glm::vec3 &position, const glm::vec3 &normal = glm::vec3(0.0f, 1.0f, 0.0f),
            const glm::vec2 &uv = glm::vec2(0.0f), const glm::vec4 &color = glm::vec4(1.0f));

        void AddTriangle(uint32_t i0, uint32_t i1, uint32_t i2);
        // Adds a quad (i0, i1, i2, i3) in counter-clockwise order as two triangles (i0, i1, i2) and (i0, i2, i3)
        void AddQuad(uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3);

        void Append(const MeshBuilder &other, const glm::mat4 &transform = glm::mat4(1.0f));
        void Transform(const glm::mat4 &matrix);

        // Post-processing / Procedural Modifiers
        void RecalculateNormals();
        void RecalculateTangents();
        void FlipNormals();
        void Subdivide(uint32_t iterations = 1);
        void DisplaceAlongNormal(const std::function<float(const glm::vec3 &pos, const glm::vec2 &uv)> &displacementFn);
        void ApplyPerlinDisplacement(float amplitude = 0.25f, float frequency = 2.0f, uint32_t octaves = 3, uint32_t seed = 1337u);

        void Clear();

        const std::vector<VertexMeshStatic> &GetVertices() const { return m_Vertices; }
        std::vector<VertexMeshStatic> &GetVertices() { return m_Vertices; }
        const std::vector<uint32_t> &GetIndices() const { return m_Indices; }
        std::vector<uint32_t> &GetIndices() { return m_Indices; }

        AABB CalculateAABB() const;

        Ref<MeshPrimitive<VertexMeshStatic>> BuildPrimitive(bool createGpuBuffers = true) const;
        Ref<StaticMesh> BuildStaticMesh(const std::string &name = "ProceduralMesh", AssetHandle materialHandle = AssetHandle(0), bool createGpuBuffers = true) const;

        // Built-in Procedural Primitive Generators
        static MeshBuilder CreateCube(const glm::vec3 &size = glm::vec3(1.0f));
        static MeshBuilder CreateSphere(float radius = 0.5f, uint32_t rings = 24, uint32_t sectors = 36);
        static MeshBuilder CreateIcosphere(float radius = 0.5f, uint32_t subdivisions = 2);
        static MeshBuilder CreateCylinder(float radius = 0.5f, float height = 1.0f, uint32_t sectors = 32, bool capped = true);
        static MeshBuilder CreateCapsule(float radius = 0.5f, float height = 2.0f, uint32_t rings = 12, uint32_t sectors = 24);
        static MeshBuilder CreatePlane(const glm::vec2 &size = glm::vec2(10.0f), uint32_t subdivisionsX = 1, uint32_t subdivisionsZ = 1);
        static MeshBuilder CreateCone(float radius = 0.5f, float height = 1.0f, uint32_t sectors = 32, bool capped = true);
        static MeshBuilder CreateTorus(float majorRadius = 0.5f, float minorRadius = 0.15f, uint32_t majorSegments = 32, uint32_t minorSegments = 16);

    private:
        std::vector<VertexMeshStatic> m_Vertices;
        std::vector<uint32_t> m_Indices;
    };
}

#endif
