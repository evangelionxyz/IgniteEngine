// Copyright (c) 2026 Evangelion Manuhutu

#include "widget.hpp"
#include "ignite/serializer/serializer.hpp"

#include <algorithm>

namespace ignite
{
    IWidgetItem::IWidgetItem(Widget *widget)
        : m_Widget(widget)
    {

    }

    Rect IWidgetItem::GetAlignedRect() const
    {
        Rect alignedRect = m_Rect;
        glm::vec2 &position = alignedRect.min;
        glm::vec2 &size = alignedRect.max;
        
        const auto &vpSizeI = m_Widget->GetViewportSize();
        const glm::vec2 viewportSize = { static_cast<float>(vpSizeI.x), static_cast<float>(vpSizeI.y) };
        
        switch (m_Alignment)
        {
            case WidgetAlignment::TOP_CENTER:
                position.x = viewportSize.x / 2.0f + m_Rect.min.x - m_Rect.GetSize().x / 2.0f;
                size.x = viewportSize.x / 2.0f + m_Rect.min.x + m_Rect.GetSize().x / 2.0f;
            break;
            case WidgetAlignment::TOP_RIGHT:
                position.x = viewportSize.x - m_Rect.min.x - m_Rect.GetSize().x;
                size.x = viewportSize.x - m_Rect.min.x;
                break;
            case WidgetAlignment::CENTER_LEFT:
                position.y = viewportSize.y / 2.0f + m_Rect.min.y - m_Rect.GetSize().y / 2.0f;
                size.y = viewportSize.y / 2.0f + m_Rect.min.y + m_Rect.GetSize().y / 2.0f;
                break;
            case WidgetAlignment::CENTER:
                position.x = viewportSize.x / 2.0f + m_Rect.min.x - m_Rect.GetSize().x / 2.0f;
                size.x = viewportSize.x / 2.0f + m_Rect.min.x + m_Rect.GetSize().x / 2.0f;
                position.y = viewportSize.y / 2.0f + m_Rect.min.y - m_Rect.GetSize().y / 2.0f;
                size.y = viewportSize.y / 2.0f + m_Rect.min.y + m_Rect.GetSize().y / 2.0f;
                break;
            case WidgetAlignment::CENTER_RIGHT:
                position.x = viewportSize.x - m_Rect.min.x - m_Rect.GetSize().x;
                size.x = viewportSize.x - m_Rect.min.x;
                position.y = viewportSize.y / 2.0f + m_Rect.min.y - m_Rect.GetSize().y / 2.0f;
                size.y = viewportSize.y / 2.0f + m_Rect.min.y + m_Rect.GetSize().y / 2.0f;
                break;
            case WidgetAlignment::BOTTOM_LEFT:
                position.y = viewportSize.y - m_Rect.max.y;
                size.y = viewportSize.y - m_Rect.min.y;
                break;
            case WidgetAlignment::BOTTOM_CENTER:
                position.x = viewportSize.x / 2.0f + m_Rect.min.x - m_Rect.GetSize().x / 2.0f;
                size.x = viewportSize.x / 2.0f + m_Rect.min.x + m_Rect.GetSize().x / 2.0f;
                position.y = viewportSize.y - m_Rect.max.y;
                size.y = viewportSize.y - m_Rect.min.y;
                break;
            case WidgetAlignment::BOTTOM_RIGHT:
                position.x = viewportSize.x - m_Rect.min.x - m_Rect.GetSize().x;
                size.x = viewportSize.x - m_Rect.min.x; 
                position.y = viewportSize.y - m_Rect.max.y;
                size.y = viewportSize.y - m_Rect.min.y;
                break;
            case WidgetAlignment::TOP_LEFT:
            default: break;
        }

        return alignedRect;
    }

