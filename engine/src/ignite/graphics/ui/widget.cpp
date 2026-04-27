// Copyright (c) 2026 Evangelion Manuhutu

#include "widget.hpp"
#include "widget_container.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/core/input/input.hpp"
#include "ignite/graphics/font.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/graphics/texture.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <unordered_map>

namespace ignite
{
    // =====================================
    // CANVAS IMPLEMENTATION
    WidgetCanvas::WidgetCanvas(Scene *scene)
        : m_Scene(scene), m_ViewportSize({ 1280, 720 })
    {
    }

    WidgetCanvas::~WidgetCanvas()
    {
    }

    bool WidgetCanvas::Serialize(const std::filesystem::path &filepath)
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
        for (const auto &[_, item] : m_WidgetItems)
        {
            if (!item)
            {
                continue;
            }

            sr.BeginMap();
            sr.AddKeyValue("Name", item->name);
            sr.AddKeyValue("ID", item->id);
            sr.AddKeyValue("ParentID", item->parent ? item->parent->id : 0);
            sr.AddKeyValue("Type", static_cast<int>(item->GetWidgetType()));
            sr.AddKeyValue("Position", item->position);
            sr.AddKeyValue("Size", item->size);
            sr.AddKeyValue("Alignment", static_cast<int>(item->alignment));
            sr.AddKeyValue("SizingMode", static_cast<int>(item->sizingMode));
            sr.AddKeyValue("Visible", item->visible);
            sr.AddKeyValue("ZIndex", item->zIndex);

            if (item->GetWidgetType() == WidgetType::Container)
            {
                if (const Ref<WidgetContainer> container = item->As<WidgetContainer>())
                {
                    sr.AddKeyValue("Layout", static_cast<int>(container->layout));
                    sr.AddKeyValue("Padding", container->padding);
                    sr.AddKeyValue("Gap", container->gap);
                }
            }

            if (item->GetWidgetType() == WidgetType::Button)
            {
                if (const Ref<WidgetButton> button = item->As<WidgetButton>())
                {
                    sr.AddKeyValue("ImageHandle", static_cast<uint64_t>(button->imageHandle));
                    sr.AddKeyValue("NormalColor", button->normalColor);
                    sr.AddKeyValue("HoverColor", button->hoverColor);
                    sr.AddKeyValue("PressedColor", button->pressedColor);
                    sr.AddKeyValue("BorderColor", button->borderColor);
                    
                    if (button->label)
                    {
                        sr.AddKeyValue("Text", button->label->text);
                        sr.AddKeyValue("FontHandle", static_cast<uint64_t>(button->label->fontHandle));
                        sr.AddKeyValue("Color", button->label->color);
                        sr.AddKeyValue("FontSize", button->label->fontSize);
                        sr.AddKeyValue("Kerning", button->label->kerning);
                        sr.AddKeyValue("LineSpacing", button->label->lineSpacing);
                    }
                    
                }
            }

            if (item->GetWidgetType() == WidgetType::Label)
            {
                if (const Ref<WidgetLabel> label = item->As<WidgetLabel>())
                {
                    sr.AddKeyValue("Text", label->text);
                    sr.AddKeyValue("Color", label->color);
                    sr.AddKeyValue("FontHandle", static_cast<uint64_t>(label->fontHandle));
                    sr.AddKeyValue("FontSize", label->fontSize);
                    sr.AddKeyValue("Kerning", label->kerning);
                    sr.AddKeyValue("LineSpacing", label->lineSpacing);
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

    Ref<WidgetCanvas> WidgetCanvas::Deserialize(const std::filesystem::path &filepath)
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

        Ref<WidgetCanvas> widget = CreateRef<WidgetCanvas>(nullptr);
        if (widgetNode["Name"]) widget->name = widgetNode["Name"].as<std::string>();
        if (widgetNode["Enabled"]) widget->m_Enabled = widgetNode["Enabled"].as<bool>();
        if (widgetNode["BlocksWidgetsBelow"]) widget->m_BlocksWidgetsBelow = widgetNode["BlocksWidgetsBelow"].as<bool>();
        if (widgetNode["NextItemId"]) widget->m_NextWidgetItemId = widgetNode["NextItemId"].as<int>();

        widget->m_WidgetItems.clear();
        std::unordered_map<int, int> parentMap;
        if (YAML::Node itemsNode = widgetNode["Items"]; itemsNode && itemsNode.IsSequence())
        {
            for (const YAML::Node &itemNode : itemsNode)
            {
                if (!itemNode["ID"] || !itemNode["Type"])
                {
                    continue;
                }

                const int id = itemNode["ID"].as<int>();
                const int parentId = itemNode["ParentID"] ? itemNode["ParentID"].as<int>() : 0;
                const WidgetType type = static_cast<WidgetType>(itemNode["Type"].as<int>());
                

                Ref<IWidgetItem> item = nullptr;

                switch (type)
                {
                    case WidgetType::Container:
                        item = CreateRef<WidgetContainer>();
                        break;
                    case WidgetType::Button:
                        item = CreateRef<WidgetButton>("");
                        break;
                    case WidgetType::Label:
                        item = CreateRef<WidgetLabel>("");
                        break;
                    default:
                        break;
                }

                if (!item)
                    continue;

                item->id = id;
                if (auto n = itemNode["Name"]) item->name = n.as<std::string>();
                if (auto n = itemNode["Position"]) item->position = n.as<glm::vec2>();
                if (auto n = itemNode["ZIndex"])    item->zIndex   = n.as<int>();
                if (auto n = itemNode["Size"]) item->size = n.as<glm::vec2>();
                if (auto n = itemNode["Alignment"]) item->alignment = static_cast<WidgetAlignment>(n.as<int>());
                if (auto n = itemNode["SizingMode"]) item->sizingMode = static_cast<SizingMode>(n.as<int>());
                if (auto n = itemNode["Visible"]) item->visible = n.as<bool>();

                if (type == WidgetType::Container)
                {
                    if (Ref<WidgetContainer> container = item->As<WidgetContainer>())
                    {
                        if (itemNode["Layout"]) container->layout = static_cast<LayoutMode>(itemNode["Layout"].as<int>());
                        if (itemNode["Padding"]) container->padding = itemNode["Padding"].as<float>();
                        if (itemNode["Gap"]) container->gap = itemNode["Gap"].as<float>();
                    }
                }

                if (type == WidgetType::Button)
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
                        if (itemNode["FontHandle"]) button->SetFontHandle(AssetHandle(itemNode["FontHandle"].as<uint64_t>()));
                        if (itemNode["FontSize"]) button->SetFontSize(itemNode["FontSize"].as<float>());
                        if (itemNode["Kerning"]) button->SetKerning(itemNode["Kerning"].as<float>());
                        if (itemNode["LineSpacing"]) button->SetLineSpacing(itemNode["LineSpacing"].as<float>());
                    }
                }
                else if (type == WidgetType::Label)
                {
                    if (Ref<WidgetLabel> text = item->As<WidgetLabel>())
                    {
                        if (itemNode["Text"]) text->SetText(itemNode["Text"].as<std::string>());
                        if (itemNode["Color"]) text->SetColor(itemNode["Color"].as<glm::vec4>());
                        if (itemNode["FontHandle"]) text->SetFontHandle(AssetHandle(itemNode["FontHandle"].as<uint64_t>()));
                        if (itemNode["Kerning"]) text->SetKerning(itemNode["Kerning"].as<float>());
                        if (itemNode["LineSpacing"]) text->SetLineSpacing(itemNode["LineSpacing"].as<float>());
                    }
                }

                widget->m_WidgetItems[id] = item;
                parentMap[id] = parentId;
                widget->m_NextWidgetItemId = std::max(widget->m_NextWidgetItemId, id + 1);
            }
        }

        for (auto &[id, item] : widget->m_WidgetItems)
        {
            if (!item)
            {
                continue;
            }

            const auto parentIt = parentMap.find(id);
            if (parentIt == parentMap.end() || parentIt->second == 0)
            {
                if (item->GetWidgetType() == WidgetType::Container && !widget->m_Root)
                {
                    widget->m_Root = item->As<WidgetContainer>();
                }
                continue;
            }

            const auto ownerIt = widget->m_WidgetItems.find(parentIt->second);
            if (ownerIt == widget->m_WidgetItems.end() || !ownerIt->second)
            {
                continue;
            }

            item->parent = ownerIt->second.get();
            ownerIt->second->children.push_back(item);
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

    int WidgetCanvas::GetNextItemId()
    {
        while (m_WidgetItems.contains(m_NextWidgetItemId))
        {
            ++m_NextWidgetItemId;
        }
        return m_NextWidgetItemId++;
    }

    int WidgetCanvas::AddButton(WidgetContainer *container, const std::string &text)
    {
        const int id = GetNextItemId();
        Ref<WidgetButton> button = CreateRef<WidgetButton>(text);
        button->id = id;

        if (container)
        {
            button->parent = container;
            container->children.push_back(button);
        }

        m_WidgetItems[id] = button;
        return id;
    }

    int WidgetCanvas::AddLabel(WidgetContainer *container, const std::string &text)
    {
        const int id = GetNextItemId();
        Ref<WidgetLabel> label = CreateRef<WidgetLabel>(text);
        label->id = id;

        if (container)
        {
            label->parent = container;
            container->children.push_back(label);
        }

        m_WidgetItems[id] = label;
        return id;
    }

    int WidgetCanvas::AddContainer(WidgetContainer *container)
    {
        const int id = GetNextItemId();
        Ref<WidgetContainer> child = CreateRef<WidgetContainer>();
        child->id = id;

        if (container)
        {
            child->parent = container;
            container->children.push_back(child);
        }

        m_WidgetItems[id] = child;
        return id;
    }

    WidgetContainer *WidgetCanvas::CreateRoot(uint32_t width, uint32_t height)
    {
        if (!m_Root)
        {
            const int id = GetNextItemId();
            m_Root = CreateRef<WidgetContainer>();
            m_Root->id = id;
            m_Root->sizingMode = SizingMode::ExpandToParent;
            m_Root->position = glm::vec2(0.0f);
            m_Root->size = glm::vec2(static_cast<float>(width), static_cast<float>(height));
            m_WidgetItems[id] = m_Root;
        }

        return m_Root.get();
    }

    bool WidgetCanvas::RemoveItem(int id)
    {
        const auto it = m_WidgetItems.find(id);
        if (it == m_WidgetItems.end())
        {
            return false;
        }

        const Ref<IWidgetItem> item = it->second;
        if (item && item->parent)
        {
            auto &siblings = item->parent->children;
            siblings.erase(std::remove_if(siblings.begin(), siblings.end(), [&](const Ref<IWidgetItem> &child)
            {
                return child.get() == item.get();
            }), siblings.end());
        }

        if (item && m_Root && m_Root.get() == item.get())
        {
            m_Root = nullptr;
        }

        return m_WidgetItems.erase(id) > 0;
    }

    // ===================================
    // LABEL IMPLEMENTATION
    WidgetLabel::WidgetLabel(const std::string &text)
        : text(text)
    {
        color = UI_COLOR_WHITE;
    }

    void WidgetLabel::Measure()
    {
        if (!font || text.empty())
        {
            size = glm::vec2(0.0f);
            return;
        }

        const auto &fontGeometry = font->GetGeometry();
        const auto &metrics = fontGeometry.getMetrics();
        double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);

        double x = 0.0;
        double maxWidth = 0.0;
        int lines = 1;

        for (size_t i = 0; i < text.size(); ++i)
        {
            char character = text[i];
            if (character == '\n')
            {
                maxWidth = std::max(maxWidth, x);
                x = 0.0;
                lines++;
                continue;
            }

            auto glyph = fontGeometry.getGlyph(character);
            if (!glyph) glyph = fontGeometry.getGlyph('?');
            if (!glyph) continue;

            double advance = glyph->getAdvance();
            if (i < text.size() - 1)
            {
                fontGeometry.getAdvance(advance, character, text[i + 1]);
            }
            x += fsScale * advance + kerning;
        }

        maxWidth = std::max(maxWidth, x);
        
        size.x = static_cast<float>(maxWidth) * fontSize;
        size.y = static_cast<float>(lines) * (static_cast<float>(metrics.lineHeight * fsScale) + lineSpacing) * fontSize;
    }

    void WidgetLabel::Arrange(const Rect &parentRect)
    {
        worldRect = CalculateAlignedRect(parentRect);
    }

    bool WidgetLabel::HitTest(int px, int py)
    {
        return worldRect.Contains(glm::vec2(static_cast<float>(px), static_cast<float>(py)));
    }

    // ==================================
    // BUTTON IMPLEMENTATION
    WidgetButton::WidgetButton(const std::string &text)
        : label(CreateScope<WidgetLabel>(text))
    {
        normalColor = UI_COLOR_DARK_GRAY;
        hoverColor = UI_COLOR_GRAY;
        pressedColor = UI_COLOR_DARK_GRAY;
        borderColor = UI_COLOR_GRAY;
    }

    WidgetButton::~WidgetButton()
    {
    }

    const glm::vec4 &WidgetButton::GetCurrentColor() const
    {
        if (pressed)
            return pressedColor;
        if (hovered)
            return hoverColor;
        return normalColor;
    }

    void WidgetButton::SetText(const std::string &text)
    {
        if (label)
        {
            label->SetText(text);
        }
    }

    const std::string &WidgetButton::GetText() const
    {
        static const std::string empty;
        return label ? label->GetText() : empty;
    }

    void WidgetButton::SetColors(const glm::vec4 &normal, const glm::vec4 &hover, const glm::vec4 &pressedColor_)
    {
        normalColor = normal;
        hoverColor = hover;
        pressedColor = pressedColor_;
    }

    void WidgetButton::SetTextColor(const glm::vec4 &textColor)
    {
        if (label)
        {
            label->SetColor(textColor);
        }
    }

    const glm::vec4 &WidgetButton::GetTextColor() const
    {
        static const glm::vec4 fallbackTextColor = glm::vec4(UI_COLOR_WHITE);
        return label ? label->GetColor() : fallbackTextColor;
    }

    void WidgetButton::SetFontHandle(AssetHandle handle)
    {
        if (label)
        {
            label->SetFontHandle(handle);
        }
    }

    AssetHandle WidgetButton::GetFontHandle() const
    {
        return label ? label->GetFontHandle() : AssetHandle(0);
    }

    void WidgetButton::SetFontSize(float newFontSize)
    {
        if (label)
        {
            label->SetFontSize(newFontSize);
        }
    }

    float WidgetButton::GetFontSize() const
    {
        return label ? label->GetFontSize() : 16.0f;
    }

    void WidgetButton::SetKerning(float newKerning)
    {
        if (label)
        {
            label->SetKerning(newKerning);
        }
    }

    float WidgetButton::GetKerning() const
    {
        return label ? label->GetKerning() : 0.0f;
    }

    void WidgetButton::SetLineSpacing(float newLineSpacing)
    {
        if (label)
        {
            label->SetLineSpacing(newLineSpacing);
        }
    }

    float WidgetButton::GetLineSpacing() const
    {
        return label ? label->GetLineSpacing() : -0.025f;
    }

    void WidgetButton::OnMouseClick(const glm::uvec2 &mousePos, bool isPressed)
    {
        const bool contains = HitTest(static_cast<int>(mousePos.x), static_cast<int>(mousePos.y));

        if (contains != hovered)
        {
            if (contains)
            {
                if (IWidgetItem::m_OnHoverEnter)
                {
                    IWidgetItem::m_OnHoverEnter();
                }
            }
            else if (IWidgetItem::m_OnHoverExit)
            {
                IWidgetItem::m_OnHoverExit();
            }
        }

        hovered = contains;

        if (contains && isPressed && !pressed)
        {
            pressed = true;
            if (IWidgetItem::m_OnPressed)
            {
                IWidgetItem::m_OnPressed();
            }
            return;
        }

        if (pressed && !isPressed)
        {
            pressed = false;
            if (contains && IWidgetItem::m_OnClick)
            {
                IWidgetItem::m_OnClick();
            }

            if (IWidgetItem::m_OnReleased)
            {
                IWidgetItem::m_OnReleased();
            }
        }
    }

    void WidgetButton::Measure()
    {
        if (label)
            label->Measure();
    }

    void WidgetButton::Arrange(const Rect &parentRect)
    {
        worldRect = CalculateAlignedRect(parentRect);
    }

    bool WidgetButton::HitTest(int px, int py)
    {
        return worldRect.Contains(glm::vec2(static_cast<float>(px), static_cast<float>(py)));
    }
}
