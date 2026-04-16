// Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO

#ifndef RENDERER_2D_HPP
#define RENDERER_2D_HPP

#include "ignite/core/types.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/math/math.hpp"

#include "ignite/math/aabb.hpp"

#include "ignite/graphics/buffers/vertex_buffer.hpp"
#include "ignite/graphics/buffers/index_buffer.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/objects/material_2d.hpp"

#include <array>
#include <unordered_map>

namespace ignite
{
    constexpr uint32_t MAX_POINT_LIGHTS_2D = 32;

    struct PointLight2D_GPUData
    {
        glm::vec4 position = glm::vec4(0.0f);
        glm::vec4 color = glm::vec4(1.0f);
        float radius = 0.0f;
        float intensity = 1.0f;
        glm::vec2 _padding = glm::vec2(0.0f);
    };

    struct Material2DLighting_GPUData
    {
        uint32_t pointLightCount = 0;
        glm::vec3 _padding = glm::vec3(0.0f);
        std::array<PointLight2D_GPUData, MAX_POINT_LIGHTS_2D> pointLights;
    };

    class GraphicsPipeline;
    class DeviceManager;
    class Texture;
    class RenderTarget;
    class Font;
    class Project;

    class Sprite2DComponent;

    template<typename VertexType>
    struct BatchRender
    {
        uint32_t minCount = 256;
        uint32_t maxCount = minCount;
        uint32_t verticesPerObject = 4;
        uint32_t indicesPerObject = 6;
        uint32_t maxVertices = maxCount * verticesPerObject;
        uint32_t maxIndices = maxCount * indicesPerObject;
        uint32_t lowUsageFrames = 0;
        uint8_t textureSlotIndex = 1; // 0 for white texture
        uint32_t indexCount = 0;
        uint32_t count = 0;

        VertexType* vertexBufferBase = nullptr;
        VertexType*vertexBufferPtr = nullptr;
        Ref<VertexBuffer> vertexBuffer;
        Ref<IndexBuffer> indexBuffer;
        std::vector<Ref<Texture>> textureSlots;

        ~BatchRender()
        {
            vertexBufferPtr = nullptr;
            delete[] vertexBufferBase;
        }
    };

    class Renderer2D
    {
    public:
        Renderer2D();
        ~Renderer2D();

        void Begin(nvrhi::ICommandList *cmd);
        void Flush(nvrhi::IFramebuffer *framebuffer, const Ref<ConstantBuffer> &cameraBuffer);
        void End();

        void SetFillMode(nvrhi::RasterFillMode mode) { m_FillMode = mode; ClearPipelineCache(); }

