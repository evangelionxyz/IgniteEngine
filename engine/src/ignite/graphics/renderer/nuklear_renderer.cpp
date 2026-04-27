// Copyright (c) 2026 Evangelion Manuhutu

#ifndef NK_IMPLEMENTATION
    #define NK_IMPLEMENTATION
#endif

#include "nuklear_renderer.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/project/project.hpp"
#include "ignite/graphics/ui/widget.hpp"
#include "ignite/graphics/ui/widget_container.hpp"
#include <SDL3/SDL_events.h>

#include <algorithm>

namespace ignite
{
    int nk_sdl_handle_event(nk_context *ctx, SDL_Event *evt)
    {
        int ctrl_down = SDL_GetModState() & SDL_KMOD_CTRL;
        static int insert_toggle = 0;

        switch (evt->type)
        {
            case SDL_EVENT_KEY_UP: /* KEYUP & KEYDOWN share same routine */
            case SDL_EVENT_KEY_DOWN:
            {
                int down = evt->type == SDL_EVENT_KEY_DOWN;
                switch (evt->key.key)
                {
                    case SDLK_RSHIFT: /* RSHIFT & LSHIFT share same routine */
                    case SDLK_LSHIFT:
                    nk_input_key(ctx, NK_KEY_SHIFT, down);
                    break;
                    case SDLK_DELETE:
                    nk_input_key(ctx, NK_KEY_DEL, down);
                    break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                    nk_input_key(ctx, NK_KEY_ENTER, down);
                    break;
                    case SDLK_TAB:
                    nk_input_key(ctx, NK_KEY_TAB, down);
                    break;
                    case SDLK_BACKSPACE:
                    nk_input_key(ctx, NK_KEY_BACKSPACE, down);
                    break;
                    case SDLK_HOME:
                    nk_input_key(ctx, NK_KEY_TEXT_START, down);
                    nk_input_key(ctx, NK_KEY_SCROLL_START, down);
                    break;
                    case SDLK_END:
                    nk_input_key(ctx, NK_KEY_TEXT_END, down);
                    nk_input_key(ctx, NK_KEY_SCROLL_END, down);
                    break;
                    case SDLK_PAGEDOWN:
                    nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down);
                    break;
                    case SDLK_PAGEUP:
                    nk_input_key(ctx, NK_KEY_SCROLL_UP, down);
                    break;
                    case SDLK_Z:
                    nk_input_key(ctx, NK_KEY_TEXT_UNDO, down && ctrl_down);
                    break;
                    case SDLK_R:
                    nk_input_key(ctx, NK_KEY_TEXT_REDO, down && ctrl_down);
                    break;
                    case SDLK_C:
                    nk_input_key(ctx, NK_KEY_COPY, down && ctrl_down);
                    break;
                    case SDLK_V:
                    nk_input_key(ctx, NK_KEY_PASTE, down && ctrl_down);
                    break;
                    case SDLK_X:
                    nk_input_key(ctx, NK_KEY_CUT, down && ctrl_down);
                    break;
                    case SDLK_B:
                    nk_input_key(ctx, NK_KEY_TEXT_LINE_START, down && ctrl_down);
                    break;
                    case SDLK_E:
                    nk_input_key(ctx, NK_KEY_TEXT_LINE_END, down && ctrl_down);
                    break;
                    case SDLK_UP:
                    nk_input_key(ctx, NK_KEY_UP, down);
                    break;
                    case SDLK_DOWN:
                    nk_input_key(ctx, NK_KEY_DOWN, down);
                    break;
                    case SDLK_ESCAPE:
                    nk_input_key(ctx, NK_KEY_TEXT_RESET_MODE, down);
                    break;
                    case SDLK_INSERT:
                    if (down) insert_toggle = !insert_toggle;
                    if (insert_toggle)
                    {
                        nk_input_key(ctx, NK_KEY_TEXT_INSERT_MODE, down);
                    }
                    else
                    {
                        nk_input_key(ctx, NK_KEY_TEXT_REPLACE_MODE, down);
                    }
                    break;
                    case SDLK_A:
                    if (ctrl_down)
                        nk_input_key(ctx, NK_KEY_TEXT_SELECT_ALL, down);
                    break;
                    case SDLK_LEFT:
                    if (ctrl_down)
                        nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT, down);
                    else
                        nk_input_key(ctx, NK_KEY_LEFT, down);
                    break;
                    case SDLK_RIGHT:
                    if (ctrl_down)
                        nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, down);
                    else
                        nk_input_key(ctx, NK_KEY_RIGHT, down);
                    break;
                }
                return 1;
            }

            case SDL_EVENT_MOUSE_BUTTON_UP: /* MOUSEBUTTONUP & MOUSEBUTTONDOWN share same routine */
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
                int down = evt->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                const int x = evt->button.x, y = evt->button.y;
                switch (evt->button.button)
                {
                    case SDL_BUTTON_LEFT:
                    if (evt->button.clicks > 1)
                    {
                        nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, down);
                    }
                    nk_input_button(ctx, NK_BUTTON_LEFT, x, y, down);
                    break;
                    case SDL_BUTTON_MIDDLE:
                    nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, down);
                    break;
                    case SDL_BUTTON_RIGHT:
                    nk_input_button(ctx, NK_BUTTON_RIGHT, x, y, down);
                    break;
                }
                return 1;
            }

            case SDL_EVENT_MOUSE_MOTION:
            {
                nk_input_motion(ctx, evt->motion.x, evt->motion.y);
                return 1;
            }

            case SDL_EVENT_TEXT_INPUT:
            {
                nk_glyph glyph;
                memcpy(glyph, evt->text.text, NK_UTF_SIZE);
                nk_input_glyph(ctx, glyph);
                return 1;
            }

            case SDL_EVENT_MOUSE_WHEEL:
            nk_input_scroll(ctx, nk_vec2(evt->wheel.x, evt->wheel.y));
            return 1;
        }
        return 0;
    }
    
    struct NuklearPushConstants
    {
        float invDisplaySize[2];
        float displayPos[2];
    };

    NuklearRenderer::NuklearRenderer()
    {
        nk_init_default(&m_Ctx, nullptr);

        nk_font_atlas_init_default(&m_Atlas);
        nk_font_atlas_begin(&m_Atlas);

        // Default font
        nk_font *font = nk_font_atlas_add_default(&m_Atlas, 13.0f, nullptr);

        m_Ctx.style.font = &font->handle;
        m_Ctx.style.button.rounding = 8.0f;
        m_Ctx.style.window.padding.x = 0.0f;
        m_Ctx.style.window.padding.y = 0.0f;

        auto device = DeviceManager::GetInstance()->GetDevice();
        m_NvrhiCmd = device->createCommandList(nvrhi::CommandListParameters()
            .setEnableImmediateExecution(false)
            .setQueueType(nvrhi::CommandQueue::Graphics));

        const auto desc = nvrhi::SamplerDesc()
            .setAllAddressModes(nvrhi::SamplerAddressMode::Wrap)
            .setAllFilters(true);

        m_FontSampler = device->createSampler(desc);
        LOG_ASSERT(m_FontSampler, "Failed to create Nuklear font sampler");

        UpdateFontTexture(&m_Atlas);
    }

    NuklearRenderer::~NuklearRenderer()
    {
        // Shutdown
        m_BindingSetCache.clear();
        m_PSOCache.clear();
        m_BindingLayout = nullptr;

        m_FontTexture = nullptr;
        m_FontSampler = nullptr;

        m_VertexBuffer = nullptr;
        m_IndexBuffer = nullptr;

        m_NvrhiCmd = nullptr;

        // Clear
        nk_font_atlas_clear(&m_Atlas);
        nk_free(&m_Ctx);
    }

    void NuklearRenderer::HandleEvent(SDL_Event *evt)
    {
        nk_sdl_handle_event(&m_Ctx, evt);
    }

    void NuklearRenderer::SetScene(Scene *scene)
    {
        m_Scene = scene;
    }

    void NuklearRenderer::SetProject(Project *project)
    {
        m_Project = project;
    }

    static void RenderWidgetItems(nk_context *ctx, const std::vector<Ref<IWidgetItem>> &items)
    {
        for (const auto &item : items)
        {
            if (!item || !item->IsVisible())
                continue;

            switch (item->GetWidgetType())
            {
                case WidgetType::Button:
                {
                    auto button = std::static_pointer_cast<WidgetButton>(item);
                    nk_layout_row_static(ctx, button->size.y, (int)button->size.x, 1);
                    
                    nk_style_button bt_style = ctx->style.button;
                    bt_style.normal.data.color = nk_rgba_f(button->normalColor.r, button->normalColor.g, button->normalColor.b, button->normalColor.a);
                    bt_style.hover.data.color = nk_rgba_f(button->hoverColor.r, button->hoverColor.g, button->hoverColor.b, button->hoverColor.a);
                    bt_style.active.data.color = nk_rgba_f(button->pressedColor.r, button->pressedColor.g, button->pressedColor.b, button->pressedColor.a);
                    if (nk_button_text_styled(ctx, &bt_style, button->GetText().c_str(), (int)button->GetText().size()))
                    {
                        glm::uvec2 hitPos = glm::vec2(item->position.x + item->size.x * 0.5f, item->position.y + item->size.y * 0.5f);
                        button->OnMouseClick(hitPos, true);
                        button->OnMouseClick(hitPos, false);
                    }

                    break;
                }
                case WidgetType::Label:
                {
                    auto label = std::static_pointer_cast<WidgetLabel>(item);
                    const auto &col = label->color;
                    nk_layout_row_dynamic(ctx, label->fontSize, 1);
                    nk_label_colored(ctx, label->GetText().c_str(), NK_TEXT_LEFT, nk_rgba_f(col.r, col.g, col.b, col.a));
                    break;
                }
                case WidgetType::Container:
                {
                    auto container = std::static_pointer_cast<WidgetContainer>(item);
                    if (nk_group_begin(ctx, container->name.c_str(), NK_WINDOW_NO_SCROLLBAR))
                    {
                        RenderWidgetItems(ctx, container->children);
                        nk_group_end(ctx);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    void NuklearRenderer::BeginNuklearFrame(const Ref<WidgetCanvas> &canvas, const Rect &parentRect)
    {
        if (!canvas)
            return;

        IGN_PROFILE_FUNCTION();
        nk_input_end(&m_Ctx);

        canvas->GetRoot()->Measure();
        canvas->GetRoot()->Arrange(parentRect);

        // Find root containers
        std::vector<Ref<IWidgetItem>> rootItems;
        for (const auto &[id, widgetItem] : canvas->GetItems())
        {
            if (!widgetItem || !widgetItem->IsVisible() || widgetItem->parent)
                continue;

            rootItems.push_back(widgetItem);
        }

        // Stable sort by Z-Index
        std::stable_sort(rootItems.begin(), rootItems.end(), [](const Ref<IWidgetItem> &a, const Ref<IWidgetItem> &b) { return a->zIndex < b->zIndex; });
        for (const auto &rootItem : rootItems)
        {
            if (rootItem->GetWidgetType() == WidgetType::Container)
            {
                const nk_flags windowFlags = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_NO_SCROLLBAR;
                if (nk_begin(&m_Ctx, rootItem->name.c_str(), nk_rect(rootItem->position.x, rootItem->position.y, rootItem->size.x, rootItem->size.y), windowFlags))
                {
                    RenderWidgetItems(&m_Ctx, rootItem->children);
                }
                nk_end(&m_Ctx);
                nk_window_set_bounds(&m_Ctx, rootItem->name.c_str(), nk_rect(rootItem->position.x, rootItem->position.y, rootItem->size.x, rootItem->size.y));
            }
        }
        nk_input_begin(&m_Ctx);
    }

    void NuklearRenderer::Render(nvrhi::ICommandList *nvrhiCmd, nvrhi::IFramebuffer *framebuffer)
    {
        IGN_PROFILE_FUNCTION();

        if (!&m_Ctx)
            return;

        const auto fbInfo = framebuffer->getFramebufferInfo();
        int fbWidth = fbInfo.width;
        int fbHeight = fbInfo.height;

        if (fbWidth <= 0 || fbHeight <= 0)
            return;

        nvrhiCmd->beginMarker("Nuklear");

        nk_buffer nkCmds;
        nk_buffer_init_default(&nkCmds);

        if (!UpdateGeometry(nvrhiCmd, &nkCmds))
        {
            nk_buffer_free(&nkCmds);
            nvrhiCmd->close();
            return;
        }

        NuklearPushConstants pushConstants = {};
        pushConstants.invDisplaySize[0] = 1.0f / static_cast<float>(fbWidth);
        pushConstants.invDisplaySize[1] = 1.0f / static_cast<float>(fbHeight);
        pushConstants.displayPos[0] = 0.0f;
        pushConstants.displayPos[1] = 0.0f;

        nvrhi::GraphicsState drawState;
        drawState.framebuffer = framebuffer;
        LOG_ASSERT(drawState.framebuffer, "Invalid framebuffer");

        Ref<GraphicsPipeline> pipeline = GetPSO(drawState.framebuffer);
        drawState.pipeline = pipeline->GetHandle();

        drawState.viewport.viewports.push_back(
            nvrhi::Viewport(static_cast<float>(fbWidth), static_cast<float>(fbHeight))
        );
        drawState.viewport.scissorRects.resize(1);

        drawState.vertexBuffers = { { m_VertexBuffer, 0, 0 } };
        drawState.indexBuffer.buffer = m_IndexBuffer;
        drawState.indexBuffer.format = sizeof(nk_draw_index) == 2 ? nvrhi::Format::R16_UINT : nvrhi::Format::R32_UINT;
        drawState.indexBuffer.offset = 0;

        const nvrhi::BindingLayoutHandle bindingLayout = pipeline->GetBindingLayout(0);
        drawState.bindings.resize(1);

        nvrhi::ITexture *lastTexture = nullptr;
        nvrhi::IBindingSet *lastBindingSet = nullptr;

        const nk_draw_command *nkCmd = nullptr;
        uint32_t offset = 0;

        nk_draw_foreach(nkCmd, &m_Ctx, &nkCmds)
        {
            if (!nkCmd->elem_count)
                continue;

            auto texture = static_cast<nvrhi::ITexture *>(nkCmd->texture.ptr);
            if (!texture)
                texture = m_FontTexture.Get();

            if (texture != lastTexture)
            {
                lastTexture = texture;
                lastBindingSet = GetBindingSet(texture, bindingLayout);
            }

            drawState.bindings[0] = lastBindingSet;
            LOG_ASSERT(drawState.bindings[0], "Invalid draw state binding");

            auto clipMinMax = nkCmd->clip_rect;
            if (clipMinMax.x < 0.0f) clipMinMax.x = 0.0f;
            if (clipMinMax.y < 0.0f) clipMinMax.y = 0.0f;
            if (clipMinMax.w > static_cast<float>(fbWidth)) clipMinMax.w = static_cast<float>(fbWidth);
            if (clipMinMax.h > static_cast<float>(fbHeight)) clipMinMax.h = static_cast<float>(fbHeight);

            if (clipMinMax.w <= clipMinMax.x || clipMinMax.h <= clipMinMax.y)
            {
                offset += nkCmd->elem_count;
                continue;
            }

            drawState.viewport.scissorRects[0] = nvrhi::Rect(
                int(clipMinMax.x),
                int(clipMinMax.w),
                int(clipMinMax.y),
                int(clipMinMax.h)
            );

            nvrhi::DrawArguments drawArguments;
            drawArguments.vertexCount = nkCmd->elem_count;
            drawArguments.startVertexLocation = 0;
            drawArguments.startIndexLocation = offset;

            nvrhiCmd->setGraphicsState(drawState);
            nvrhiCmd->setPushConstants(&pushConstants, sizeof(pushConstants));
            nvrhiCmd->drawIndexed(drawArguments);

            offset += nkCmd->elem_count;
        }

        nvrhiCmd->endMarker();

        nk_buffer_free(&nkCmds);
        nk_clear(&m_Ctx);
    }

    bool NuklearRenderer::UpdateFontTexture(nk_font_atlas *atlas)
    {
        IGN_PROFILE_FUNCTION();

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        if (m_FontTexture)
            return true;

        int width, height;
        const void *pixels = nk_font_atlas_bake(atlas, &width, &height, NK_FONT_ATLAS_RGBA32);
        if (!pixels)
            return false;

        nvrhi::TextureDesc textureDesc;
        textureDesc.width = width;
        textureDesc.height = height;
        textureDesc.format = nvrhi::Format::RGBA8_UNORM;
        textureDesc.debugName = "Nuklear font texture";

        m_FontTexture = device->createTexture(textureDesc);
        LOG_ASSERT(m_FontTexture, "Failed to create Nuklear font texture");

        m_NvrhiCmd->open();
        m_NvrhiCmd->beginTrackingTextureState(m_FontTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);
        m_NvrhiCmd->writeTexture(m_FontTexture, 0, 0, pixels, width * 4);
        m_NvrhiCmd->setPermanentTextureState(m_FontTexture, nvrhi::ResourceStates::ShaderResource);
        m_NvrhiCmd->commitBarriers();
        m_NvrhiCmd->close();

        {
            std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());
            device->executeCommandList(m_NvrhiCmd);
        }

        nk_font_atlas_end(atlas, nk_handle_ptr(m_FontTexture.Get()), nullptr);
        return true;
    }

    void NuklearRenderer::BackBufferResizing()
    {
        m_PSOCache.clear();
        m_BindingSetCache.clear();
    }

    bool NuklearRenderer::ReallocateBuffer(nvrhi::BufferHandle &buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer)
    {
        if (buffer == nullptr || size_t(buffer->getDesc().byteSize) < requiredSize)
        {
            nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

            nvrhi::BufferDesc desc;
            desc.byteSize = static_cast<u32>(reallocateSize);
            desc.debugName = isIndexBuffer ? "Nuklear index buffer" : "Nuklear vertex buffer";
            desc.canHaveUAVs = false;
            desc.isVertexBuffer = !isIndexBuffer;
            desc.isIndexBuffer = isIndexBuffer;
            desc.isDrawIndirectArgs = false;
            desc.isVolatile = false;
            desc.initialState = isIndexBuffer ? nvrhi::ResourceStates::IndexBuffer : nvrhi::ResourceStates::VertexBuffer;
            desc.keepInitialState = true;

            buffer = device->createBuffer(desc);

            if (!buffer)
                return false;
        }

        return true;
    }

    Ref<GraphicsPipeline> NuklearRenderer::GetPSO(nvrhi::IFramebuffer *framebuffer)
    {
        IGN_PROFILE_FUNCTION();

        auto pipelineIter = m_PSOCache.find(framebuffer);
        if (pipelineIter != m_PSOCache.end())
        {
            return pipelineIter->second;
        }

        if (!m_BindingLayout)
        {
            nvrhi::BindingLayoutDesc layoutDesc;
            layoutDesc.visibility = nvrhi::ShaderType::All;
            layoutDesc.bindings =
            {
                nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float) * 4),
                nvrhi::BindingLayoutItem::Texture_SRV(0),
                nvrhi::BindingLayoutItem::Sampler(0)
            };

            nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
            m_BindingLayout = device->createBindingLayout(layoutDesc);
        }

        auto vertexShader = Shader::Create("resources/shaders/nuklear.vertex.hlsl", ShaderType::Vertex, true);
        auto pixelShader = Shader::Create("resources/shaders/nuklear.pixel.hlsl", ShaderType::Pixel, true);

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.enableDepthWrite = false;
        params.enableDepthTest = false;
        params.enableDepthStencil = false;
        params.fillMode = nvrhi::RasterFillMode::Solid;
        params.cullMode = nvrhi::RasterCullMode::None;
        params.depthFunc = nvrhi::ComparisonFunc::Always;

        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create();
        pipeline->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(m_BindingLayout)
            .Build(framebuffer, params);

        m_PSOCache[framebuffer] = pipeline;
        return pipeline;
    }

    nvrhi::IBindingSet *NuklearRenderer::GetBindingSet(nvrhi::ITexture *texture, nvrhi::BindingLayoutHandle bindingLayout)
    {
        IGN_PROFILE_FUNCTION();

        auto iter = m_BindingSetCache.find(texture);
        if (iter != m_BindingSetCache.end())
            return iter->second;

        nvrhi::BindingSetDesc desc;
        desc.bindings =
        {
            nvrhi::BindingSetItem::PushConstants(0, sizeof(float) * 4),
            nvrhi::BindingSetItem::Texture_SRV(0, texture),
            nvrhi::BindingSetItem::Sampler(0, m_FontSampler)
        };

        nvrhi::BindingSetHandle binding;

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        binding = device->createBindingSet(desc, bindingLayout);
        LOG_ASSERT(binding, "Failed to create Nuklear binding set");

        m_BindingSetCache[texture] = binding;
        return binding;
    }

    void NuklearRenderer::InvalidateTextureCache(nvrhi::ITexture *texture)
    {
        if (!texture)
            return;

        m_BindingSetCache.erase(texture);
    }

    bool NuklearRenderer::UpdateGeometry(nvrhi::ICommandList *commandList, nk_buffer *cmds)
    {
        IGN_PROFILE_FUNCTION();

        static const nk_draw_vertex_layout_element vertex_layout[] =
        {
            {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(NkDrawVertex, position)},
            {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(NkDrawVertex, uv)},
            {NK_VERTEX_COLOR, NK_FORMAT_R32G32B32A32_FLOAT, NK_OFFSETOF(NkDrawVertex, col)},
            {NK_VERTEX_LAYOUT_END}
        };

        nk_buffer vbuf, ibuf;
        nk_buffer_init_default(&vbuf);
        nk_buffer_init_default(&ibuf);

        nk_convert_config config;
        memset(&config, 0, sizeof(config));
        config.vertex_layout = vertex_layout;
        config.vertex_size = sizeof(NkDrawVertex);
        config.vertex_alignment = NK_ALIGNOF(NkDrawVertex);
        
        config.global_alpha = 1.0f;
        config.shape_AA = NK_ANTI_ALIASING_ON;
        config.line_AA = NK_ANTI_ALIASING_ON;
        config.circle_segment_count = 22;
        config.curve_segment_count = 22;
        config.arc_segment_count = 22;

        nk_convert(&m_Ctx, cmds, &vbuf, &ibuf, &config);

        const void *vertices = nk_buffer_memory_const(&vbuf);
        const void *indices = nk_buffer_memory_const(&ibuf);

        size_t vtxCount = nk_buffer_total(&vbuf) / sizeof(NkDrawVertex);
        size_t idxCount = nk_buffer_total(&ibuf) / sizeof(nk_draw_index);

        if (vtxCount == 0 || idxCount == 0)
        {
            nk_buffer_free(&vbuf);
            nk_buffer_free(&ibuf);
            return true;
        }

        size_t expandedVertexSize = vtxCount * sizeof(NkDrawVertex);

        if (!ReallocateBuffer(m_VertexBuffer, expandedVertexSize, (vtxCount + 5000) * sizeof(NkDrawVertex), false))
        {
            nk_buffer_free(&vbuf);
            nk_buffer_free(&ibuf);
            return false;
        }

        if (!ReallocateBuffer(m_IndexBuffer, idxCount * sizeof(nk_draw_index), (idxCount + 5000) * sizeof(nk_draw_index), true))
        {
            nk_buffer_free(&vbuf);
            nk_buffer_free(&ibuf);
            return false;
        }

        m_NkVertexBuffer.resize(m_VertexBuffer->getDesc().byteSize / sizeof(NkDrawVertex));
        m_NkIndexBuffer.resize(m_IndexBuffer->getDesc().byteSize / sizeof(nk_draw_index));

        auto srcVtx = static_cast<const NkDrawVertex *>(vertices);
        for (size_t i = 0; i < vtxCount; ++i)
        {
            NkDrawVertex &dst = m_NkVertexBuffer[i];
            dst.position[0] = srcVtx[i].position[0];
            dst.position[1] = srcVtx[i].position[1];
            dst.uv[0] = srcVtx[i].uv[0];
            dst.uv[1] = srcVtx[i].uv[1];
            
            dst.col[0] = srcVtx[i].col[0];
            dst.col[1] = srcVtx[i].col[1];
            dst.col[2] = srcVtx[i].col[2];
            dst.col[3] = srcVtx[i].col[3];
        }

        memcpy(m_NkIndexBuffer.data(), indices, idxCount * sizeof(nk_draw_index));

        commandList->writeBuffer(m_VertexBuffer, m_NkVertexBuffer.data(), vtxCount * sizeof(NkDrawVertex));
        commandList->writeBuffer(m_IndexBuffer, m_NkIndexBuffer.data(), idxCount * sizeof(nk_draw_index));

        nk_buffer_free(&vbuf);
        nk_buffer_free(&ibuf);

        return true;
    }
}
