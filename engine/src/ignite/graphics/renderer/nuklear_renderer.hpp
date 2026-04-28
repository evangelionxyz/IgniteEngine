// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef NUKLEAR_RENDERER_HPP
#define NUKLEAR_RENDERER_HPP

#ifndef NK_INCLUDE_VERTEX_BUFFER_OUTPUT
    #define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
    #define NK_UINT_DRAW_INDEX
#endif

#ifndef NK_INCLUDE_FONT_BAKING
    #define NK_INCLUDE_FONT_BAKING
    #define NK_INCLUDE_SOFTWARE_FONT
#endif

#ifndef NK_INCLUDE_DEFAULT_ALLOCATOR
    #define NK_INCLUDE_DEFAULT_ALLOCATOR
#endif

#ifndef NK_INCLUDE_DEFAULT_FONT
    #define NK_INCLUDE_DEFAULT_FONT
#endif

#ifndef NK_INCLUDE_STANDARD_IO
    #define NK_INCLUDE_STANDARD_IO
#endif


#include "nuklear.h"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>
#include "ignite/core/types.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/math/math.hpp"
#include <SDL3/SDL.h>
#include <unordered_map>
#include <vector>

namespace ignite
{
    class GraphicsPipeline;
    class Scene;
    class Project;
    class WidgetCanvas;

    struct NkDrawVertex
    {
        float position[2];
        float uv[2];
        float col[4];
    };

    class NuklearRenderer
    {
    public:
        NuklearRenderer();
        ~NuklearRenderer();

        void HandleEvent(SDL_Event *evt);
        void BeginNuklearFrame(const Ref<WidgetCanvas> &canvas, const Rect &parentRect);
        void SetScene(Scene *scene);
        void SetProject(Project *project);

        void Render(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);

    private:
        Scene *m_Scene = nullptr;
        Project *m_Project = nullptr;

        nvrhi::CommandListHandle m_NvrhiCmd;

        nvrhi::TextureHandle m_FontTexture;
        nvrhi::SamplerHandle m_FontSampler;

        nvrhi::BufferHandle m_VertexBuffer;
        nvrhi::BufferHandle m_IndexBuffer;

        nvrhi::BindingLayoutHandle m_BindingLayout;
        std::unordered_map<nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> m_PSOCache;
        std::unordered_map<nvrhi::ITexture *, nvrhi::BindingSetHandle> m_BindingSetCache;

        std::vector<NkDrawVertex> m_NkVertexBuffer;
        std::vector<nk_draw_index> m_NkIndexBuffer;

        bool UpdateFontTexture(nk_font_atlas *atlas);
        void BackBufferResizing();
        void InvalidateTextureCache(nvrhi::ITexture *texture);

    private:
        bool ReallocateBuffer(nvrhi::BufferHandle &buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer);
        Ref<GraphicsPipeline> GetPSO(nvrhi::IFramebuffer *framebuffer);
        nvrhi::IBindingSet *GetBindingSet(nvrhi::ITexture *texture, nvrhi::BindingLayoutHandle bindingLayout);
        bool UpdateGeometry(nvrhi::ICommandList *commandList, nk_buffer *cmds);

        nk_context m_Ctx;
        nk_font_atlas m_Atlas;
        nk_draw_null_texture m_NullTexture {};
    };
}

#endif
