// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "widget_renderer.hpp"

#include "widget_canvas.hpp"
#include "widget_container.hpp"
#include "widget_button.hpp"
#include "widget_label.hpp"
#include "widget_image.hpp"

#include "ignite/core/input/input.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/framebuffer_key.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/font.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/scene.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <unordered_set>
#include <limits>

namespace ignite
{
    namespace
    {
        static std::vector<Ref<IWidgetItem>> GetSortedVisibleChildren(const std::vector<Ref<IWidgetItem>> &children)
        {
            std::vector<Ref<IWidgetItem>> sorted;
            sorted.reserve(children.size());
            for (const Ref<IWidgetItem> &child : children)
            {
                if (child && child->IsVisible())
                {
                    sorted.push_back(child);
                }
            }

            std::stable_sort(sorted.begin(), sorted.end(), [](const Ref<IWidgetItem> &a, const Ref<IWidgetItem> &b)
            {
                return a->zIndex < b->zIndex;
            });

            return sorted;
        }

        static void MeasureRecursive(const Ref<IWidgetItem> &item)
        {
            if (!item || !item->IsVisible())
            {
                return;
            }

            for (const Ref<IWidgetItem> &child : item->children)
            {
                MeasureRecursive(child);
            }

            item->Measure();
        }

        static void ResolveWidgetAssetsRecursive(const Ref<IWidgetItem> &item, AssetManager *assetManager)
        {
            if (!item || !assetManager)
            {
                return;
            }

            if (Ref<WidgetButton> button = item->As<WidgetButton>())
            {
                if (button->imageHandle != AssetHandle(0))
                {
                    button->image = assetManager->GetAsset<Texture>(button->imageHandle);
                    if (!button->image)
                        return;
                }
                else
                {
                    button->image = nullptr;
                }

                if (button->label)
                {
                    if (button->label->fontHandle != AssetHandle(0))
                    {
                        button->label->font = assetManager->GetAsset<Font>(button->label->fontHandle);
                        if (!button->label->font)
                            return;
                    }
                    else
                    {
                        button->label->font = nullptr;
                    }
                }
            }
            else if (Ref<WidgetLabel> label = item->As<WidgetLabel>())
            {
                if (label->fontHandle != AssetHandle(0))
                {
                    label->font = assetManager->GetAsset<Font>(label->fontHandle);
                    if (!label->font)
                        return;
                }
                else
                {
                    label->font = nullptr;
                }
            }
            else if (Ref<WidgetImage> img = item->As<WidgetImage>())
            {
                img->image = img->imageHandle != AssetHandle(0)
                    ? assetManager->GetAsset<Texture>(img->imageHandle)
                    : nullptr;
            }

            for (const Ref<IWidgetItem> &child : item->children)
            {
                ResolveWidgetAssetsRecursive(child, assetManager);
            }
        }

        static void UpdateRecursive(const Ref<IWidgetItem> &item, const glm::uvec2 &mousePos, bool isMousePressed)
        {
            if (!item || !item->IsVisible())
            {
                return;
            }

            if (Ref<WidgetButton> button = item->As<WidgetButton>())
            {
                button->OnMouseClick(mousePos, isMousePressed);
            }

            for (const Ref<IWidgetItem> &child : item->children)
            {
                UpdateRecursive(child, mousePos, isMousePressed);
            }
        }