        void DrawBox(const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));
        void DrawRect(const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));
        void DrawLine(const std::vector<glm::vec3>& positions, const glm::vec4& color = glm::vec4(1.0f));
        void DrawLine(const glm::vec3 &pos0, const glm::vec3 &pos1, const glm::vec4& color = glm::vec4(1.0f));
        void DrawAABB(const AABB& aabb, const glm::vec4& color = glm::vec4(1.0f));

        void DrawCircle(const glm::vec3& position, const glm::vec3 &scale, const glm::vec4& color = glm::vec4(1.0f), float thickness = -1.0f, float fade = 0.005f, uint32_t objectID = 0xFFFFFFFFu);
        void DrawCircle(const glm::mat4 &transform, const glm::vec4 &color = glm::vec4(1.0f), float thickness = -1.0f, float fade = 0.005f, uint32_t objectID = 0xFFFFFFFFu);

        void DrawQuad(const Rect &rect, float rotation, const glm::vec4 &color, const Ref<Texture> &texture, const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &tilingFactor = glm::vec2(1.0f), uint32_t objectID = 0xFFFFFFFFu);
        void DrawQuad(const glm::vec3 &position, const glm::vec2 &size, f32 rotation, const glm::vec4 &color, const Ref<Texture>& texture, const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &tilingFactor = glm::vec2(1.0f), uint32_t objectID = 0xFFFFFFFFu);
        void DrawQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, const Ref<Texture>& texture, const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &tilingFactor = glm::vec2(1.0f), uint32_t objectID = 0xFFFFFFFFu);
        void DrawQuad(const glm::mat4 &transform, const glm::vec4 &color, const Ref<Texture>& texture, const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &tilingFactor = glm::vec2(1.0f), uint32_t objectID = 0xFFFFFFFFu);

        void DrawQuad(const glm::mat4 &transform, const glm::vec4 &color, const glm::vec4 &additiveColor, Material2DType materialType, const Ref<Texture> &texture, 
            const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &tilingFactor = glm::vec2(1.0f), uint32_t objectID = 0xFFFFFFFFu);

        void SetPointLights2D(const std::vector<PointLight2D_GPUData> &pointLights);

        void DrawString(const std::string &str, const Ref<Font> &font, const glm::vec4 &color, const glm::mat4 &transform, float kerning, float linespacing, uint32_t objectID = 0xFFFFFFFFu);

        Ref<Texture> ResolveTexture(Project *project, AssetHandle handle);
        Ref<Material2D> ResolveMaterial2D(Project *project, AssetHandle handle);
        void ClearAssetResolveCache();

        void BuildPreRenderCache();
        bool ReplayPreRenderCache(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, const Ref<ConstantBuffer> &cameraBuffer);
        void InvalidatePreRenderCache();

        void InitQuadData();
        void InitLineData();
        void InitCircleData();
        void InitTextData();

        void ClearPipelineCache();
        
        uint32_t GetOrInsertQuadTexture(const Ref<Texture>& texture);
        uint32_t GetOrInsertFontTexture(const Ref<Texture>& texture);
        
        static Ref<Renderer2D> Create();
        
    private:
        nvrhi::ICommandList *m_Cmd;
        BatchRender<Vertex2DQuad> m_QuadBatch;
        BatchRender<Vertex2DLine> m_LineBatch;
        BatchRender<Vertex2DCircle> m_CircleBatch;
        BatchRender<VertexText> m_TextBatch;

        nvrhi::RasterFillMode m_FillMode = nvrhi::RasterFillMode::Solid;

        Ref<ConstantBuffer> m_Material2DLightingBuffer;
        Material2DLighting_GPUData m_Material2DLightingData;
        bool m_Material2DLightingDirty = true;

        struct AssetResolveKey
        {
            Project *project = nullptr;
            AssetHandle handle = AssetHandle(0);

            bool operator==(const AssetResolveKey &other) const noexcept
            {
                return project == other.project && handle == other.handle;
            }
        };

        struct AssetResolveKeyHash
        {
            size_t operator()(const AssetResolveKey &key) const noexcept
            {
                size_t h = std::hash<const void *>{}(key.project);
                h ^= (std::hash<AssetHandle>{}(key.handle) + 0x9e3779b9 + (h << 6) + (h >> 2));
                return h;
            }
        };

        std::unordered_map<AssetResolveKey, Ref<Texture>, AssetResolveKeyHash> m_TextureResolveCache;
        std::unordered_map<AssetResolveKey, Ref<Material2D>, AssetResolveKeyHash> m_Material2DResolveCache;

        struct PreRenderCacheData
        {
            bool valid = false;

            Material2DLighting_GPUData lightingData;

            std::vector<Vertex2DCircle> circleVertices;
            uint32_t circleIndexCount = 0;

            std::vector<Vertex2DQuad> quadVertices;
            uint32_t quadIndexCount = 0;
            std::vector<Ref<Texture>> quadTextureSlots;

            std::vector<VertexText> textVertices;
            uint32_t textIndexCount = 0;
            std::vector<Ref<Texture>> textTextureSlots;
        };

        PreRenderCacheData m_PreRenderCache;
    };
}

#endif
