// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "mesh_builder.hpp"
#include "ignite/core/device/device_manager.hpp"
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <map>

namespace ignite
{
    namespace
    {
        void ComputeSafeTangentBasis(const glm::vec3 &normal, glm::vec3 &outTangent, glm::vec3 &outBitangent)
        {
            const glm::vec3 n = glm::length(normal) > 1e-6f ? glm::normalize(normal) : glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::vec3 helper = (std::abs(n.x) < 0.9f) ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
            outTangent = glm::normalize(helper - n * glm::dot(n, helper));
            outBitangent = glm::normalize(glm::cross(n, outTangent));
        }

        float Hash3D(int x, int y, int z, uint32_t seed)
        {
            uint32_t n = static_cast<uint32_t>(x * 374761393 + y * 668265263 + z * 2147483647) ^ (seed * 1664525u);
            n = (n ^ (n >> 13u)) * 1274126177u;
            n = n ^ (n >> 16u);
            return static_cast<float>(n & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu) * 2.0f - 1.0f;
        }

        float ValueNoise3D(const glm::vec3 &p, uint32_t seed)
        {
            const glm::ivec3 i = glm::ivec3(glm::floor(p));
            const glm::vec3 f = glm::fract(p);
            const glm::vec3 u = f * f * (3.0f - 2.0f * f);

            const float n000 = Hash3D(i.x + 0, i.y + 0, i.z + 0, seed);
            const float n100 = Hash3D(i.x + 1, i.y + 0, i.z + 0, seed);
            const float n010 = Hash3D(i.x + 0, i.y + 1, i.z + 0, seed);
            const float n110 = Hash3D(i.x + 1, i.y + 1, i.z + 0, seed);
            const float n001 = Hash3D(i.x + 0, i.y + 0, i.z + 1, seed);
            const float n101 = Hash3D(i.x + 1, i.y + 0, i.z + 1, seed);
            const float n011 = Hash3D(i.x + 0, i.y + 1, i.z + 1, seed);
            const float n111 = Hash3D(i.x + 1, i.y + 1, i.z + 1, seed);

            const float nx00 = glm::mix(n000, n100, u.x);
            const float nx10 = glm::mix(n010, n110, u.x);
            const float nx01 = glm::mix(n001, n101, u.x);
            const float nx11 = glm::mix(n011, n111, u.x);

            const float nxy0 = glm::mix(nx00, nx10, u.y);
            const float nxy1 = glm::mix(nx01, nx11, u.y);

            return glm::mix(nxy0, nxy1, u.z);
        }
    }

    uint32_t MeshBuilder::AddVertex(const VertexMeshStatic &vertex)
    {
        const auto index = static_cast<uint32_t>(m_Vertices.size());
        m_Vertices.push_back(vertex);
        return index;
    }

    uint32_t MeshBuilder::AddVertex(const glm::vec3 &position, const glm::vec3 &normal, const glm::vec2 &uv, const glm::vec4 &color)
    {
        VertexMeshStatic v{};
        v.position = position;
        v.normal = glm::length(normal) > 1e-6f ? glm::normalize(normal) : glm::vec3(0.0f, 1.0f, 0.0f);
        v.uv = uv;
        v.color = color;
        ComputeSafeTangentBasis(v.normal, v.tangent, v.bitangent);
        return AddVertex(v);
    }

    void MeshBuilder::AddTriangle(uint32_t i0, uint32_t i1, uint32_t i2)
    {
        m_Indices.push_back(i0);
        m_Indices.push_back(i1);
        m_Indices.push_back(i2);
    }

    void MeshBuilder::AddQuad(uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3)
    {
        AddTriangle(i0, i1, i2);
        AddTriangle(i0, i2, i3);
    }

