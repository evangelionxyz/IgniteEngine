// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "widget_renderer.hpp"
#include "ignite/graphics/bindless_system.hpp"
#include "ignite/graphics/binding_set.hpp"
#include "ignite/graphics/render_target.hpp"

#include "widget_canvas.hpp"
#include "widget_container.hpp"
#include "widget_button.hpp"
#include "widget_label.hpp"
#include "widget_image.hpp"

#include "ignite/core/input/input_system.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/hash_keys.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/font.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/icamera.hpp"

#include <glm/gtc/matrix_transform.hpp>

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
            if (!item)
            {
                return;
            }

            if (Ref<WidgetButton> button = item->As<WidgetButton>())
            {
                if (assetManager)
                {
                    Ref<Texture> prevImg = button->image;
                    button->image = (button->imageHandle != AssetHandle(0))
                        ? assetManager->GetAsset<Texture>(button->imageHandle)
                        : nullptr;
                    if (prevImg != button->image)
                    {
                        button->MarkLayoutDirty();
                    }
                }

                if (button->label)
                {
                    Ref<Font> prevFont = button->label->font;
                    button->label->font = (assetManager && button->label->fontHandle != AssetHandle(0))
                        ? assetManager->GetAsset<Font>(button->label->fontHandle)
                        : nullptr;
                    if (!button->label->font)
                    {
                        button->label->font = Renderer::GetDefaultFont();
                    }
                    const bool isFontReady = (button->label->font && button->label->font->IsReady());
                    if (prevFont != button->label->font || button->label->fontReadyAtLastLayout != isFontReady)
                    {
                        button->label->fontReadyAtLastLayout = isFontReady;
                        button->label->MarkLayoutDirty();
                    }
                }
            }
            else if (Ref<WidgetLabel> label = item->As<WidgetLabel>())
            {
                Ref<Font> prevFont = label->font;
                label->font = (assetManager && label->fontHandle != AssetHandle(0))
                    ? assetManager->GetAsset<Font>(label->fontHandle)
                    : nullptr;
                if (!label->font)
                {
                    label->font = Renderer::GetDefaultFont();
                }
                const bool isFontReady = (label->font && label->font->IsReady());
                if (prevFont != label->font || label->fontReadyAtLastLayout != isFontReady)
                {
                    label->fontReadyAtLastLayout = isFontReady;
                    label->MarkLayoutDirty();
                }
            }
            else if (Ref<WidgetImage> img = item->As<WidgetImage>())
            {
                if (assetManager)
                {
                    Ref<Texture> prevImg = img->image;
                    img->image = (img->imageHandle != AssetHandle(0))
                        ? assetManager->GetAsset<Texture>(img->imageHandle)
                        : nullptr;
                    if (prevImg != img->image)
                    {
                        img->MarkLayoutDirty();
                    }
                }
            }

            for (const Ref<IWidgetItem> &child : item->children)
            {
                ResolveWidgetAssetsRecursive(child, assetManager);
            }
        }

        static void ResetInteractionRecursive(const Ref<IWidgetItem> &item)
        {
            if (!item)
                return;

            if (Ref<WidgetButton> button = item->As<WidgetButton>())
            {
                button->ResetInteractionState();
            }

            for (const Ref<IWidgetItem> &child : item->children)
            {
                ResetInteractionRecursive(child);
            }
        }

        static void UpdateRecursive(const Ref<IWidgetItem> &item, const glm::uvec2 &mousePos, bool isMousePressed, float deltaTime)
        {
            if (!item)
                return;

            if (!item->IsVisible())
            {
                ResetInteractionRecursive(item);
                return;
            }

            if (Ref<WidgetButton> button = item->As<WidgetButton>())
            {
                button->OnMouseClick(mousePos, isMousePressed);
                button->UpdateAnimation(deltaTime);
            }

            for (const Ref<IWidgetItem> &child : item->children)
            {
                UpdateRecursive(child, mousePos, isMousePressed, deltaTime);
            }
        }

        static void DrawWidgetDropShadow(WidgetRenderer *renderer, const Rect &borderRect, const UIStyle &style, float opacity)
        {
            if (!style.shadowEnabled || style.shadowColor.a <= 0.001f || style.shadowSize <= 0.0f)
                return;

            const glm::vec2 expand(style.shadowSize);
            const Rect shadowRect = {
                borderRect.min - expand + style.shadowOffset,
                borderRect.max + expand + style.shadowOffset
            };
            glm::vec4 shadowCol = style.shadowColor;
            shadowCol.a *= opacity;
            const float softness = std::clamp(style.shadowSoftness, 0.08f, 0.85f);
            renderer->DrawQuad(shadowRect, 0.0f, shadowCol, nullptr, glm::vec2(0.0f), glm::vec2(1.0f), glm::vec2(2.0f, softness));
        }

        static void DrawRecursive(WidgetRenderer *renderer, const Ref<IWidgetItem> &item)
        {
            if (!renderer || !item || !item->IsVisible())
            {
                return;
            }

            const float opacity = item->baseStyle.opacity;
            glm::vec4 bgColor = item->baseStyle.backgroundColor;
            glm::vec4 borderColor = item->baseStyle.borderColor;
            float borderWidth = item->baseStyle.borderWidth;
            float cornerRadius = item->baseStyle.cornerRadius;

            if (Ref<WidgetButton> button = item->As<WidgetButton>())
            {
                bgColor = button->GetCurrentColor();
                if (button->style.borderColor.a > 0.0f) borderColor = button->style.borderColor;
                if (button->style.borderWidth > 0.0f) borderWidth = button->style.borderWidth;
                if (button->style.cornerRadius > 0.0f) cornerRadius = button->style.cornerRadius;

                bgColor.a *= opacity;
                borderColor.a *= opacity;

                const float hoverT = glm::clamp(button->hoverAnim, 0.0f, 1.0f);
                const float pressT = glm::clamp(button->pressAnim, 0.0f, 1.0f);
                const float yShift = std::round(-2.0f * hoverT + 2.5f * pressT);

                Rect btnRect = {
                    button->box.border.min + glm::vec2(0.0f, yShift),
                    button->box.border.max + glm::vec2(0.0f, yShift)
                };

                // 1. Soft drop shadow + outer hover glow
                DrawWidgetDropShadow(renderer, btnRect, button->baseStyle, opacity);
                if (bgColor.a > 0.05f && hoverT > 0.01f && borderColor.a > 0.01f)
                {
                    Rect glowRect = {
                        btnRect.min - glm::vec2(6.0f, 6.0f),
                        btnRect.max + glm::vec2(6.0f, 6.0f)
                    };
                    glm::vec4 glowCol = glm::vec4(borderColor.r, borderColor.g, borderColor.b, 0.32f * hoverT * opacity);
                    renderer->DrawQuad(glowRect, 0.0f, glowCol, nullptr, glm::vec2(0.0f), glm::vec2(1.0f), glm::vec2(2.0f, 0.36f));
                }

                // 2. Button surface
                if (bgColor.a > 0.0f)
                {
                    const glm::vec2 surfaceMode = (button->image == nullptr)
                        ? glm::vec2(3.0f, hoverT + pressT * 0.6f)
                        : glm::vec2(1.0f, 1.0f);

                    if (cornerRadius > 0.0f)
                    {
                        renderer->DrawRoundedQuad(btnRect, cornerRadius, bgColor, button->image, glm::vec2(0.0f), glm::vec2(1.0f), surfaceMode);
                    }
                    else
                    {
                        renderer->DrawQuad(btnRect, 0.0f, bgColor, button->image, glm::vec2(0.0f), glm::vec2(1.0f), surfaceMode);
                    }
                }

                // 3. Button border with hover intensification
                if (borderWidth > 0.0f && borderColor.a > 0.0f)
                {
                    glm::vec4 activeBorderCol = glm::mix(borderColor, glm::vec4(glm::min(glm::vec3(borderColor) * 1.45f + glm::vec3(0.12f), glm::vec3(1.0f)), borderColor.a), hoverT);
                    if (pressT > 0.05f)
                    {
                        activeBorderCol = glm::mix(activeBorderCol, glm::vec4(1.0f, 0.92f, 0.55f, borderColor.a), pressT * 0.7f);
                    }
                    renderer->DrawRoundedBorder(btnRect, cornerRadius, borderWidth + hoverT * 0.75f, activeBorderCol, glm::vec2(4.0f, hoverT));
                }

                // 4. Crisp pixel-snapped button label
                if (button->label && button->label->font && button->label->font->IsReady())
                {
                    const Rect textBounds = button->label->GetTextBounds();
                    const glm::vec2 textSize = textBounds.GetSize();
                    const glm::vec2 textPos =
                    {
                        std::round(button->box.content.min.x + std::max((button->box.content.GetSize().x - textSize.x) * 0.5f, 0.0f) - textBounds.min.x),
                        std::round(button->box.content.min.y + std::max((button->box.content.GetSize().y - textSize.y) * 0.5f, 0.0f) - textBounds.min.y + yShift)
                    };

                    LabelStyle labelStyle = button->label->style;
                    labelStyle.color.a *= opacity;
                    if (hoverT > 0.01f)
                    {
                        labelStyle.color.r = std::min(1.0f, labelStyle.color.r + 0.08f * hoverT);
                        labelStyle.color.g = std::min(1.0f, labelStyle.color.g + 0.08f * hoverT);
                        labelStyle.color.b = std::min(1.0f, labelStyle.color.b + 0.08f * hoverT);
                    }

                    if (labelStyle.shadowEnabled && labelStyle.shadowColor.a > 0.001f)
                    {
                        glm::vec4 shadowCol = labelStyle.shadowColor;
                        shadowCol.a *= opacity;
                        const glm::mat4 shadowTransform = glm::translate(glm::mat4(1.0f), glm::vec3(textPos + labelStyle.shadowOffset, 0.0f)) *
                            glm::scale(glm::mat4(1.0f), { labelStyle.fontSize, labelStyle.fontSize, 1.0f });
                        renderer->DrawString(button->label->text, button->label->font, shadowCol, shadowTransform, labelStyle.kerning, labelStyle.lineSpacing);
                    }

                    const glm::mat4 textTransform = glm::translate(glm::mat4(1.0f), glm::vec3(textPos, 0.0f)) *
                        glm::scale(glm::mat4(1.0f), { labelStyle.fontSize, labelStyle.fontSize, 1.0f });

                    renderer->DrawString(button->label->text, button->label->font, labelStyle.color, textTransform, labelStyle.kerning, labelStyle.lineSpacing);
                }
            }
            else if (Ref<WidgetLabel> label = item->As<WidgetLabel>())
            {
                bgColor.a *= opacity;
                borderColor.a *= opacity;

                DrawWidgetDropShadow(renderer, label->box.border, label->baseStyle, opacity);

                if (bgColor.a > 0.0f)
                {
                    if (cornerRadius > 0.0f)
                        renderer->DrawRoundedQuad(label->box.border, cornerRadius, bgColor, nullptr, glm::vec2(0.0f), glm::vec2(1.0f));
                    else
                        renderer->DrawQuad(label->box.border, 0.0f, bgColor, nullptr, glm::vec2(0.0f), glm::vec2(1.0f));
                }

                if (borderWidth > 0.0f && borderColor.a > 0.0f)
                {
                    renderer->DrawRoundedBorder(label->box.border, cornerRadius, borderWidth, borderColor);
                }

                if (label->font && label->font->IsReady())
                {
                    const Rect textBounds = label->GetTextBounds();
                    LabelStyle labelStyle = label->style;
                    labelStyle.color.a *= opacity;

                    const glm::vec2 snappedPos = glm::round(label->box.content.min - textBounds.min);

                    if (labelStyle.shadowEnabled && labelStyle.shadowColor.a > 0.001f)
                    {
                        glm::vec4 shadowCol = labelStyle.shadowColor;
                        shadowCol.a *= opacity;
                        const glm::mat4 shadowTransform = glm::translate(glm::mat4(1.0f), glm::vec3(snappedPos + labelStyle.shadowOffset, 0.0f)) *
                            glm::scale(glm::mat4(1.0f), { labelStyle.fontSize, labelStyle.fontSize, 1.0f });
                        renderer->DrawString(label->text, label->font, shadowCol, shadowTransform, labelStyle.kerning, labelStyle.lineSpacing);
                    }

                    const glm::mat4 textTransform = glm::translate(glm::mat4(1.0f), glm::vec3(snappedPos, 0.0f)) *
                        glm::scale(glm::mat4(1.0f), { labelStyle.fontSize, labelStyle.fontSize, 1.0f });

                    renderer->DrawString(label->text, label->font, labelStyle.color, textTransform, labelStyle.kerning, labelStyle.lineSpacing);
                }
            }
            else if (Ref<WidgetImage> img = item->As<WidgetImage>())
            {
                bgColor.a *= opacity;
                borderColor.a *= opacity;

                DrawWidgetDropShadow(renderer, img->box.border, img->baseStyle, opacity);

                glm::vec4 color = img->tintColor;
                color.a *= opacity;
                if (bgColor.a > 0.0f) color *= bgColor;

                if (cornerRadius > 0.0f)
                    renderer->DrawRoundedQuad(img->box.content, cornerRadius, color, img->image, img->uv0, img->uv1);
                else
                    renderer->DrawQuad(img->box.content, 0.0f, color, img->image, img->uv0, img->uv1);

                if (borderWidth > 0.0f && borderColor.a > 0.0f)
                {
                    renderer->DrawRoundedBorder(img->box.border, cornerRadius, borderWidth, borderColor);
                }
            }
            else
            {
                bgColor.a *= opacity;
                borderColor.a *= opacity;

                const glm::vec2 rectSize = item->box.border.GetSize();
                const bool isTacticalCard = (bgColor.a >= 0.25f) && (borderWidth > 0.0f || cornerRadius > 2.0f) && (rectSize.x >= 48.0f && rectSize.y >= 24.0f) && (rectSize.x < 1850.0f || rectSize.y < 1020.0f);

                DrawWidgetDropShadow(renderer, item->box.border, item->baseStyle, opacity);

                if (bgColor.a > 0.0f)
                {
                    const glm::vec2 surfaceMode = isTacticalCard ? glm::vec2(3.0f, 0.0f) : glm::vec2(1.0f, 1.0f);
                    if (cornerRadius > 0.0f)
                        renderer->DrawRoundedQuad(item->box.border, cornerRadius, bgColor, nullptr, glm::vec2(0.0f), glm::vec2(1.0f), surfaceMode);
                    else
                        renderer->DrawQuad(item->box.border, 0.0f, bgColor, nullptr, glm::vec2(0.0f), glm::vec2(1.0f), surfaceMode);
                }

                if (borderWidth > 0.0f && borderColor.a > 0.0f)
                {
                    renderer->DrawRoundedBorder(item->box.border, cornerRadius, borderWidth, borderColor, glm::vec2(4.0f, 0.0f));
                }
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
    static std::unordered_map<CameraBindingKey, Ref<BindingSet>, CameraBindingKeyHash> s_WidgetBindingSetCache;

    void WidgetRenderer::ClearCache()
    {
        s_WidgetQuadPSOCache.clear();
        s_WidgetTextPSOCache.clear();
        s_WidgetBindingSetCache.clear();
    }

    static Ref<GraphicsPipeline> GetWidgetQuadPipelineForFB(RenderTarget *framebuffer)
    {
        auto key = MakeFramebufferKey(framebuffer);
        auto it = s_WidgetQuadPSOCache.find(key);
        if (it != s_WidgetQuadPSOCache.end())
            return it->second;

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.srcBlend = nri::BlendFactor::SRC_ALPHA;
        params.destBlend = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
        params.srcBlendAlpha = nri::BlendFactor::ONE;
        params.destBlendAlpha = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
        params.enableDepthWrite = false;
        params.enableDepthTest = false;
        params.enableDepthStencil = false;
        params.fillMode = nri::FillMode::SOLID;
        params.cullMode = nri::CullMode::NONE;
        params.depthFunc = nri::CompareOp::ALWAYS;

        BindingLayoutDesc bindingLayoutDesc;
        bindingLayoutDesc.SetVisibility(nri::StageBits::ALL);
        bindingLayoutDesc.AddItem(BindingLayoutItem::ConstantBuffer(0));
        bindingLayoutDesc.AddItem(BindingLayoutItem::Sampler(0));

        Ref<Shader> vertexShader = Shader::Create("resources/shaders/widget.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, true);
        Ref<Shader> pixelShader = Shader::Create("resources/shaders/widget.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, true);

        Ref<GraphicsPipeline> gp = GraphicsPipeline::Create("Widget Quad Pipeline");
        gp->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(bindingLayoutDesc)
            .AddBindingLayout(BindlessSystem::GetBindingLayout())
            .Build(framebuffer, params);

        s_WidgetQuadPSOCache.emplace(key, gp);

        return gp;
    }

    static Ref<GraphicsPipeline> GetWidgetTextPipelineForFB(RenderTarget *framebuffer)
    {
        auto key = MakeFramebufferKey(framebuffer);
        auto it = s_WidgetTextPSOCache.find(key);
        if (it != s_WidgetTextPSOCache.end())
            return it->second;

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.srcBlend = nri::BlendFactor::SRC_ALPHA;
        params.destBlend = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
        params.srcBlendAlpha = nri::BlendFactor::ONE;
        params.destBlendAlpha = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
        params.enableDepthWrite = false;
        params.enableDepthTest = false;
        params.enableDepthStencil = false;
        params.fillMode = nri::FillMode::SOLID;
        params.cullMode = nri::CullMode::NONE;
        params.depthFunc = nri::CompareOp::ALWAYS;

        BindingLayoutDesc bindingLayoutDesc;
        bindingLayoutDesc.SetVisibility(nri::StageBits::ALL);
        bindingLayoutDesc.AddItem(BindingLayoutItem::ConstantBuffer(0));
        bindingLayoutDesc.AddItem(BindingLayoutItem::Sampler(0));

        Ref<Shader> vertexShader = Shader::Create("resources/shaders/widget_msdf_font.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, true);
        Ref<Shader> pixelShader = Shader::Create("resources/shaders/widget_msdf_font.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, true);

        Ref<GraphicsPipeline> gp = GraphicsPipeline::Create("Widget Text Pipeline");
        gp->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(bindingLayoutDesc)
            .AddBindingLayout(BindlessSystem::GetBindingLayout())
            .Build(framebuffer, params);

        s_WidgetTextPSOCache.emplace(key, gp);

        return gp;
    }

    static Ref<BindingSet> GetWidgetBindingSet(nri::PipelineLayout *pipelineLayout, const std::vector<Ref<Texture>> &textures, const Ref<ConstantBuffer> &cameraBuffer)
    {
        CameraBindingKey key { pipelineLayout, cameraBuffer ? cameraBuffer->GetDescriptor() : nullptr };
        auto it = s_WidgetBindingSetCache.find(key);
        if (it != s_WidgetBindingSetCache.end())
            return it->second;

        nri::Descriptor *sampler = nullptr;
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

        BindingSetDesc bindingSetDesc;
        bindingSetDesc.addItem(BindingSetItem::ConstantBuffer(0, cameraBuffer->GetDescriptor()));
        bindingSetDesc.addItem(BindingSetItem::Sampler(0, sampler));

        Ref<BindingSet> bindingSet = BindingSet::Create("[WidgetRenderer] BindingSet", pipelineLayout, 0, bindingSetDesc);
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
        delete[] m_QuadIndicesBase;
    }

    void WidgetRenderer::SetMousePosition(uint32_t mouseX, uint32_t mouseY)
    {
        m_MouseX = mouseX;
        m_MouseY = mouseY;
    }

    void WidgetRenderer::DrawQuad(const Rect &rect, float rotation, const glm::vec4 &color, const Ref<Texture> &texture, const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &tilingFactor)
    {
        uint32_t vertexOffset = static_cast<uint32_t>(m_QuadBatch.vertexBufferPtr - m_QuadBatch.vertexBufferBase);
        if (vertexOffset + 4 > m_QuadBatch.maxVertices || m_QuadBatch.indexCount + 6 > m_QuadBatch.maxIndices)
        {
            return;
        }

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
            m_QuadBatch.vertexBufferPtr->tilingFactor = tilingFactor;
            m_QuadBatch.vertexBufferPtr->color = color;
            m_QuadBatch.vertexBufferPtr->texIndex = texIndex;
            m_QuadBatch.vertexBufferPtr++;
        }

        *m_QuadIndicesPtr++ = vertexOffset + 0;
        *m_QuadIndicesPtr++ = vertexOffset + 1;
        *m_QuadIndicesPtr++ = vertexOffset + 2;
        *m_QuadIndicesPtr++ = vertexOffset + 0;
        *m_QuadIndicesPtr++ = vertexOffset + 3;
        *m_QuadIndicesPtr++ = vertexOffset + 1;

        m_QuadBatch.indexCount += 6;
        m_QuadBatch.count++;
    }

    void WidgetRenderer::DrawRoundedQuad(const Rect &rect, float cornerRadius, const glm::vec4 &color, const Ref<Texture> &texture, const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &tilingFactor)
    {
        float r = std::min({ cornerRadius, rect.GetSize().x * 0.5f, rect.GetSize().y * 0.5f });
        if (r <= 0.0f)
        {
            DrawQuad(rect, 0.0f, color, texture, uv0, uv1, tilingFactor);
            return;
        }

        uint32_t texIndex = GetOrInsertQuadTexture(texture);

        const int segments = 12;
        std::vector<glm::vec2> points;
        points.reserve(48);

        auto addCornerArc = [&](const glm::vec2 &center, float startAngle, float endAngle) {
            for (int i = 0; i < segments; ++i) {
                float theta = startAngle + (endAngle - startAngle) * (static_cast<float>(i) / segments);
                points.push_back(center + glm::vec2(r * cosf(theta), r * sinf(theta)));
            }
        };

        // Top-Left: PI to 1.5 * PI
        addCornerArc(glm::vec2(rect.min.x + r, rect.min.y + r), 3.14159265f, 4.71238898f);
        // Top-Right: 1.5 * PI to 2.0 * PI
        addCornerArc(glm::vec2(rect.max.x - r, rect.min.y + r), 4.71238898f, 6.28318531f);
        // Bottom-Right: 0 to 0.5 * PI
        addCornerArc(glm::vec2(rect.max.x - r, rect.max.y - r), 0.0f, 1.57079633f);
        // Bottom-Left: 0.5 * PI to PI
        addCornerArc(glm::vec2(rect.min.x + r, rect.max.y - r), 1.57079633f, 3.14159265f);

        uint32_t K = static_cast<uint32_t>(points.size());
        uint32_t vertexOffset = static_cast<uint32_t>(m_QuadBatch.vertexBufferPtr - m_QuadBatch.vertexBufferBase);

        if (vertexOffset + K + 1 > m_QuadBatch.maxVertices || m_QuadBatch.indexCount + K * 3 > m_QuadBatch.maxIndices)
        {
            return;
        }

        // Center point
        glm::vec2 centerPos = (rect.min + rect.max) * 0.5f;
        m_QuadBatch.vertexBufferPtr->position = glm::vec4(centerPos.x, centerPos.y, 0.0f, 1.0f);
        m_QuadBatch.vertexBufferPtr->texCoord = glm::vec2(0.5f, 0.5f);
        m_QuadBatch.vertexBufferPtr->tilingFactor = tilingFactor;
        m_QuadBatch.vertexBufferPtr->color = color;
        m_QuadBatch.vertexBufferPtr->texIndex = texIndex;
        m_QuadBatch.vertexBufferPtr++;

        // Contour points
        for (const auto &p : points) {
            float u = uv0.x + (p.x - rect.min.x) / rect.GetSize().x * (uv1.x - uv0.x);
            float v = uv0.y + (p.y - rect.min.y) / rect.GetSize().y * (uv1.y - uv0.y);

            m_QuadBatch.vertexBufferPtr->position = glm::vec4(p.x, p.y, 0.0f, 1.0f);
            m_QuadBatch.vertexBufferPtr->texCoord = glm::vec2(u, v);
            m_QuadBatch.vertexBufferPtr->tilingFactor = tilingFactor;
            m_QuadBatch.vertexBufferPtr->color = color;
            m_QuadBatch.vertexBufferPtr->texIndex = texIndex;
            m_QuadBatch.vertexBufferPtr++;
        }

        // Indices
        for (uint32_t i = 0; i < K - 1; ++i) {
            *m_QuadIndicesPtr++ = vertexOffset + 0;
            *m_QuadIndicesPtr++ = vertexOffset + 1 + i;
            *m_QuadIndicesPtr++ = vertexOffset + 2 + i;
        }
        *m_QuadIndicesPtr++ = vertexOffset + 0;
        *m_QuadIndicesPtr++ = vertexOffset + K;
        *m_QuadIndicesPtr++ = vertexOffset + 1;

        m_QuadBatch.indexCount += K * 3;
        m_QuadBatch.count++;
    }

    void WidgetRenderer::DrawRoundedBorder(const Rect &rect, float cornerRadius, float borderWidth, const glm::vec4 &borderColor, const glm::vec2 &tilingFactor)
    {
        if (borderWidth <= 0.0f || borderColor.a <= 0.0f)
            return;

        uint32_t texIndex = 0; // Solid color border

        float r_out = std::min({ cornerRadius, rect.GetSize().x * 0.5f, rect.GetSize().y * 0.5f });

        std::vector<glm::vec2> outerPoints;
        std::vector<glm::vec2> innerPoints;
        outerPoints.reserve(32);
        innerPoints.reserve(32);

        if (r_out <= 0.0f)
        {
            outerPoints = {
                { rect.min.x, rect.min.y },
                { rect.max.x, rect.min.y },
                { rect.max.x, rect.max.y },
                { rect.min.x, rect.max.y }
            };

            Rect innerRect = { rect.min + glm::vec2(borderWidth), rect.max - glm::vec2(borderWidth) };
            innerPoints = {
                { innerRect.min.x, innerRect.min.y },
                { innerRect.max.x, innerRect.min.y },
                { innerRect.max.x, innerRect.max.y },
                { innerRect.min.x, innerRect.max.y }
            };
        }
        else
        {
            float r_in = std::max(0.0f, r_out - borderWidth);
            const int segments = 12;

            auto addCornerArcs = [&](const glm::vec2 &center, float startAngle, float endAngle) {
                for (int i = 0; i < segments; ++i) {
                    float theta = startAngle + (endAngle - startAngle) * (static_cast<float>(i) / segments);
                    float ct = cosf(theta);
                    float st = sinf(theta);
                    outerPoints.push_back(center + glm::vec2(r_out * ct, r_out * st));
                    innerPoints.push_back(center + glm::vec2(r_in * ct, r_in * st));
                }
            };

            // Top-Left
            addCornerArcs(glm::vec2(rect.min.x + r_out, rect.min.y + r_out), 3.14159265f, 4.71238898f);
            // Top-Right
            addCornerArcs(glm::vec2(rect.max.x - r_out, rect.min.y + r_out), 4.71238898f, 6.28318531f);
            // Bottom-Right
            addCornerArcs(glm::vec2(rect.max.x - r_out, rect.max.y - r_out), 0.0f, 1.57079633f);
            // Bottom-Left
            addCornerArcs(glm::vec2(rect.min.x + r_out, rect.max.y - r_out), 1.57079633f, 3.14159265f);
        }

        uint32_t K = static_cast<uint32_t>(outerPoints.size());
        uint32_t vertexOffset = static_cast<uint32_t>(m_QuadBatch.vertexBufferPtr - m_QuadBatch.vertexBufferBase);

        if (vertexOffset + K * 2 > m_QuadBatch.maxVertices || m_QuadBatch.indexCount + K * 6 > m_QuadBatch.maxIndices)
        {
            return;
        }

        const glm::vec2 rectSize = glm::max(rect.GetSize(), glm::vec2(1.0f));

        // Outer vertices
        for (uint32_t i = 0; i < K; ++i) {
            glm::vec2 normUV = (outerPoints[i] - rect.min) / rectSize;
            m_QuadBatch.vertexBufferPtr->position = glm::vec4(outerPoints[i].x, outerPoints[i].y, 0.0f, 1.0f);
            m_QuadBatch.vertexBufferPtr->texCoord = normUV;
            m_QuadBatch.vertexBufferPtr->tilingFactor = tilingFactor;
            m_QuadBatch.vertexBufferPtr->color = borderColor;
            m_QuadBatch.vertexBufferPtr->texIndex = texIndex;
            m_QuadBatch.vertexBufferPtr++;
        }

        // Inner vertices
        for (uint32_t i = 0; i < K; ++i) {
            glm::vec2 normUV = (innerPoints[i] - rect.min) / rectSize;
            m_QuadBatch.vertexBufferPtr->position = glm::vec4(innerPoints[i].x, innerPoints[i].y, 0.0f, 1.0f);
            m_QuadBatch.vertexBufferPtr->texCoord = normUV;
            m_QuadBatch.vertexBufferPtr->tilingFactor = tilingFactor;
            m_QuadBatch.vertexBufferPtr->color = borderColor;
            m_QuadBatch.vertexBufferPtr->texIndex = texIndex;
            m_QuadBatch.vertexBufferPtr++;
        }

        // Indices
        for (uint32_t i = 0; i < K; ++i) {
            uint32_t next = (i + 1) % K;

            *m_QuadIndicesPtr++ = vertexOffset + i;
            *m_QuadIndicesPtr++ = vertexOffset + next;
            *m_QuadIndicesPtr++ = vertexOffset + K + next;

            *m_QuadIndicesPtr++ = vertexOffset + i;
            *m_QuadIndicesPtr++ = vertexOffset + K + next;
            *m_QuadIndicesPtr++ = vertexOffset + K + i;
        }

        m_QuadBatch.indexCount += K * 6;
        m_QuadBatch.count++;
    }

    void WidgetRenderer::DrawString(const std::string &str, const Ref<Font> &font, const glm::vec4 &color, const glm::mat4 &transform, float kerning, float linespacing)
    {
        if (!font || !font->IsReady() || str.empty())
            return;

        Ref<Texture> fontAtlasTexture = font->GetAtlasTexture();
        if (!fontAtlasTexture || !fontAtlasTexture->IsReady())
            return;

        const auto &fontGeometry = font->GetGeometry();
        const auto &metrics = fontGeometry.getMetrics();
        const double metricHeight = metrics.ascenderY - metrics.descenderY;
        if (metricHeight == 0.0)
            return;

        uint32_t texIndex = GetOrInsertFontTexture(fontAtlasTexture);

        double x = 0.0;
        double y = 0.0;
        double fsScale = 1.0 / metricHeight;
        const auto *spaceGlyph = fontGeometry.getGlyph(' ');
        const double spaceGlypAdvance = spaceGlyph ? spaceGlyph->getAdvance() : 0.25;

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

    static void ClearDirtyRecursive(const Ref<IWidgetItem> &item)
    {
        if (!item)
            return;
        item->m_DirtyLayout = false;
        item->m_DirtyPaint = false;
        for (const Ref<IWidgetItem> &child : item->children)
        {
            ClearDirtyRecursive(child);
        }
    }

    void WidgetRenderer::Update(float deltaTime)
    {
        (void)deltaTime;
        BuildRenderLayers();

        const glm::uvec2 mousePos = { m_MouseX, m_MouseY };
        const bool allowPointerInteraction = (InputSystem::GetCursorMode() != CursorMode::Disabled);
        const bool isMousePressed = allowPointerInteraction && InputSystem::IsMouseButtonPressed(Mouse::ButtonLeft);

        const uint32_t canvasW = GetCanvasWidth();
        const uint32_t canvasH = GetCanvasHeight();

        for (const WidgetRenderLayer &layer : m_RenderLayers)
        {
            if (!layer.widget || !layer.widget->IsEnabled())
                continue;

            layer.widget->SetViewportSize(canvasW, canvasH);
            WidgetContainer *root = layer.widget->GetRoot();
            if (!root)
            {
                root = layer.widget->CreateRoot(canvasW, canvasH);
            }
            if (!root)
            {
                continue;
            }

            ResolveWidgetAssetsRecursive(root->As<IWidgetItem>(), AssetManager::GetInstance());

            if (root->m_DirtyLayout)
            {
                MeasureRecursive(root->As<IWidgetItem>());
                root->Arrange(Rect(0.0f, 0.0f, static_cast<float>(canvasW), static_cast<float>(canvasH)));
                ClearDirtyRecursive(root->As<IWidgetItem>());
            }
        }

        bool inputBlocked = !allowPointerInteraction;
        for (auto it = m_RenderLayers.rbegin(); it != m_RenderLayers.rend(); ++it)
        {
            const WidgetRenderLayer &layer = *it;
            if (!layer.widget)
                continue;

            WidgetContainer *root = layer.widget->GetRoot();
            if (!root)
                continue;

            if (!layer.widget->IsEnabled() || inputBlocked)
            {
                ResetInteractionRecursive(root->As<IWidgetItem>());
            }
            else
            {
                UpdateRecursive(root->As<IWidgetItem>(), mousePos, isMousePressed, deltaTime);
                if (layer.blocksWidgetsBelow)
                {
                    inputBlocked = true;
                }
            }
        }
    }

    void WidgetRenderer::Begin(nri::CommandBuffer *cmd)
    {
        m_QuadBatch.indexCount = 0;
        m_QuadBatch.count = 0;
        m_QuadBatch.textureSlotIndex = 1;
        m_QuadBatch.vertexBufferPtr = m_QuadBatch.vertexBufferBase;
        m_QuadIndicesPtr = m_QuadIndicesBase;

        m_TextBatch.indexCount = 0;
        m_TextBatch.count = 0;
        m_TextBatch.textureSlotIndex = 1;
        m_TextBatch.vertexBufferPtr = m_TextBatch.vertexBufferBase;

        m_Cmd = cmd;
    }

    void WidgetRenderer::Flush(RenderTarget *framebuffer)
    {
        if (!m_Cmd || !framebuffer)
            return;

        if (m_QuadBatch.indexCount == 0 && m_TextBatch.indexCount == 0)
            return;

        auto &nri = DeviceManager::GetInstance()->GetNRI();

        const bool startedRendering = !framebuffer->IsRendering();
        if (startedRendering)
            framebuffer->BeginRendering(m_Cmd, false, false);

        if (m_QuadBatch.indexCount > 0)
        {
            const size_t bufferSize = reinterpret_cast<uint8_t *>(m_QuadBatch.vertexBufferPtr) - reinterpret_cast<uint8_t *>(m_QuadBatch.vertexBufferBase);
            m_QuadBatch.vertexBuffer->SetData(Buffer(m_QuadBatch.vertexBufferBase, bufferSize));

            const size_t indexBufferSize = reinterpret_cast<uint8_t *>(m_QuadIndicesPtr) - reinterpret_cast<uint8_t *>(m_QuadIndicesBase);
            m_QuadBatch.indexBuffer->SetData(Buffer(m_QuadIndicesBase, indexBufferSize));

            Ref<GraphicsPipeline> gp = GetWidgetQuadPipelineForFB(framebuffer);
            Ref<BindingSet> bindingSet = GetWidgetBindingSet(gp->GetPipelineLayout(), m_QuadBatch.textureSlots, m_CameraBuffer);

            nri.CmdSetPipelineLayout(*m_Cmd, nri::BindPoint::GRAPHICS, *gp->GetPipelineLayout());
            nri.CmdSetPipeline(*m_Cmd, *gp->GetHandle());

            nri::SetDescriptorSetDesc set0Desc = {};
            set0Desc.setIndex = 0;
            set0Desc.descriptorSet = bindingSet->GetHandle();
            set0Desc.bindPoint = nri::BindPoint::GRAPHICS;
            nri.CmdSetDescriptorSet(*m_Cmd, set0Desc);

            nri::SetDescriptorSetDesc set1Desc = {};
            set1Desc.setIndex = 1;
            set1Desc.descriptorSet = BindlessSystem::GetDescriptorSet();
            set1Desc.bindPoint = nri::BindPoint::GRAPHICS;
            nri.CmdSetDescriptorSet(*m_Cmd, set1Desc);

            nri::VertexBufferDesc vbDesc = {};
            vbDesc.buffer = m_QuadBatch.vertexBuffer->GetHandle();
            vbDesc.offset = 0;
            vbDesc.stride = sizeof(VertexWidgetQuad);
            nri.CmdSetVertexBuffers(*m_Cmd, 0, &vbDesc, 1);
            nri.CmdSetIndexBuffer(*m_Cmd, *m_QuadBatch.indexBuffer->GetHandle(), 0, nri::IndexType::UINT32);

            nri::DrawIndexedDesc drawDesc = {};
            drawDesc.indexNum = m_QuadBatch.indexCount;
            drawDesc.instanceNum = 1;
            nri.CmdDrawIndexed(*m_Cmd, drawDesc);
        }

        if (m_TextBatch.indexCount > 0)
        {
            const size_t bufferSize = reinterpret_cast<uint8_t *>(m_TextBatch.vertexBufferPtr) - reinterpret_cast<uint8_t *>(m_TextBatch.vertexBufferBase);
            m_TextBatch.vertexBuffer->SetData(Buffer(m_TextBatch.vertexBufferBase, bufferSize));

            Ref<GraphicsPipeline> gp = GetWidgetTextPipelineForFB(framebuffer);
            Ref<BindingSet> bindingSet = GetWidgetBindingSet(gp->GetPipelineLayout(), m_TextBatch.textureSlots, m_CameraBuffer);

            nri.CmdSetPipelineLayout(*m_Cmd, nri::BindPoint::GRAPHICS, *gp->GetPipelineLayout());
            nri.CmdSetPipeline(*m_Cmd, *gp->GetHandle());

            nri::SetDescriptorSetDesc set0Desc = {};
            set0Desc.setIndex = 0;
            set0Desc.descriptorSet = bindingSet->GetHandle();
            set0Desc.bindPoint = nri::BindPoint::GRAPHICS;
            nri.CmdSetDescriptorSet(*m_Cmd, set0Desc);

            nri::SetDescriptorSetDesc set1Desc = {};
            set1Desc.setIndex = 1;
            set1Desc.descriptorSet = BindlessSystem::GetDescriptorSet();
            set1Desc.bindPoint = nri::BindPoint::GRAPHICS;
            nri.CmdSetDescriptorSet(*m_Cmd, set1Desc);

            nri::VertexBufferDesc vbDesc = {};
            vbDesc.buffer = m_TextBatch.vertexBuffer->GetHandle();
            vbDesc.offset = 0;
            vbDesc.stride = sizeof(VertexWidgetText);
            nri.CmdSetVertexBuffers(*m_Cmd, 0, &vbDesc, 1);
            nri.CmdSetIndexBuffer(*m_Cmd, *m_TextBatch.indexBuffer->GetHandle(), 0, nri::IndexType::UINT32);

            nri::DrawIndexedDesc drawDesc = {};
            drawDesc.indexNum = m_TextBatch.indexCount;
            drawDesc.instanceNum = 1;
            nri.CmdDrawIndexed(*m_Cmd, drawDesc);
        }

        if (startedRendering)
            framebuffer->EndRendering(m_Cmd);
    }

    void WidgetRenderer::Render(nri::CommandBuffer *cmd, RenderTarget *fb)
    {
        CameraBufferData cameraData = { m_Projection, m_View, {0.0f, 0.0f, 0.0f, 1.0f} };
        m_CameraBuffer->SetData(Buffer(&cameraData, sizeof(cameraData)));

        Begin(cmd);
        RenderWidgetItems();
        Flush(fb);
    }

    void WidgetRenderer::SetCanvasSize(uint32_t width, uint32_t height)
    {
        if (m_CanvasWidth != width || m_CanvasHeight != height)
        {
            m_CanvasWidth = width;
            m_CanvasHeight = height;
            const uint32_t cw = GetCanvasWidth();
            const uint32_t ch = GetCanvasHeight();
            for (const auto &layer : m_RenderLayers)
            {
                if (layer.widget)
                {
                    layer.widget->SetViewportSize(cw, ch);
                }
            }
        }
    }

    uint32_t WidgetRenderer::GetOrInsertQuadTexture(const Ref<Texture> &texture)
    {
        if (texture == nullptr || (texture && !texture->GetHandle()))
            return Renderer::GetWhiteTexture() ? Renderer::GetWhiteTexture()->GetBindlessIndex() : 0;
        return texture->GetBindlessIndex();
    }

    uint32_t WidgetRenderer::GetOrInsertFontTexture(const Ref<Texture> &texture)
    {
        if (texture == nullptr || (texture && !texture->GetHandle()))
            return Renderer::GetWhiteTexture() ? Renderer::GetWhiteTexture()->GetBindlessIndex() : 0;
        return texture->GetBindlessIndex();
    }

    void WidgetRenderer::Resize(uint32_t width, uint32_t height)
    {
        m_Width = width;
        m_Height = height;
        m_Projection = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);
        if (m_CanvasWidth == 0 || m_CanvasHeight == 0)
        {
            for (const auto &layer : m_RenderLayers)
            {
                if (layer.widget)
                {
                    layer.widget->SetViewportSize(width, height);
                }
            }
        }
    }

    void WidgetRenderer::BuildRenderLayers()
    {
        m_RenderLayers.clear();

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

            AssetManager *assetManager = AssetManager::GetInstance();
            for (const WidgetChildEntry &child : widget->GetChildWidgets())
            {
                if (!child.enabled || child.handle == AssetHandle(0) || !assetManager)
                    continue;

                Ref<WidgetCanvas> childWidget = assetManager->GetAsset<WidgetCanvas>(child.handle);
                if (!childWidget)
                    continue;

                collectWidget(childWidget, child.blockWidgetsBelow);
            }
        };

        collectWidget(m_ActiveWidget, m_ActiveWidget->BlocksWidgetsBelow());
    }

    void WidgetRenderer::RenderWidgetItems()
    {
        for (const WidgetRenderLayer &layer : m_RenderLayers)
        {
            if (!layer.widget || !layer.widget->IsEnabled())
                continue;

            WidgetContainer *root = layer.widget->GetRoot();
            if (!root)
            {
                root = layer.widget->CreateRoot(GetCanvasWidth(), GetCanvasHeight());
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
        m_QuadBatch.minCount = 2048;
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

        m_QuadIndicesBase = new uint32_t[m_QuadBatch.maxIndices];
        m_QuadIndicesPtr = m_QuadIndicesBase;

        m_QuadBatch.textureSlots.resize(32);
        m_QuadBatch.textureSlots[0] = Renderer::GetWhiteTexture();
    }

    void WidgetRenderer::InitTextData()
    {
        m_TextBatch.minCount = 2048;
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

        m_TextBatch.indexBuffer->SetData(Buffer(indices.data(), indices.size() * sizeof(uint32_t)));
    }

    Ref<WidgetRenderer> WidgetRenderer::Create(uint32_t width, uint32_t height)
    {
        return CreateRef<WidgetRenderer>(width, height);
    }
}