    WidgetButton::WidgetButton(Widget *widget, const std::string &text)
        : IWidgetItem(widget), m_Text(text)
    {
        m_NormalColor = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);
        m_HoverColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
        m_PressedColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
        m_TextColor = UI_COLOR_WHITE;
        m_BorderColor = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
    }

    WidgetButton::~WidgetButton()
    {
    }

    void WidgetButton::Update(float deltaTime, const glm::uvec2 &mousePos)
    {
        bool wasHovered = m_IsHovered;
        m_IsHovered = Contains(mousePos);

        if (m_IsHovered && !wasHovered && m_OnHoverEnter)
        {
            m_OnHoverEnter();
        }
        else if (!m_IsHovered && wasHovered && m_OnHoverExit)
        {
            m_OnHoverExit();
        }
    }

    void WidgetButton::OnMouseClick(const glm::uvec2 &mousePos, bool isPressed)
    {
        if (Contains(mousePos))
        {
            if (isPressed)
            {
                m_IsPressed = true;
                if (m_OnPressed)
                {
                    m_OnPressed();
                }
            }
            else if (m_IsPressed)
            {
                m_IsPressed = false;
                if (m_OnClick)
                {
                    m_OnClick();
                }
                if (m_OnReleased)
                {
                    m_OnReleased();
                }
            }
        }
        else if (!isPressed)
        {
            m_IsPressed = false;
        }
    }

    const glm::vec4 &WidgetButton::GetCurrentColor() const
    {
        if (m_IsPressed)
            return m_PressedColor;
        if (m_IsHovered)
            return m_HoverColor;
        return m_NormalColor;
    }

    Widget::Widget(Scene *scene)
        : m_Scene(scene), m_ViewportSize({ 1280, 720 })
    {
    }

    Widget::~Widget()
    {
    }

    void Widget::Update(float deltaTime, const glm::uvec2 &mousePos)
    {
        for (auto &[id, item] : m_WidgetItems)
        {
            if (item && item->IsVisible())
            {
                item->Update(deltaTime, mousePos);
            }
        }
    }

    bool Widget::Serialize(const std::filesystem::path &filepath)
    {
        Serializer sr(filepath);
        sr.BeginMap();

        sr.BeginMap("Widget");
        sr.AddKeyValue("Version", ENGINE_VERSION);
        sr.AddKeyValue("Name", name);
        sr.AddKeyValue("Enabled", m_Enabled);
        sr.AddKeyValue("BlocksWidgetsBelow", m_BlocksWidgetsBelow);
        sr.AddKeyValue("NextItemId", m_NextWidgetItemId);

        sr.BeginSequence("Items");
        for (const auto &[id, item] : m_WidgetItems)
        {
            if (!item)
            {
                continue;
            }

            sr.BeginMap();
            sr.AddKeyValue("ID", id);
            sr.AddKeyValue("Type", static_cast<int>(item->GetItemType()));
            sr.AddKeyValue("Rect", item->GetRect());
            sr.AddKeyValue("Alignment", static_cast<int>(item->GetAlignment()));
            sr.AddKeyValue("Visible", item->IsVisible());

            if (item->GetItemType() == WidgetItemType::Button)
            {
                if (const Ref<WidgetButton> button = item->As<WidgetButton>())
                {
                    sr.AddKeyValue("Text", button->GetText());
                    sr.AddKeyValue("ImageHandle", static_cast<uint64_t>(button->GetImageHandle()));
                    sr.AddKeyValue("NormalColor", button->GetNormalColor());
                    sr.AddKeyValue("HoverColor", button->GetHoverColor());
                    sr.AddKeyValue("PressedColor", button->GetPressedColor());
                    sr.AddKeyValue("TextColor", button->GetTextColor());
                    sr.AddKeyValue("BorderColor", button->GetBorderColor());
                }
            }
            else if (item->GetItemType() == WidgetItemType::Text)
            {
                if (const Ref<WidgetText> text = item->As<WidgetText>())
                {
                    sr.AddKeyValue("Text", text->GetText());
                    sr.AddKeyValue("Color", text->GetColor());
                    sr.AddKeyValue("FontHandle", static_cast<uint64_t>(text->GetFontHandle()));
                    sr.AddKeyValue("Kerning", text->GetKerning());
                    sr.AddKeyValue("LineSpacing", text->GetLineSpacing());
                }
            }

            sr.EndMap();
        }
        sr.EndSequence();

        sr.BeginSequence("ChildWidgets");
        for (const WidgetChildEntry &child : m_ChildWidgets)
        {
            sr.BeginMap();
            sr.AddKeyValue("Handle", static_cast<uint64_t>(child.handle));
            sr.AddKeyValue("Enabled", child.enabled);
            sr.AddKeyValue("BlockWidgetsBelow", child.blockWidgetsBelow);
            sr.EndMap();
        }
        sr.EndSequence();

        sr.EndMap();
        sr.EndMap();

        sr.Serialize();
        SetDirtyFlag(false);
        return true;
    }

    Ref<Widget> Widget::Deserialize(const std::filesystem::path &filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            return nullptr;
        }

        YAML::Node fileNode = Serializer::Deserialize(filepath);
        YAML::Node widgetNode = fileNode["Widget"];
        if (!widgetNode)
        {
            return nullptr;
        }

        Ref<Widget> widget = CreateRef<Widget>(nullptr);
        if (widgetNode["Name"]) widget->name = widgetNode["Name"].as<std::string>();
        if (widgetNode["Enabled"]) widget->m_Enabled = widgetNode["Enabled"].as<bool>();
        if (widgetNode["BlocksWidgetsBelow"]) widget->m_BlocksWidgetsBelow = widgetNode["BlocksWidgetsBelow"].as<bool>();
        if (widgetNode["NextItemId"]) widget->m_NextWidgetItemId = widgetNode["NextItemId"].as<int>();

        widget->m_WidgetItems.clear();
        if (YAML::Node itemsNode = widgetNode["Items"]; itemsNode && itemsNode.IsSequence())
        {
            for (const YAML::Node &itemNode : itemsNode)
            {
                if (!itemNode["ID"] || !itemNode["Type"])
                {
                    continue;
                }

                const int id = itemNode["ID"].as<int>();
                const WidgetItemType type = static_cast<WidgetItemType>(itemNode["Type"].as<int>());
                Ref<IWidgetItem> item = nullptr;

                switch (type)
                {
                    case WidgetItemType::Button: item = CreateRef<WidgetButton>(widget.get()); break;
                    case WidgetItemType::Text: item = CreateRef<WidgetText>(widget.get()); break;
                    default: break;
                }

                if (!item)
                {
                    continue;
                }

                if (itemNode["Rect"])
                {
                    const Rect rect = itemNode["Rect"].as<Rect>();
                    item->SetPosition(rect.min);
                    item->SetSize(rect.GetSize());
                }
                if (itemNode["Alignment"])
                {
                    item->SetAlignment(static_cast<WidgetAlignment>(itemNode["Alignment"].as<int>()));
                }
                if (itemNode["Visible"])
                {
                    item->SetVisible(itemNode["Visible"].as<bool>());
                }

                if (type == WidgetItemType::Button)
                {
                    if (Ref<WidgetButton> button = item->As<WidgetButton>())
                    {
                        if (itemNode["Text"]) button->SetText(itemNode["Text"].as<std::string>());
                        if (itemNode["ImageHandle"]) button->SetImageHandle(AssetHandle(itemNode["ImageHandle"].as<uint64_t>()));

                        glm::vec4 normal = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);
                        glm::vec4 hover = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
                        glm::vec4 pressed = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
                        if (itemNode["NormalColor"]) normal = itemNode["NormalColor"].as<glm::vec4>();
                        if (itemNode["HoverColor"]) hover = itemNode["HoverColor"].as<glm::vec4>();
                        if (itemNode["PressedColor"]) pressed = itemNode["PressedColor"].as<glm::vec4>();
                        button->SetColors(normal, hover, pressed);
                        if (itemNode["TextColor"]) button->SetTextColor(itemNode["TextColor"].as<glm::vec4>());
                        if (itemNode["BorderColor"]) button->SetBorderColor(itemNode["BorderColor"].as<glm::vec4>());
                    }
                }
                else if (type == WidgetItemType::Text)
                {
                    if (Ref<WidgetText> text = item->As<WidgetText>())
                    {
                        if (itemNode["Text"]) text->SetText(itemNode["Text"].as<std::string>());
                        if (itemNode["Color"]) text->SetColor(itemNode["Color"].as<glm::vec4>());
                        if (itemNode["FontHandle"]) text->SetFontHandle(AssetHandle(itemNode["FontHandle"].as<uint64_t>()));
                        if (itemNode["Kerning"]) text->SetKerning(itemNode["Kerning"].as<float>());
                        if (itemNode["LineSpacing"]) text->SetLineSpacing(itemNode["LineSpacing"].as<float>());
                    }
                }

                widget->m_WidgetItems[id] = item;
                widget->m_NextWidgetItemId = std::max(widget->m_NextWidgetItemId, id + 1);
            }
        }

        widget->m_ChildWidgets.clear();
        if (YAML::Node childrenNode = widgetNode["ChildWidgets"]; childrenNode && childrenNode.IsSequence())
        {
            for (const YAML::Node &childNode : childrenNode)
            {
                WidgetChildEntry child;
                if (childNode["Handle"]) child.handle = AssetHandle(childNode["Handle"].as<uint64_t>());
                if (childNode["Enabled"]) child.enabled = childNode["Enabled"].as<bool>();
                if (childNode["BlockWidgetsBelow"]) child.blockWidgetsBelow = childNode["BlockWidgetsBelow"].as<bool>();
                widget->m_ChildWidgets.push_back(child);
            }
        }

        widget->SetDirtyFlag(false);
        widget->SetReadyFlag(true);
        return widget;
    }

    int Widget::GetNextItemId()
    {
        while (m_WidgetItems.contains(m_NextWidgetItemId))
        {
            ++m_NextWidgetItemId;
        }
        return m_NextWidgetItemId++;
    }

    int Widget::AddButton(const std::string &text)
    {
        const int id = GetNextItemId();
        m_WidgetItems[id] = CreateRef<WidgetButton>(this, text);
        return id;
    }

    int Widget::AddText(const std::string &text)
    {
        const int id = GetNextItemId();
        m_WidgetItems[id] = CreateRef<WidgetText>(this, text);
        return id;
    }

    bool Widget::RemoveItem(int id)
    {
        return m_WidgetItems.erase(id) > 0;
    }

}
