// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_RENDERER_2D_HPP
#define IGN_RENDERER_2D_HPP

#include "ignite/core/base.hpp"
#include "batch_data.hpp"
#include "ignite/core/types.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/math/math.hpp"
#include "ignite/math/aabb.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/hash_keys.hpp"

#include <array>
#include <unordered_map>

namespace ignite
{
    constexpr uint32_t MAX_POINT_LIGHTS_2D = 32;

	class GraphicsPipeline;
	class DeviceManager;
	class ConstantBuffer;
	class Texture;
	class RenderTarget;
	class Font;
	class Project;
	class Sprite2DComponent;

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

    class IGN_API Renderer2D
    {
    public:
        Renderer2D();
        ~Renderer2D();

        void Begin(nvrhi::ICommandList *cmd);
        void Flush(nvrhi::IFramebuffer *framebuffer, const nvrhi::BufferHandle &cameraBuffer);
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

        void BuildPreRenderCache();
        bool ReplayPreRenderCache(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, const nvrhi::BufferHandle &cameraBuffer);
        void InvalidatePreRenderCache();

        void InitQuadData();
        void InitLineData();
        void InitCircleData();
        void InitTextData();

        void ClearPipelineCache();
        
        uint32_t GetOrInsertQuadTexture(const Ref<Texture> &texture);
        uint32_t GetOrInsertFontTexture(const Ref<Texture> &texture);
        
        static Ref<Renderer2D> Create();
        
    private:
        Ref<GraphicsPipeline> GetQuadPipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
        Ref<GraphicsPipeline> GetTextPipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
        Ref<GraphicsPipeline> GetCirclePipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
        Ref<GraphicsPipeline> GetLinePipelineForFB(nvrhi::IFramebuffer *framebuffer);

        nvrhi::BindingSetHandle GetQuadBindingSet(nvrhi::IBindingLayout *bindingLayout, const std::vector<Ref<Texture>> &textures, const nvrhi::BufferHandle &cameraBuffer, const nvrhi::BufferHandle &lightingBuffer);
        nvrhi::BindingSetHandle GetTextBindingSet(nvrhi::IBindingLayout *bindingLayout, const std::vector<Ref<Texture>> &textures, const nvrhi::BufferHandle &cameraBuffer, const nvrhi::BufferHandle &lightingBuffer);
		nvrhi::BindingSetHandle GetLineBindingSet(nvrhi::IBindingLayout *bindingLayout, const nvrhi::BufferHandle &cameraBuffer);
        nvrhi::BindingSetHandle GetCircleBindingSet(nvrhi::IBindingLayout *bindingLayout, const nvrhi::BufferHandle &cameraBuffer);

    private:
        nvrhi::ICommandList *m_Cmd;
        BatchRender<Vertex2DQuad> m_QuadBatch;
        BatchRender<Vertex2DLine> m_LineBatch;
        BatchRender<Vertex2DCircle> m_CircleBatch;
        BatchRender<VertexText> m_TextBatch;

        ConstantBuffer m_Material2DLightingBuffer;
        Material2DLighting_GPUData m_Material2DLightingData;

		std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_LinePSOCache;
		std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_QuadPSOCache;
		std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_CirclePSOCache;
		std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_TextPSOCache;

		std::unordered_map<CameraLightingBindingKey, nvrhi::BindingSetHandle, CameraLightingBindingKeyHash> m_QuadBindingSetCache;
		std::unordered_map<CameraLightingBindingKey, nvrhi::BindingSetHandle, CameraLightingBindingKeyHash> m_TextBindingSetCache;
		std::unordered_map<CameraBindingKey, nvrhi::BindingSetHandle, CameraBindingKeyHash> m_LineBindingSetCache;
		std::unordered_map<CameraBindingKey, nvrhi::BindingSetHandle, CameraBindingKeyHash> m_CircleBindingSetCache;

        struct PreRenderCacheData
        {
            Material2DLighting_GPUData lightingData;

            std::vector<Vertex2DCircle> circleVertices;
            uint32_t circleIndexCount = 0;

            std::vector<Vertex2DQuad> quadVertices;
            uint32_t quadIndexCount = 0;
            std::vector<Ref<Texture>> quadTextureSlots;

            std::vector<VertexText> textVertices;
            uint32_t textIndexCount = 0;
            std::vector<Ref<Texture>> textTextureSlots;

			bool valid = false;
        };

        Ref<PreRenderCacheData> m_PreRenderCache;

		nvrhi::RasterFillMode m_FillMode = nvrhi::RasterFillMode::Solid;
		bool m_Material2DLightingDirty = true;
    };
}

#endif
