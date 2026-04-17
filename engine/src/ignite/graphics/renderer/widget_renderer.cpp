// Copyright (c) 2026 Evangelion Manuhutu

#include "widget_renderer.hpp"
#include "renderer_2d.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/core/input/input.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/project/project.hpp"
#include "ignite/graphics/font.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <unordered_set>
#include <limits>

namespace ignite
{
    WidgetRenderer::WidgetRenderer(uint32_t width, uint32_t height)
        : m_Width(width), m_Height(height), m_MouseX(0), m_MouseY(0)
    {
        m_Renderer2D = Renderer2D::Create();
        m_Projection = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);

        m_CameraBuffer = ConstantBuffer::Create(sizeof(CameraBufferData), false, 1, "[WidgetRenderer] Camera buffer");
    }

    WidgetRenderer::~WidgetRenderer()
    {
        m_CameraBuffer = nullptr;
    }

    void WidgetRenderer::SetMousePosition(uint32_t mouseX, uint32_t mouseY)
    {
        m_MouseX = mouseX;
        m_MouseY = mouseY;
    }

    void WidgetRenderer::Update(float deltaTime)
    {
        BuildRenderLayers();
        if (!m_Project || (!m_PreviewWidget && !m_Scene))
        {
            return;
        }

        const glm::uvec2 mousePos = { m_MouseX, m_MouseY };
        const glm::uvec2 offscreenMousePos = { std::numeric_limits<uint32_t>::max() / 2u, std::numeric_limits<uint32_t>::max() / 2u };
        const bool mousePressed = Input::IsMouseButtonPressed(Mouse::ButtonLeft);

        bool blockedByTopWidget = false;
        for (auto it = m_RenderLayers.rbegin(); it != m_RenderLayers.rend(); ++it)
        {
            if (!it->widget || !it->widget->IsEnabled())
            {
                continue;
            }

            it->widget->SetViewportSize(m_Width, m_Height);
            it->widget->SetScene(m_Scene);

            const bool acceptsInput = !blockedByTopWidget;
            it->widget->Update(deltaTime, acceptsInput ? mousePos : offscreenMousePos);

            if (acceptsInput && mousePressed != m_LastMousePressed)
            {
                for (auto &[id, item] : it->widget->GetItems())
                {
                    if (!item || !item->IsVisible())
                    {
                        continue;
                    }

                    if (Ref<WidgetButton> button = item->As<WidgetButton>())
                    {
                        button->OnMouseClick(mousePos, mousePressed);
                    }
                }
            }

            if (it->blocksWidgetsBelow)
            {
                blockedByTopWidget = true;
            }
        }

        m_LastMousePressed = mousePressed;
    }

    void WidgetRenderer::Render(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *fb)
    {
        CameraBufferData cameraData = { m_Projection, glm::mat4(1.0f), {0.0f, 0.0f, 0.0f, 1.0f} };
        m_CameraBuffer->SetData(cmd, Buffer(&cameraData, sizeof(cameraData)));

        m_Renderer2D->Begin(cmd);

        RenderWidgetItems();

        m_Renderer2D->Flush(fb, m_CameraBuffer);
        m_Renderer2D->End();
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
        std::function<void(const Ref<Widget> &, bool)> collectWidget = [&](const Ref<Widget> &widget, bool blocksLower)
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
                {
                    continue;
                }

                Ref<Widget> childWidget = m_Project->GetAsset<Widget>(child.handle, AssetType::Widget);
                if (!childWidget)
                {
                    childWidget = m_Project->GetAssetImmediate<Widget>(child.handle, AssetType::Widget);
                }

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
            {
                continue;
            }

            Ref<Widget> widget = m_Project->GetAsset<Widget>(widgetComp.widgetHandle, AssetType::Widget);
            if (!widget)
            {
                widget = m_Project->GetAssetImmediate<Widget>(widgetComp.widgetHandle, AssetType::Widget);
            }

            collectWidget(widget, widget ? widget->BlocksWidgetsBelow() : false);
        }
    }

    void WidgetRenderer::RenderWidgetItems()
    {
        if (!m_Project)
        {
            return;
        }

        for (const WidgetRenderLayer &layer : m_RenderLayers)
        {
            if (!layer.widget || !layer.widget->IsEnabled())
            {
                continue;
            }

            for (const auto &[id, widgetItem] : layer.widget->GetItems())
            {
                if (!widgetItem || !widgetItem->IsVisible())
                {
                    continue;
                }

                // Render different widget types
                if (Ref<WidgetButton> button = widgetItem->As<WidgetButton>())
                {
                    const Rect &rect = button->GetAlignedRect();
                    const glm::vec4 &buttonColor = button->GetCurrentColor();

                    Ref<Texture> image = button->GetImage();
                    if (!image && button->GetImageHandle() != AssetHandle(0))
                    {
                        image = m_Project->GetAsset<Texture>(button->GetImageHandle(), AssetType::Texture);
                        if (!image)
                        {
                            image = m_Project->GetAssetImmediate<Texture>(button->GetImageHandle(), AssetType::Texture);
                        }
                    }

                    m_Renderer2D->DrawQuad(rect, 0.0f, buttonColor, image, { 0.0f, 1.0f }, { 1.0f, 0.0f });
                }
                else if (Ref<WidgetText> text = widgetItem->As<WidgetText>())
                {
                    if (text->GetFontHandle() == AssetHandle(0))
                    {
                        continue;
                    }

                    Ref<Font> font = m_Project->GetAsset<Font>(text->GetFontHandle(), AssetType::Font);
                    if (!font)
                    {
                        font = m_Project->GetAssetImmediate<Font>(text->GetFontHandle(), AssetType::Font);
                    }
                    if (!font)
                    {
                        continue;
                    }

                    const Rect rect = text->GetAlignedRect();
                    glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(rect.min, 0.0f));
                    m_Renderer2D->DrawString(text->GetText(), font, text->GetColor(), transform, text->GetKerning(), text->GetLineSpacing());
                }
            }
        }
    }

    Ref<WidgetRenderer> WidgetRenderer::Create(uint32_t width, uint32_t height)
    {
        return CreateRef<WidgetRenderer>(width, height);
    }
}