    void MeshBuilder::Append(const MeshBuilder &other, const glm::mat4 &transform)
    {
        const auto baseVertex = static_cast<uint32_t>(m_Vertices.size());
        const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(transform)));

        m_Vertices.reserve(m_Vertices.size() + other.m_Vertices.size());
        for (const auto &v : other.m_Vertices)
        {
            VertexMeshStatic tv = v;
            tv.position = glm::vec3(transform * glm::vec4(v.position, 1.0f));
            tv.normal = glm::normalize(normalMat * v.normal);
            tv.tangent = glm::normalize(normalMat * v.tangent);
            tv.bitangent = glm::normalize(normalMat * v.bitangent);
            m_Vertices.push_back(tv);
        }

        m_Indices.reserve(m_Indices.size() + other.m_Indices.size());
        for (uint32_t idx : other.m_Indices)
        {
            m_Indices.push_back(baseVertex + idx);
        }
    }

    void MeshBuilder::Transform(const glm::mat4 &matrix)
    {
        const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(matrix)));
        for (auto &v : m_Vertices)
        {
            v.position = glm::vec3(matrix * glm::vec4(v.position, 1.0f));
            v.normal = glm::normalize(normalMat * v.normal);
            v.tangent = glm::normalize(normalMat * v.tangent);
            v.bitangent = glm::normalize(normalMat * v.bitangent);
        }
    }

    void MeshBuilder::RecalculateNormals()
    {
        for (auto &v : m_Vertices)
        {
            v.normal = glm::vec3(0.0f);
        }

        for (size_t i = 0; i + 2 < m_Indices.size(); i += 3)
        {
            const uint32_t i0 = m_Indices[i + 0];
            const uint32_t i1 = m_Indices[i + 1];
            const uint32_t i2 = m_Indices[i + 2];
            if (i0 >= m_Vertices.size() || i1 >= m_Vertices.size() || i2 >= m_Vertices.size())
                continue;

            const glm::vec3 e1 = m_Vertices[i1].position - m_Vertices[i0].position;
            const glm::vec3 e2 = m_Vertices[i2].position - m_Vertices[i0].position;
            const glm::vec3 faceNormal = glm::cross(e1, e2); // Area-weighted normal

            m_Vertices[i0].normal += faceNormal;
            m_Vertices[i1].normal += faceNormal;
            m_Vertices[i2].normal += faceNormal;
        }

        for (auto &v : m_Vertices)
        {
            const float len = glm::length(v.normal);
            v.normal = (len > 1e-6f) ? (v.normal / len) : glm::vec3(0.0f, 1.0f, 0.0f);
        }

        RecalculateTangents();
    }

    void MeshBuilder::RecalculateTangents()
    {
        std::vector<glm::vec3> tan1(m_Vertices.size(), glm::vec3(0.0f));
        std::vector<glm::vec3> tan2(m_Vertices.size(), glm::vec3(0.0f));

        for (size_t i = 0; i + 2 < m_Indices.size(); i += 3)
        {
            const uint32_t i0 = m_Indices[i + 0];
            const uint32_t i1 = m_Indices[i + 1];
            const uint32_t i2 = m_Indices[i + 2];
            if (i0 >= m_Vertices.size() || i1 >= m_Vertices.size() || i2 >= m_Vertices.size())
                continue;

            const glm::vec3 &v0 = m_Vertices[i0].position;
            const glm::vec3 &v1 = m_Vertices[i1].position;
            const glm::vec3 &v2 = m_Vertices[i2].position;

            const glm::vec2 &w0 = m_Vertices[i0].uv;
            const glm::vec2 &w1 = m_Vertices[i1].uv;
            const glm::vec2 &w2 = m_Vertices[i2].uv;

            const glm::vec3 dp1 = v1 - v0;
            const glm::vec3 dp2 = v2 - v0;
            const glm::vec2 duv1 = w1 - w0;
            const glm::vec2 duv2 = w2 - w0;

            const float det = duv1.x * duv2.y - duv1.y * duv2.x;
            if (std::abs(det) > 1e-7f)
            {
                const float r = 1.0f / det;
                const glm::vec3 sdir = (dp1 * duv2.y - dp2 * duv1.y) * r;
                const glm::vec3 tdir = (dp2 * duv1.x - dp1 * duv2.x) * r;

                tan1[i0] += sdir;
                tan1[i1] += sdir;
                tan1[i2] += sdir;

                tan2[i0] += tdir;
                tan2[i1] += tdir;
                tan2[i2] += tdir;
            }
        }

        for (size_t i = 0; i < m_Vertices.size(); ++i)
        {
            const glm::vec3 n = glm::length(m_Vertices[i].normal) > 1e-6f
                ? glm::normalize(m_Vertices[i].normal)
                : glm::vec3(0.0f, 1.0f, 0.0f);
            m_Vertices[i].normal = n;

            glm::vec3 t = tan1[i] - n * glm::dot(n, tan1[i]);
            if (glm::length(t) > 1e-5f)
            {
                t = glm::normalize(t);
                const float handedness = (glm::dot(glm::cross(n, t), tan2[i]) < 0.0f) ? -1.0f : 1.0f;
                m_Vertices[i].tangent = t;
                m_Vertices[i].bitangent = glm::normalize(glm::cross(n, t) * handedness);
            }
            else
            {
                ComputeSafeTangentBasis(n, m_Vertices[i].tangent, m_Vertices[i].bitangent);
            }
        }
    }

    void MeshBuilder::FlipNormals()
    {
        for (auto &v : m_Vertices)
        {
            v.normal = -v.normal;
            v.bitangent = -v.bitangent;
        }
        for (size_t i = 0; i + 2 < m_Indices.size(); i += 3)
        {
            std::swap(m_Indices[i + 1], m_Indices[i + 2]);
        }
    }

    void MeshBuilder::Subdivide(uint32_t iterations)
    {
        for (uint32_t iter = 0; iter < iterations; ++iter)
        {
            std::vector<uint32_t> newIndices;
            newIndices.reserve(m_Indices.size() * 4);
            std::map<std::pair<uint32_t, uint32_t>, uint32_t> midpointCache;

            auto getMidpoint = [&](uint32_t a, uint32_t b) -> uint32_t
            {
                auto edge = (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
                auto it = midpointCache.find(edge);
                if (it != midpointCache.end())
                    return it->second;

                const VertexMeshStatic &va = m_Vertices[a];
                const VertexMeshStatic &vb = m_Vertices[b];

                VertexMeshStatic vm{};
                vm.position = (va.position + vb.position) * 0.5f;
                vm.normal = glm::normalize(va.normal + vb.normal);
                vm.uv = (va.uv + vb.uv) * 0.5f;
                vm.color = (va.color + vb.color) * 0.5f;
                ComputeSafeTangentBasis(vm.normal, vm.tangent, vm.bitangent);

                const uint32_t midIdx = AddVertex(vm);
                midpointCache[edge] = midIdx;
                return midIdx;
            };

            const size_t oldIndexCount = m_Indices.size();
            for (size_t i = 0; i + 2 < oldIndexCount; i += 3)
            {
                const uint32_t i0 = m_Indices[i + 0];
                const uint32_t i1 = m_Indices[i + 1];
                const uint32_t i2 = m_Indices[i + 2];

                const uint32_t m01 = getMidpoint(i0, i1);
                const uint32_t m12 = getMidpoint(i1, i2);
                const uint32_t m20 = getMidpoint(i2, i0);

                newIndices.push_back(i0);  newIndices.push_back(m01); newIndices.push_back(m20);
                newIndices.push_back(i1);  newIndices.push_back(m12); newIndices.push_back(m01);
                newIndices.push_back(i2);  newIndices.push_back(m20); newIndices.push_back(m12);
                newIndices.push_back(m01); newIndices.push_back(m12); newIndices.push_back(m20);
            }

            m_Indices = std::move(newIndices);
        }
        RecalculateTangents();
    }

    void MeshBuilder::DisplaceAlongNormal(const std::function<float(const glm::vec3 &pos, const glm::vec2 &uv)> &displacementFn)
    {
        if (!displacementFn)
            return;

        for (auto &v : m_Vertices)
        {
            const float offset = displacementFn(v.position, v.uv);
            v.position += v.normal * offset;
        }
        RecalculateNormals();
    }

    void MeshBuilder::ApplyPerlinDisplacement(float amplitude, float frequency, uint32_t octaves, uint32_t seed)
    {
        DisplaceAlongNormal([=](const glm::vec3 &pos, const glm::vec2 &) -> float
        {
            float total = 0.0f;
            float freq = frequency;
            float amp = amplitude;
            for (uint32_t o = 0; o < std::max(1u, octaves); ++o)
            {
                total += ValueNoise3D(pos * freq, seed + o * 101u) * amp;
                freq *= 2.0f;
                amp *= 0.5f;
            }
            return total;
        });
    }

    void MeshBuilder::Clear()
    {
        m_Vertices.clear();
        m_Indices.clear();
    }

    AABB MeshBuilder::CalculateAABB() const
    {
        if (m_Vertices.empty())
            return AABB::FromMinMax(glm::vec3(0.0f), glm::vec3(0.0f));

        glm::vec3 minPt(std::numeric_limits<float>::max());
        glm::vec3 maxPt(std::numeric_limits<float>::lowest());
        for (const auto &v : m_Vertices)
        {
            minPt = glm::min(minPt, v.position);
            maxPt = glm::max(maxPt, v.position);
        }
        return AABB::FromMinMax(minPt, maxPt);
    }

    Ref<MeshPrimitive<VertexMeshStatic>> MeshBuilder::BuildPrimitive(bool createGpuBuffers) const
    {
        auto primitive = MeshPrimitive<VertexMeshStatic>::Create(m_Vertices, m_Indices);
        if (createGpuBuffers && DeviceManager::GetInstance() && DeviceManager::GetInstance()->GetDevice())
        {
            primitive->WriteBuffer(nullptr);
        }
        return primitive;
    }

    Ref<StaticMesh> MeshBuilder::BuildStaticMesh(const std::string &name, AssetHandle materialHandle, bool createGpuBuffers) const
    {
        auto mesh = StaticMesh::Create();
        auto primitive = BuildPrimitive(createGpuBuffers);
        auto instance = StaticMeshInstance::Create(name, primitive);
        if (materialHandle != AssetHandle(0))
        {
            instance->SetMaterial(materialHandle);
        }
        mesh->AddMeshInstance(instance);
        mesh->CalculateLocalAABB();
        mesh->SetReadyFlag(true);
        return mesh;
    }

    MeshBuilder MeshBuilder::CreateCube(const glm::vec3 &size)
    {
        MeshBuilder mb;
        const glm::vec3 h = size * 0.5f;

        struct FaceDesc
        {
            glm::vec3 normal;
            glm::vec3 v[4];
        };

        // Counter-clockwise winding when viewed from outside (+normal)
        const FaceDesc faces[6] =
        {
            // +Z (Front)
            { { 0.0f,  0.0f,  1.0f }, { { -h.x, -h.y,  h.z }, {  h.x, -h.y,  h.z }, {  h.x,  h.y,  h.z }, { -h.x,  h.y,  h.z } } },
            // -Z (Back)
            { { 0.0f,  0.0f, -1.0f }, { {  h.x, -h.y, -h.z }, { -h.x, -h.y, -h.z }, { -h.x,  h.y, -h.z }, {  h.x,  h.y, -h.z } } },
            // +X (Right)
            { { 1.0f,  0.0f,  0.0f }, { {  h.x, -h.y,  h.z }, {  h.x, -h.y, -h.z }, {  h.x,  h.y, -h.z }, {  h.x,  h.y,  h.z } } },
            // -X (Left)
            { {-1.0f,  0.0f,  0.0f }, { { -h.x, -h.y, -h.z }, { -h.x, -h.y,  h.z }, { -h.x,  h.y,  h.z }, { -h.x,  h.y, -h.z } } },
            // +Y (Top)
            { { 0.0f,  1.0f,  0.0f }, { { -h.x,  h.y,  h.z }, {  h.x,  h.y,  h.z }, {  h.x,  h.y, -h.z }, { -h.x,  h.y, -h.z } } },
            // -Y (Bottom)
            { { 0.0f, -1.0f,  0.0f }, { { -h.x, -h.y, -h.z }, {  h.x, -h.y, -h.z }, {  h.x, -h.y,  h.z }, { -h.x, -h.y,  h.z } } },
        };

        const glm::vec2 uvs[4] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } };

        for (const auto &f : faces)
        {
            const uint32_t i0 = mb.AddVertex(f.v[0], f.normal, uvs[0]);
            const uint32_t i1 = mb.AddVertex(f.v[1], f.normal, uvs[1]);
            const uint32_t i2 = mb.AddVertex(f.v[2], f.normal, uvs[2]);
            const uint32_t i3 = mb.AddVertex(f.v[3], f.normal, uvs[3]);
            mb.AddQuad(i0, i1, i2, i3);
        }

        mb.RecalculateTangents();
        return mb;
    }

    MeshBuilder MeshBuilder::CreateSphere(float radius, uint32_t rings, uint32_t sectors)
    {
        MeshBuilder mb;
        rings = std::max(3u, rings);
        sectors = std::max(3u, sectors);

        const float pi = glm::pi<float>();
        const float twoPi = glm::two_pi<float>();

        for (uint32_t r = 0; r <= rings; ++r)
        {
            const float v = static_cast<float>(r) / static_cast<float>(rings);
            const float phi = v * pi; // 0 (top +Y) to pi (bottom -Y)
            const float sinPhi = std::sin(phi);
            const float cosPhi = std::cos(phi);

            for (uint32_t s = 0; s <= sectors; ++s)
            {
                const float u = static_cast<float>(s) / static_cast<float>(sectors);
                const float theta = u * twoPi;
                const float sinTheta = std::sin(theta);
                const float cosTheta = std::cos(theta);

                const glm::vec3 n(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
                mb.AddVertex(n * radius, n, glm::vec2(u, v));
            }
        }

        const uint32_t stride = sectors + 1;
        for (uint32_t r = 0; r < rings; ++r)
        {
            for (uint32_t s = 0; s < sectors; ++s)
            {
                const uint32_t i0 = r * stride + s;
                const uint32_t i1 = r * stride + (s + 1);
                const uint32_t i2 = (r + 1) * stride + (s + 1);
                const uint32_t i3 = (r + 1) * stride + s;

                if (r != 0)
                    mb.AddTriangle(i0, i1, i2);
                if (r != rings - 1)
                    mb.AddTriangle(i0, i2, i3);
            }
        }

        mb.RecalculateTangents();
        return mb;
    }

    MeshBuilder MeshBuilder::CreateIcosphere(float radius, uint32_t subdivisions)
    {
        MeshBuilder mb;
        const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;

        const glm::vec3 basePositions[12] =
        {
            glm::normalize(glm::vec3(-1.0f,  t,  0.0f)),
            glm::normalize(glm::vec3( 1.0f,  t,  0.0f)),
            glm::normalize(glm::vec3(-1.0f, -t,  0.0f)),
            glm::normalize(glm::vec3( 1.0f, -t,  0.0f)),
            glm::normalize(glm::vec3( 0.0f, -1.0f,  t)),
            glm::normalize(glm::vec3( 0.0f,  1.0f,  t)),
            glm::normalize(glm::vec3( 0.0f, -1.0f, -t)),
            glm::normalize(glm::vec3( 0.0f,  1.0f, -t)),
            glm::normalize(glm::vec3( t,  0.0f, -1.0f)),
            glm::normalize(glm::vec3( t,  0.0f,  1.0f)),
            glm::normalize(glm::vec3(-t,  0.0f, -1.0f)),
            glm::normalize(glm::vec3(-t,  0.0f,  1.0f))
        };

        for (const auto &n : basePositions)
        {
            const float u = std::atan2(n.z, n.x) / glm::two_pi<float>() + 0.5f;
            const float v = std::asin(glm::clamp(n.y, -1.0f, 1.0f)) / glm::pi<float>() + 0.5f;
            mb.AddVertex(n * radius, n, glm::vec2(u, v));
        }

        const uint32_t faces[20][3] =
        {
            {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
            {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
            {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
            {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
        };

        for (const auto &f : faces)
        {
            mb.AddTriangle(f[0], f[1], f[2]);
        }

        mb.Subdivide(std::min(subdivisions, 5u));
        for (auto &v : mb.GetVertices())
        {
            v.normal = glm::normalize(v.position);
            v.position = v.normal * radius;
            v.uv.x = std::atan2(v.normal.z, v.normal.x) / glm::two_pi<float>() + 0.5f;
            v.uv.y = std::asin(glm::clamp(v.normal.y, -1.0f, 1.0f)) / glm::pi<float>() + 0.5f;
        }
        mb.RecalculateTangents();
        return mb;
    }

    MeshBuilder MeshBuilder::CreateCylinder(float radius, float height, uint32_t sectors, bool capped)
    {
        MeshBuilder mb;
        sectors = std::max(3u, sectors);
        const float halfH = height * 0.5f;
        const float twoPi = glm::two_pi<float>();

        // Side wall
        for (uint32_t y = 0; y <= 1; ++y)
        {
            const float posY = (y == 0) ? halfH : -halfH;
            const float v = static_cast<float>(y);
            for (uint32_t s = 0; s <= sectors; ++s)
            {
                const float u = static_cast<float>(s) / static_cast<float>(sectors);
                const float theta = u * twoPi;
                const glm::vec3 n(std::cos(theta), 0.0f, std::sin(theta));
                mb.AddVertex(glm::vec3(n.x * radius, posY, n.z * radius), n, glm::vec2(u, v));
            }
        }

        const uint32_t stride = sectors + 1;
        for (uint32_t s = 0; s < sectors; ++s)
        {
            const uint32_t i0 = s;
            const uint32_t i1 = s + 1;
            const uint32_t i2 = stride + s + 1;
            const uint32_t i3 = stride + s;
            mb.AddQuad(i0, i1, i2, i3);
        }

        if (capped)
        {
            // Top cap (+Y)
            const uint32_t topCenter = mb.AddVertex(glm::vec3(0.0f, halfH, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.5f));
            for (uint32_t s = 0; s <= sectors; ++s)
            {
                const float theta = (static_cast<float>(s) / static_cast<float>(sectors)) * twoPi;
                const float c = std::cos(theta);
                const float sn = std::sin(theta);
                mb.AddVertex(glm::vec3(c * radius, halfH, sn * radius), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(c * 0.5f + 0.5f, sn * 0.5f + 0.5f));
            }
            for (uint32_t s = 0; s < sectors; ++s)
            {
                mb.AddTriangle(topCenter, topCenter + 1 + s + 1, topCenter + 1 + s);
            }

            // Bottom cap (-Y)
            const uint32_t botCenter = mb.AddVertex(glm::vec3(0.0f, -halfH, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.5f));
            for (uint32_t s = 0; s <= sectors; ++s)
            {
                const float theta = (static_cast<float>(s) / static_cast<float>(sectors)) * twoPi;
                const float c = std::cos(theta);
                const float sn = std::sin(theta);
                mb.AddVertex(glm::vec3(c * radius, -halfH, sn * radius), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(c * 0.5f + 0.5f, sn * 0.5f + 0.5f));
            }
            for (uint32_t s = 0; s < sectors; ++s)
            {
                mb.AddTriangle(botCenter, botCenter + 1 + s, botCenter + 1 + s + 1);
            }
        }

        mb.RecalculateTangents();
        return mb;
    }

    MeshBuilder MeshBuilder::CreateCapsule(float radius, float height, uint32_t rings, uint32_t sectors)
    {
        MeshBuilder mb;
        rings = std::max(4u, (rings / 2u) * 2u); // Ensure even number of hemisphere rings
        sectors = std::max(4u, sectors);

        const float cylinderHeight = std::max(0.0f, height - 2.0f * radius);
        const float halfCyl = cylinderHeight * 0.5f;
        const float pi = glm::pi<float>();
        const float twoPi = glm::two_pi<float>();
        const uint32_t halfRings = rings / 2u;

        // Top hemisphere + cylinder + bottom hemisphere
        const uint32_t totalRings = rings + 1;
        for (uint32_t r = 0; r <= totalRings; ++r)
        {
            float phi = 0.0f;
            float yOffset = 0.0f;
            if (r <= halfRings)
            {
                phi = (static_cast<float>(r) / static_cast<float>(rings)) * pi;
                yOffset = halfCyl;
            }
            else
            {
                phi = (static_cast<float>(r - 1) / static_cast<float>(rings)) * pi;
                yOffset = -halfCyl;
            }

            const float sinPhi = std::sin(phi);
            const float cosPhi = std::cos(phi);
            const float v = static_cast<float>(r) / static_cast<float>(totalRings);

            for (uint32_t s = 0; s <= sectors; ++s)
            {
                const float u = static_cast<float>(s) / static_cast<float>(sectors);
                const float theta = u * twoPi;
                const glm::vec3 n(sinPhi * std::cos(theta), cosPhi, sinPhi * std::sin(theta));
                const glm::vec3 pos = n * radius + glm::vec3(0.0f, yOffset, 0.0f);
                mb.AddVertex(pos, n, glm::vec2(u, v));
            }
        }

        const uint32_t stride = sectors + 1;
        for (uint32_t r = 0; r < totalRings; ++r)
        {
            for (uint32_t s = 0; s < sectors; ++s)
            {
                const uint32_t i0 = r * stride + s;
                const uint32_t i1 = r * stride + (s + 1);
                const uint32_t i2 = (r + 1) * stride + (s + 1);
                const uint32_t i3 = (r + 1) * stride + s;

                mb.AddQuad(i0, i1, i2, i3);
            }
        }

        mb.RecalculateTangents();
        return mb;
    }

    MeshBuilder MeshBuilder::CreatePlane(const glm::vec2 &size, uint32_t subdivisionsX, uint32_t subdivisionsZ)
    {
        MeshBuilder mb;
        subdivisionsX = std::max(1u, subdivisionsX);
        subdivisionsZ = std::max(1u, subdivisionsZ);

        const glm::vec2 half = size * 0.5f;
        const glm::vec3 normal(0.0f, 1.0f, 0.0f);

        for (uint32_t z = 0; z <= subdivisionsZ; ++z)
        {
            const float tz = static_cast<float>(z) / static_cast<float>(subdivisionsZ);
            const float posZ = glm::mix(-half.y, half.y, tz);

            for (uint32_t x = 0; x <= subdivisionsX; ++x)
            {
                const float tx = static_cast<float>(x) / static_cast<float>(subdivisionsX);
                const float posX = glm::mix(-half.x, half.x, tx);
                mb.AddVertex(glm::vec3(posX, 0.0f, posZ), normal, glm::vec2(tx, tz));
            }
        }

        const uint32_t stride = subdivisionsX + 1;
        for (uint32_t z = 0; z < subdivisionsZ; ++z)
        {
            for (uint32_t x = 0; x < subdivisionsX; ++x)
            {
                const uint32_t i0 = z * stride + x;
                const uint32_t i1 = z * stride + (x + 1);
                const uint32_t i2 = (z + 1) * stride + (x + 1);
                const uint32_t i3 = (z + 1) * stride + x;

                mb.AddTriangle(i0, i2, i1);
                mb.AddTriangle(i0, i3, i2);
            }
        }

        mb.RecalculateTangents();
        return mb;
    }

    MeshBuilder MeshBuilder::CreateCone(float radius, float height, uint32_t sectors, bool capped)
    {
        MeshBuilder mb;
        sectors = std::max(3u, sectors);
        const float halfH = height * 0.5f;
        const float twoPi = glm::two_pi<float>();
        const float slope = radius / std::max(height, 1e-4f);

        for (uint32_t s = 0; s <= sectors; ++s)
        {
            const float u = static_cast<float>(s) / static_cast<float>(sectors);
            const float theta = u * twoPi;
            const float c = std::cos(theta);
            const float sn = std::sin(theta);
            const glm::vec3 n = glm::normalize(glm::vec3(c, slope, sn));

            // Apex vertex
            mb.AddVertex(glm::vec3(0.0f, halfH, 0.0f), n, glm::vec2(u, 0.0f));
            // Base rim vertex
            mb.AddVertex(glm::vec3(c * radius, -halfH, sn * radius), n, glm::vec2(u, 1.0f));
        }

        for (uint32_t s = 0; s < sectors; ++s)
        {
            const uint32_t apex = s * 2;
            const uint32_t rim0 = s * 2 + 1;
            const uint32_t rim1 = (s + 1) * 2 + 1;
            mb.AddTriangle(apex, rim1, rim0);
        }

        if (capped)
        {
            const uint32_t botCenter = mb.AddVertex(glm::vec3(0.0f, -halfH, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.5f));
            for (uint32_t s = 0; s <= sectors; ++s)
            {
                const float theta = (static_cast<float>(s) / static_cast<float>(sectors)) * twoPi;
                const float c = std::cos(theta);
                const float sn = std::sin(theta);
                mb.AddVertex(glm::vec3(c * radius, -halfH, sn * radius), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(c * 0.5f + 0.5f, sn * 0.5f + 0.5f));
            }
            for (uint32_t s = 0; s < sectors; ++s)
            {
                mb.AddTriangle(botCenter, botCenter + 1 + s, botCenter + 1 + s + 1);
            }
        }

        mb.RecalculateTangents();
        return mb;
    }

    MeshBuilder MeshBuilder::CreateTorus(float majorRadius, float minorRadius, uint32_t majorSegments, uint32_t minorSegments)
    {
        MeshBuilder mb;
        majorSegments = std::max(3u, majorSegments);
        minorSegments = std::max(3u, minorSegments);
        const float twoPi = glm::two_pi<float>();

        for (uint32_t i = 0; i <= majorSegments; ++i)
        {
            const float u = static_cast<float>(i) / static_cast<float>(majorSegments);
            const float theta = u * twoPi;
            const float cosTheta = std::cos(theta);
            const float sinTheta = std::sin(theta);

            for (uint32_t j = 0; j <= minorSegments; ++j)
            {
                const float v = static_cast<float>(j) / static_cast<float>(minorSegments);
                const float phi = v * twoPi;
                const float cosPhi = std::cos(phi);
                const float sinPhi = std::sin(phi);

                const glm::vec3 n(cosTheta * cosPhi, sinPhi, sinTheta * cosPhi);
                const glm::vec3 pos(
                    (majorRadius + minorRadius * cosPhi) * cosTheta,
                    minorRadius * sinPhi,
                    (majorRadius + minorRadius * cosPhi) * sinTheta
                );
                mb.AddVertex(pos, n, glm::vec2(u, v));
            }
        }

        const uint32_t stride = minorSegments + 1;
        for (uint32_t i = 0; i < majorSegments; ++i)
        {
            for (uint32_t j = 0; j < minorSegments; ++j)
            {
                const uint32_t i0 = i * stride + j;
                const uint32_t i1 = (i + 1) * stride + j;
                const uint32_t i2 = (i + 1) * stride + (j + 1);
                const uint32_t i3 = i * stride + (j + 1);

                mb.AddTriangle(i0, i2, i1);
                mb.AddTriangle(i0, i3, i2);
            }
        }

        mb.RecalculateTangents();
        return mb;
    }
}