        static void DrawRecursive(WidgetRenderer *renderer, const Ref<IWidgetItem> &item)
        {
            if (!renderer || !item || !item->IsVisible())
            {
                return;
            }

            if (Ref<WidgetButton> button = item->As<WidgetButton>())
            {
                renderer->DrawQuad(button->GetAlignedRect(), 0.0f, button->GetCurrentColor(), button->image, glm::vec2(0.0f), glm::vec2(1.0f));

                if (button->label && button->label->font && button->label->font->IsReady())
                {
                    const Rect textBounds = button->label->GetTextBounds();
                    const glm::vec2 textSize = textBounds.GetSize();
                    const glm::vec2 textPos =
                    {
                        button->GetAlignedRect().min.x + std::max((button->GetAlignedRect().GetSize().x - textSize.x) * 0.5f, 0.0f) - textBounds.min.x,
                        button->GetAlignedRect().min.y + std::max((button->GetAlignedRect().GetSize().y - textSize.y) * 0.5f, 0.0f) - textBounds.min.y
                    };

                    const glm::mat4 textTransform = glm::translate(glm::mat4(1.0f), glm::vec3(textPos, 0.0f)) *
                        glm::scale(glm::mat4(1.0f), { button->label->style.fontSize, button->label->style.fontSize, 1.0f });

                    const LabelStyle &labelStyle = button->label->style;
                    renderer->DrawString(button->label->text, button->label->font, labelStyle.color, textTransform, labelStyle.kerning, labelStyle.lineSpacing);
                }
            }
            else if (Ref<WidgetLabel> label = item->As<WidgetLabel>())
            {
                if (label->font && label->font->IsReady())
                {
                    const Rect textBounds = label->GetTextBounds();
                    const LabelStyle &labelStyle = label->style;

                    const glm::mat4 textTransform = glm::translate(glm::mat4(1.0f), glm::vec3(label->GetAlignedRect().min - textBounds.min, 0.0f)) *
                        glm::scale(glm::mat4(1.0f), { labelStyle.fontSize, labelStyle.fontSize, 1.0f });

                    renderer->DrawString(label->text, label->font, labelStyle.color, textTransform, labelStyle.kerning, labelStyle.lineSpacing);
                }
            }
            else if (Ref<WidgetImage> img = item->As<WidgetImage>())
            {
                renderer->DrawQuad(img->GetAlignedRect(), 0.0f, glm::vec4(1.0f), img->image, glm::vec2(0.0f), glm::vec2(1.0f));
            }

            const std::vector<Ref<IWidgetItem>> sortedChildren = GetSortedVisibleChildren(item->children);
            for (const Ref<IWidgetItem> &child : sortedChildren)
            {
                DrawRecursive(renderer, child);
            }
        }
    }

    static std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> s_WidgetQuadPSOCache;
    static std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> s_WidgetTextPSOCache;

    struct CameraBindingKey
    {
        nvrhi::IBindingLayout *layout = nullptr;
        nvrhi::IBuffer *cameraBuffer = nullptr;

        bool operator==(const CameraBindingKey &other) const noexcept
        {
            return layout == other.layout && cameraBuffer == other.cameraBuffer;
        }
    };

    struct CameraBindingKeyHash
    {
        size_t operator()(const CameraBindingKey &k) const noexcept
        {
            size_t h = std::hash<const void *> {}(k.layout);
            h ^= (std::hash<const void *>{}(k.cameraBuffer) + 0x9e3779b9 + (h << 6) + (h >> 2));
            return h;
        }
    };

    static std::unordered_map<CameraBindingKey, nvrhi::BindingSetHandle, CameraBindingKeyHash> s_WidgetBindingSetCache;

    static Ref<GraphicsPipeline> GetWidgetQuadPipelineForFB(nvrhi::IFramebuffer *framebuffer)
    {
        auto key = MakeFramebufferKey(framebuffer);
        auto it = s_WidgetQuadPSOCache.find(key);
        if (it != s_WidgetQuadPSOCache.end())
            return it->second;

        s_WidgetQuadPSOCache.clear();
        s_WidgetBindingSetCache.clear();

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.srcBlend = nvrhi::BlendFactor::SrcAlpha;
        params.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        params.srcBlendAlpha = nvrhi::BlendFactor::One;
        params.destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
        params.enableDepthWrite = false;
        params.enableDepthTest = false;
        params.enableDepthStencil = false;
        params.fillMode = nvrhi::RasterFillMode::Solid;
        params.cullMode = nvrhi::RasterCullMode::None;
        params.depthFunc = nvrhi::ComparisonFunc::Always;

        nvrhi::BindingLayoutDesc bindingLayoutDesc;
        bindingLayoutDesc.setVisibility(nvrhi::ShaderType::All);
        bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
        bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0));

        for (uint8_t i = 0; i < 32; i++)
        {
            bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(i));
        }

        nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(bindingLayoutDesc);

        Ref<Shader> vertexShader = Shader::Create("resources/shaders/widget.vertex.hlsl", ShaderType::Vertex, true);
        Ref<Shader> pixelShader = Shader::Create("resources/shaders/widget.pixel.hlsl", ShaderType::Pixel, true);

        Ref<GraphicsPipeline> gp = GraphicsPipeline::Create();
        gp->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(bindingLayout)
            .Build(framebuffer, params);

        s_WidgetQuadPSOCache.emplace(key, gp);

        return gp;
    }

    static Ref<GraphicsPipeline> GetWidgetTextPipelineForFB(nvrhi::IFramebuffer *framebuffer)
    {
        auto key = MakeFramebufferKey(framebuffer);
        auto it = s_WidgetTextPSOCache.find(key);
        if (it != s_WidgetTextPSOCache.end())
            return it->second;

        s_WidgetTextPSOCache.clear();
        s_WidgetBindingSetCache.clear();

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.srcBlend = nvrhi::BlendFactor::SrcAlpha;
        params.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        params.srcBlendAlpha = nvrhi::BlendFactor::One;
        params.destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
        params.enableDepthWrite = false;
        params.enableDepthTest = false;
        params.enableDepthStencil = false;
        params.fillMode = nvrhi::RasterFillMode::Solid;
        params.cullMode = nvrhi::RasterCullMode::None;
        params.depthFunc = nvrhi::ComparisonFunc::Always;

        nvrhi::BindingLayoutDesc bindingLayoutDesc;
        bindingLayoutDesc.setVisibility(nvrhi::ShaderType::All);
        bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
        bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0));

        for (uint8_t i = 0; i < 32; i++)
        {
            bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(i));
        }

        nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(bindingLayoutDesc);

        Ref<Shader> vertexShader = Shader::Create("resources/shaders/widget_msdf_font.vertex.hlsl", ShaderType::Vertex, true);
        Ref<Shader> pixelShader = Shader::Create("resources/shaders/widget_msdf_font.pixel.hlsl", ShaderType::Pixel, true);

        Ref<GraphicsPipeline> gp = GraphicsPipeline::Create();
        gp->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(bindingLayout)
            .Build(framebuffer, params);

        s_WidgetTextPSOCache.emplace(key, gp);

        return gp;
    }

    static nvrhi::BindingSetHandle GetWidgetBindingSet(nvrhi::IBindingLayout *bindingLayout, const std::vector<Ref<Texture>> &textures, const Ref<ConstantBuffer> &cameraBuffer)
    {
        CameraBindingKey key { bindingLayout, cameraBuffer ? cameraBuffer->GetHandle() : nullptr };
        auto it = s_WidgetBindingSetCache.find(key);
        if (it != s_WidgetBindingSetCache.end())
            return it->second;

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        nvrhi::SamplerHandle sampler;
        Ref<Texture> whiteTexture = Renderer::GetWhiteTexture();
        for (uint8_t i = 0; i < 32; ++i)
        {
            if (i >= textures.size()) break;
            Ref<Texture> tex = textures[i];
            if (tex && tex.get() != whiteTexture.get() && tex->GetSampler())
            {
                sampler = tex->GetSampler();
                break;
            }
        }

        if (!sampler) sampler = whiteTexture ? whiteTexture->GetSampler() : nullptr;

        nvrhi::BindingSetDesc bindingSetDesc;
        bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, cameraBuffer->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, sampler));
        for (uint8_t i = 0; i < 32; ++i)
        {
            Ref<Texture> tex = (i < textures.size() && textures[i]) ? textures[i] : whiteTexture;
            bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(i, tex->GetHandle()));
        }

        nvrhi::BindingSetHandle bindingSet = device->createBindingSet(bindingSetDesc, bindingLayout);
        s_WidgetBindingSetCache.emplace(key, bindingSet);

        return bindingSet;
    }

    WidgetRenderer::WidgetRenderer(uint32_t width, uint32_t height)
        : m_Width(width), m_Height(height), m_MouseX(0), m_MouseY(0)
    {
        m_Projection = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);
        m_CameraBuffer = ConstantBuffer::Create(sizeof(CameraBufferData), false, 1, "[WidgetRenderer] Camera buffer");

        InitQuadData();
        InitTextData();
    }

    WidgetRenderer::~WidgetRenderer()
    {
        s_WidgetQuadPSOCache.clear();
        s_WidgetTextPSOCache.clear();
        s_WidgetBindingSetCache.clear();
    }

    void WidgetRenderer::SetMousePosition(uint32_t mouseX, uint32_t mouseY)
    {
        m_MouseX = mouseX;
        m_MouseY = mouseY;
    }

    void WidgetRenderer::DrawQuad(const Rect &rect, float rotation, const glm::vec4 &color, const Ref<Texture> &texture, const glm::vec2 &uv0, const glm::vec2 &uv1)
    {
        const glm::vec2 textureCoords[] =
        {
            { uv0.x, uv0.y },
            { uv1.x, uv1.y },
            { uv0.x, uv1.y },
            { uv1.x, uv0.y }
        };

        const glm::vec4 positions[4] =
        {
            { rect.min.x, rect.min.y, 0.0f, 1.0f }, // bottom-left
            { rect.max.x, rect.max.y, 0.0f, 1.0f }, // top-right
            { rect.min.x, rect.max.y, 0.0f, 1.0f }, // top-left
            { rect.max.x, rect.min.y, 0.0f, 1.0f }, // bottom-right
        };

        uint32_t texIndex = GetOrInsertQuadTexture(texture);

        for (uint32_t i = 0; i < 4; ++i)
        {
            m_QuadBatch.vertexBufferPtr->position = positions[i];
            m_QuadBatch.vertexBufferPtr->texCoord = textureCoords[i];
            m_QuadBatch.vertexBufferPtr->tilingFactor = glm::vec2(1.0f);
            m_QuadBatch.vertexBufferPtr->color = color;
            m_QuadBatch.vertexBufferPtr->texIndex = texIndex;
            m_QuadBatch.vertexBufferPtr++;
        }

        m_QuadBatch.indexCount += 6;
        m_QuadBatch.count++;
    }

    void WidgetRenderer::DrawString(const std::string &str, const Ref<Font> &font, const glm::vec4 &color, const glm::mat4 &transform, float kerning, float linespacing)
    {
        if (!font || !font->IsReady())
            return;

        Ref<Texture> fontAtlasTexture = font->GetAtlasTexture();
        const auto &fontGeometry = font->GetGeometry();
        const auto &metrics = fontGeometry.getMetrics();

        uint32_t texIndex = GetOrInsertFontTexture(fontAtlasTexture);

        double x = 0.0;
        double y = 0.0;
        double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
        const double spaceGlypAdvance = fontGeometry.getGlyph(' ')->getAdvance();

        for (size_t i = 0; i < str.size(); ++i)
        {
            char character = str[i];
            if (character == '\r')
                continue;

            if (character == '\n')
            {
                x = 0.0;
                y += fsScale * metrics.lineHeight + linespacing;
                continue;
            }
            if (character == ' ')
            {
                float advance = static_cast<float>(spaceGlypAdvance);
                if (i < str.size() - 1)
                {
                    double dAdvence;
                    fontGeometry.getAdvance(dAdvence, character, str[i + 1]);
                    advance = static_cast<float>(dAdvence);
                }
                x += fsScale * advance + kerning;
                continue;
            }
            if (character == '\t')
            {
                x += 4.0 * (fsScale * spaceGlypAdvance + kerning);
                continue;
            }

            auto glyph = fontGeometry.getGlyph(character);
            if (!glyph) glyph = fontGeometry.getGlyph('?');
            if (!glyph) continue;

            double atlasLeft, atlasBottom, atlasRight, atlasTop;
            glyph->getQuadAtlasBounds(atlasLeft, atlasBottom, atlasRight, atlasTop);
            glm::vec2 texCoordMin(static_cast<float>(atlasLeft), static_cast<float>(atlasBottom));
            glm::vec2 texCoordMax(static_cast<float>(atlasRight), static_cast<float>(atlasTop));

            double planeLeft, planeBottom, planeRight, planeTop;
            glyph->getQuadPlaneBounds(planeLeft, planeBottom, planeRight, planeTop);
            glm::vec2 quadMin(static_cast<float>(planeLeft), static_cast<float>(planeBottom));
            glm::vec2 quadMax(static_cast<float>(planeRight), static_cast<float>(planeTop));

            quadMin *= fsScale;
            quadMax *= fsScale;

            quadMin.y = -quadMin.y;
            quadMax.y = -quadMax.y;

            quadMin += glm::vec2(x, y);
            quadMax += glm::vec2(x, y);

            float texX0 = texCoordMin.x;
            float texY0 = texCoordMin.y;
            float texX1 = texCoordMax.x;
            float texY1 = texCoordMax.y;

            if (fontAtlasTexture && fontAtlasTexture->GetWidth() > 0 && fontAtlasTexture->GetHeight() > 0)
            {
                const float texelWidth = 1.0f / static_cast<float>(fontAtlasTexture->GetWidth());
                const float texelHeight = 1.0f / static_cast<float>(fontAtlasTexture->GetHeight());
                texX0 *= texelWidth;
                texY0 *= texelHeight;
                texX1 *= texelWidth;
                texY1 *= texelHeight;
            }

            glm::vec4 quadPositions[4];
            quadPositions[0] = transform * glm::vec4(quadMin.x, quadMin.y, 0.0f, 1.0f);
            quadPositions[1] = transform * glm::vec4(quadMax.x, quadMax.y, 0.0f, 1.0f);
            quadPositions[2] = transform * glm::vec4(quadMin.x, quadMax.y, 0.0f, 1.0f);
            quadPositions[3] = transform * glm::vec4(quadMax.x, quadMin.y, 0.0f, 1.0f);

            glm::vec2 texCoords[4];
            texCoords[0] = { texX0, texY0 };
            texCoords[1] = { texX1, texY1 };
            texCoords[2] = { texX0, texY1 };
            texCoords[3] = { texX1, texY0 };

            for (int j = 0; j < 4; ++j)
            {
                m_TextBatch.vertexBufferPtr->position = quadPositions[j];
                m_TextBatch.vertexBufferPtr->color = color;
                m_TextBatch.vertexBufferPtr->texCoord = texCoords[j];
                m_TextBatch.vertexBufferPtr->texIndex = texIndex;
                m_TextBatch.vertexBufferPtr++;
            }

            m_TextBatch.indexCount += 6;
            m_TextBatch.count++;

            if (i < str.size() - 1)
            {
                double advance;
                fontGeometry.getAdvance(advance, character, str[i + 1]);
                x += fsScale * advance + kerning;
            }
        }
    }

    void WidgetRenderer::SetScene(Scene *scene)
    {
        m_Scene = scene;

        for (auto it = m_RenderLayers.rbegin(); it != m_RenderLayers.rend(); ++it)
        {
            if (!it->widget)
                continue;

            it->widget->SetViewportSize(m_Width, m_Height);
            it->widget->SetScene(m_Scene);
        }
    }

    void WidgetRenderer::Update(float deltaTime)
    {
        (void)deltaTime;
        BuildRenderLayers();

        if (!m_Project || (!m_PreviewWidget && !m_Scene))
            return;

        const glm::uvec2 mousePos = { m_MouseX, m_MouseY };
        const bool isMousePressed = Input::IsMouseButtonPressed(Mouse::ButtonLeft);

        for (const WidgetRenderLayer &layer : m_RenderLayers)
        {
            if (!layer.widget || !layer.widget->IsEnabled())
                continue;

            layer.widget->SetViewportSize(m_Width, m_Height);
            WidgetContainer *root = layer.widget->GetRoot();
            if (!root)
            {
                root = layer.widget->CreateRoot(m_Width, m_Height);
            }
            if (!root)
            {
                continue;
            }

            MeasureRecursive(root->As<IWidgetItem>());
            root->Arrange(Rect(0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height)));
            ResolveWidgetAssetsRecursive(root->As<IWidgetItem>(), m_Project->GetAssetManager());
            UpdateRecursive(root->As<IWidgetItem>(), mousePos, isMousePressed);
        }
    }

    void WidgetRenderer::Begin(nvrhi::ICommandList *cmd)
    {
        m_QuadBatch.indexCount = 0;
        m_QuadBatch.count = 0;
        m_QuadBatch.textureSlotIndex = 1;
        m_QuadBatch.vertexBufferPtr = m_QuadBatch.vertexBufferBase;

        m_TextBatch.indexCount = 0;
        m_TextBatch.count = 0;
        m_TextBatch.textureSlotIndex = 1;
        m_TextBatch.vertexBufferPtr = m_TextBatch.vertexBufferBase;

        m_Cmd = cmd;
    }

    void WidgetRenderer::Flush(nvrhi::IFramebuffer *framebuffer)
    {
        const nvrhi::Viewport &viewport = framebuffer->getFramebufferInfo().getViewport();

        if (m_QuadBatch.indexCount > 0)
        {
            const size_t bufferSize = reinterpret_cast<uint8_t *>(m_QuadBatch.vertexBufferPtr) - reinterpret_cast<uint8_t *>(m_QuadBatch.vertexBufferBase);
            m_QuadBatch.vertexBuffer->SetData(m_Cmd, Buffer(m_QuadBatch.vertexBufferBase, bufferSize));

            Ref<GraphicsPipeline> gp = GetWidgetQuadPipelineForFB(framebuffer);
            nvrhi::BindingSetHandle bindingSet = GetWidgetBindingSet(gp->GetBindingLayout(0), m_QuadBatch.textureSlots, m_CameraBuffer);

            const auto graphicsState = nvrhi::GraphicsState()
                .setPipeline(gp->GetHandle())
                .setFramebuffer(framebuffer)
                .addBindingSet(bindingSet)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(viewport))
                .addVertexBuffer(nvrhi::VertexBufferBinding { m_QuadBatch.vertexBuffer->GetHandle(), 0, 0 })
                .setIndexBuffer({ m_QuadBatch.indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });
            m_Cmd->setGraphicsState(graphicsState);

            nvrhi::DrawArguments args;
            args.vertexCount = m_QuadBatch.indexCount;
            args.instanceCount = 1;
            m_Cmd->drawIndexed(args);
        }

        if (m_TextBatch.indexCount > 0)
        {
            const size_t bufferSize = reinterpret_cast<uint8_t *>(m_TextBatch.vertexBufferPtr) - reinterpret_cast<uint8_t *>(m_TextBatch.vertexBufferBase);
            m_TextBatch.vertexBuffer->SetData(m_Cmd, Buffer(m_TextBatch.vertexBufferBase, bufferSize));

            Ref<GraphicsPipeline> gp = GetWidgetTextPipelineForFB(framebuffer);
            nvrhi::BindingSetHandle bindingSet = GetWidgetBindingSet(gp->GetBindingLayout(0), m_TextBatch.textureSlots, m_CameraBuffer);

            const auto graphicsState = nvrhi::GraphicsState()
                .setPipeline(gp->GetHandle())
                .setFramebuffer(framebuffer)
                .addBindingSet(bindingSet)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(viewport))
                .addVertexBuffer(nvrhi::VertexBufferBinding { m_TextBatch.vertexBuffer->GetHandle(), 0, 0 })
                .setIndexBuffer({ m_TextBatch.indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });
            m_Cmd->setGraphicsState(graphicsState);

            nvrhi::DrawArguments args;
            args.vertexCount = m_TextBatch.indexCount;
            args.instanceCount = 1;
            m_Cmd->drawIndexed(args);
        }
    }

    void WidgetRenderer::Render(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *fb)
    {
        CameraBufferData cameraData = { m_Projection, glm::mat4(1.0f), {0.0f, 0.0f, 0.0f, 1.0f} };
        m_CameraBuffer->SetData(cmd, Buffer(&cameraData, sizeof(cameraData)));

        Begin(cmd);
        RenderWidgetItems();
        Flush(fb);
    }

    uint32_t WidgetRenderer::GetOrInsertQuadTexture(const Ref<Texture> &texture)
    {
        uint32_t texIndex = 0;
        for (uint32_t i = 1; i < m_QuadBatch.textureSlotIndex; ++i)
        {
            if (m_QuadBatch.textureSlots[i].get() == texture.get())
            {
                texIndex = i;
                break;
            }
        }

        if (texIndex == 0)
        {
            if (m_QuadBatch.textureSlotIndex >= 32)
            {
                return 0;
            }
            texIndex = m_QuadBatch.textureSlotIndex;
            m_QuadBatch.textureSlots[m_QuadBatch.textureSlotIndex++] = texture;
        }

        return texIndex;
    }

    uint32_t WidgetRenderer::GetOrInsertFontTexture(const Ref<Texture> &texture)
    {
        uint32_t texIndex = 0;
        for (uint32_t i = 1; i < m_TextBatch.textureSlotIndex; ++i)
        {
            if (m_TextBatch.textureSlots[i].get() == texture.get())
            {
                texIndex = i;
                break;
            }
        }

        if (texIndex == 0)
        {
            if (m_TextBatch.textureSlotIndex >= 32)
            {
                return 0;
            }
            texIndex = m_TextBatch.textureSlotIndex;
            m_TextBatch.textureSlots[m_TextBatch.textureSlotIndex++] = texture;
        }

        return texIndex;
    }

    void WidgetRenderer::Resize(uint32_t width, uint32_t height)
    {
        m_Width = width;
        m_Height = height;
        m_Projection = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);
        for (const auto &layer : m_RenderLayers)
        {
            if (layer.widget)
            {
                layer.widget->SetViewportSize(width, height);
            }
        }
    }

    void WidgetRenderer::BuildRenderLayers()
    {
        m_RenderLayers.clear();
        if (!m_Project)
        {
            return;
        }

        std::unordered_set<uint64_t> visited;
        std::function<void(const Ref<WidgetCanvas> &, bool)> collectWidget = [&](const Ref<WidgetCanvas> &widget, bool blocksLower)
        {
            if (!widget)
            {
                return;
            }

            const uint64_t widgetHandle = static_cast<uint64_t>(widget->handle);
            if (widgetHandle != 0 && visited.contains(widgetHandle))
            {
                return;
            }
            if (widgetHandle != 0)
            {
                visited.insert(widgetHandle);
            }

            m_RenderLayers.push_back({ widget, blocksLower || widget->BlocksWidgetsBelow() });

            for (const WidgetChildEntry &child : widget->GetChildWidgets())
            {
                if (!child.enabled || child.handle == AssetHandle(0))
                    continue;

                Ref<WidgetCanvas> childWidget = m_Project->GetAsset<WidgetCanvas>(child.handle);
                if (!childWidget)
                    continue;

                collectWidget(childWidget, child.blockWidgetsBelow);
            }
        };

        if (m_PreviewWidget)
        {
            collectWidget(m_PreviewWidget, m_PreviewWidget->BlocksWidgetsBelow());
            return;
        }

        if (!m_Scene || !m_Scene->registry)
        {
            return;
        }

        auto widgetView = m_Scene->registry->view<WidgetComponent>();
        for (const entt::entity entity : widgetView)
        {
            const WidgetComponent &widgetComp = widgetView.get<WidgetComponent>(entity);
            if (widgetComp.widgetHandle == AssetHandle(0))
                continue;

            Ref<WidgetCanvas> widget = m_Project->GetAsset<WidgetCanvas>(widgetComp.widgetHandle);
            if (!widget)
                continue;

            collectWidget(widget, widget ? widget->BlocksWidgetsBelow() : false);
        }
    }

    void WidgetRenderer::RenderWidgetItems()
    {
        if (!m_Project)
            return;

        for (const WidgetRenderLayer &layer : m_RenderLayers)
        {
            if (!layer.widget || !layer.widget->IsEnabled())
                continue;

            WidgetContainer *root = layer.widget->GetRoot();
            if (!root)
            {
                root = layer.widget->CreateRoot(m_Width, m_Height);
            }
            if (!root)
            {
                continue;
            }

            DrawRecursive(this, root->As<IWidgetItem>());
        }
    }

    void WidgetRenderer::InitQuadData()
    {
        m_QuadBatch.minCount = 64;
        m_QuadBatch.maxCount = m_QuadBatch.minCount;
        m_QuadBatch.verticesPerObject = 4;
        m_QuadBatch.indicesPerObject = 6;
        m_QuadBatch.maxVertices = m_QuadBatch.maxCount * m_QuadBatch.verticesPerObject;
        m_QuadBatch.maxIndices = m_QuadBatch.maxCount * m_QuadBatch.indicesPerObject;

        size_t vertAllocSize = m_QuadBatch.maxVertices * sizeof(VertexWidgetQuad);
        m_QuadBatch.vertexBufferBase = new VertexWidgetQuad[m_QuadBatch.maxVertices];
        m_QuadBatch.vertexBuffer = VertexBuffer::Create(vertAllocSize);

        size_t indicesAllocSize = m_QuadBatch.maxIndices * sizeof(uint32_t);
        m_QuadBatch.indexBuffer = IndexBuffer::Create(indicesAllocSize);

        m_QuadBatch.textureSlots.resize(32);
        m_QuadBatch.textureSlots[0] = Renderer::GetWhiteTexture();

        std::vector<uint32_t> indices(m_QuadBatch.maxIndices);
        uint32_t offset = 0;
        for (uint32_t i = 0; i < m_QuadBatch.maxIndices; i += 6)
        {
            indices[0 + i] = offset + 0;
            indices[1 + i] = offset + 1;
            indices[2 + i] = offset + 2;
            indices[3 + i] = offset + 0;
            indices[4 + i] = offset + 3;
            indices[5 + i] = offset + 1;
            offset += 4;
        }

        auto device = DeviceManager::GetInstance()->GetDevice();
        nvrhi::CommandListHandle cmd = device->createCommandList();
        cmd->open();
        m_QuadBatch.indexBuffer->SetData(cmd, Buffer(indices.data(), indices.size() * sizeof(uint32_t)));
        cmd->close();
        device->executeCommandList(cmd);
    }

    void WidgetRenderer::InitTextData()
    {
        m_TextBatch.minCount = 64;
        m_TextBatch.maxCount = m_TextBatch.minCount;
        m_TextBatch.verticesPerObject = 4;
        m_TextBatch.indicesPerObject = 6;
        m_TextBatch.maxVertices = m_TextBatch.maxCount * m_TextBatch.verticesPerObject;
        m_TextBatch.maxIndices = m_TextBatch.maxCount * m_TextBatch.indicesPerObject;

        size_t vertAllocSize = m_TextBatch.maxVertices * sizeof(VertexWidgetText);
        m_TextBatch.vertexBufferBase = new VertexWidgetText[m_TextBatch.maxVertices];
        m_TextBatch.vertexBuffer = VertexBuffer::Create(vertAllocSize);

        size_t indicesAllocSize = m_TextBatch.maxIndices * sizeof(uint32_t);
        m_TextBatch.indexBuffer = IndexBuffer::Create(indicesAllocSize);

        m_TextBatch.textureSlots.resize(32);
        m_TextBatch.textureSlots[0] = Renderer::GetWhiteTexture();

        std::vector<uint32_t> indices(m_TextBatch.maxIndices);
        uint32_t offset = 0;
        for (uint32_t i = 0; i < m_TextBatch.maxIndices; i += 6)
        {
            indices[0 + i] = offset + 0;
            indices[1 + i] = offset + 1;
            indices[2 + i] = offset + 2;
            indices[3 + i] = offset + 0;
            indices[4 + i] = offset + 3;
            indices[5 + i] = offset + 1;
            offset += 4;
        }

        auto device = DeviceManager::GetInstance()->GetDevice();
        nvrhi::CommandListHandle cmd = device->createCommandList();
        cmd->open();
        m_TextBatch.indexBuffer->SetData(cmd, Buffer(indices.data(), indices.size() * sizeof(uint32_t)));
        cmd->close();
        device->executeCommandList(cmd);
    }

    Ref<WidgetRenderer> WidgetRenderer::Create(uint32_t width, uint32_t height)
    {
        return CreateRef<WidgetRenderer>(width, height);
    }
}
