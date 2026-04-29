// Copyright (c) 2026 Evangelion Manuhutu

#include "widget_canvas.hpp"
#include "widget_container.hpp"
#include "widget_label.hpp"
#include "widget_button.hpp"

#include "ignite/serializer/serializer.hpp"
#include "ignite/asset/asset_manager.hpp"

namespace ignite
{
    WidgetCanvas::WidgetCanvas(Scene *scene)
        : m_Scene(scene), m_ViewportSize({ 1280, 720 })
    { }

    WidgetCanvas::~WidgetCanvas()
    { }

    WidgetID WidgetCanvas::GetNextItemId()
    {
        while (m_WidgetItems.contains(m_NextWidgetItemId))
        {
            ++m_NextWidgetItemId;
        }
        return m_NextWidgetItemId++;
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
            sr.AddKeyValue("VerticalAlignment", static_cast<int>(item->VAlignment));
            sr.AddKeyValue("HorizontalAligment", static_cast<int>(item->HAlignment));
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
                    sr.AddKeyValue("NormalColor", button->style.color);
                    sr.AddKeyValue("HoverColor", button->style.hoverColor);
                    sr.AddKeyValue("PressedColor", button->style.pressedColor);
                    sr.AddKeyValue("BorderColor", button->style.borderColor);
                    sr.AddKeyValue("CornerRadius", button->style.cornerRadius);

                    if (button->label)
                    {
                        sr.AddKeyValue("FontHandle", static_cast<uint64_t>(button->label->fontHandle));
                        sr.AddKeyValue("Text", button->label->text);
                        sr.AddKeyValue("Color", button->label->style.color);
                        sr.AddKeyValue("FontSize", button->label->style.fontSize);
                        sr.AddKeyValue("Kerning", button->label->style.kerning);
                        sr.AddKeyValue("LineSpacing", button->label->style.lineSpacing);
                    }

                }
            }

            if (item->GetWidgetType() == WidgetType::Label)
            {
                if (const Ref<WidgetLabel> label = item->As<WidgetLabel>())
                {
                    sr.AddKeyValue("Text", label->text);
                    sr.AddKeyValue("Color", label->style.color);
                    sr.AddKeyValue("FontHandle", static_cast<uint64_t>(label->fontHandle));
                    sr.AddKeyValue("FontSize", label->style.fontSize);
                    sr.AddKeyValue("Kerning", label->style.kerning);
                    sr.AddKeyValue("LineSpacing", label->style.lineSpacing);
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

        widget->m_WidgetItems.clear();
        std::unordered_map<int, int> parentMap;
        if (YAML::Node itemsNode = widgetNode["Items"]; itemsNode && itemsNode.IsSequence())
        {
            for (const YAML::Node &itemNode : itemsNode)
            {
                if (!itemNode["ID"] || !itemNode["Type"])
                    continue;

                const WidgetType type = static_cast<WidgetType>(itemNode["Type"].as<int>());
                const WidgetID id = itemNode["ID"].as<int>();
                const WidgetID parentID = itemNode["ParentID"].as<int>();

                // Create item
                Ref<IWidgetItem> item = nullptr;
                switch (type)
                {
                    case WidgetType::Container: item = CreateRef<WidgetContainer>(id); break;
                    case WidgetType::Button: item = CreateRef<WidgetButton>("", id); break;
                    case WidgetType::Label: item = CreateRef<WidgetLabel>("", id); break;
                    default: break;
                }

                if (!item)
                    continue;

                if (auto n = itemNode["Name"]) item->name = n.as<std::string>();
                if (auto n = itemNode["ID"]) item->id = n.as<int>();
                if (auto n = itemNode["Position"]) item->position = n.as<glm::vec2>();
                if (auto n = itemNode["ZIndex"]) item->zIndex = n.as<int>();
                if (auto n = itemNode["Size"]) item->size = n.as<glm::vec2>();
                if (auto n = itemNode["VerticalAlignment"]) item->VAlignment = static_cast<VerticalAlignment>(n.as<int>());
                if (auto n = itemNode["HorizontalAlignment"]) item->HAlignment = static_cast<HorizontalAlignment>(n.as<int>());
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
                        if (auto n = itemNode["ImageHandle"]) button->imageHandle = AssetHandle(n.as<uint64_t>());
                        if (auto n = itemNode["NormalColor"]) button->style.color = n.as<glm::vec4>();
                        if (auto n = itemNode["HoverColor"]) button->style.hoverColor = n.as<glm::vec4>();
                        if (auto n = itemNode["PressedColor"]) button->style.pressedColor = n.as<glm::vec4>();
                        if (auto n = itemNode["BorderColor"]) button->style.borderColor = (n.as<glm::vec4>());
                        if (auto n = itemNode["CornerRadius"]) button->style.cornerRadius = n.as<float>();

                        // Load label
                        if (button->label)
                        {
                            if (auto n = itemNode["FontHandle"]) button->label->fontHandle = AssetHandle(n.as<uint64_t>());
                            if (auto n = itemNode["Text"]) button->label->text = n.as<std::string>();
                            if (auto n = itemNode["TextColor"]) button->label->style.color = n.as<glm::vec4>();
                            if (auto n = itemNode["FontSize"]) button->label->style.fontSize = n.as<float>();
                            if (auto n = itemNode["Kerning"]) button->label->style.kerning = n.as<float>();
                            if (auto n = itemNode["LineSpacing"]) button->label->style.lineSpacing = n.as<float>();
                        }
                    }
                }
                else if (type == WidgetType::Label)
                {
                    if (Ref<WidgetLabel> text = item->As<WidgetLabel>())
                    {
                        if (itemNode["FontHandle"]) text->fontHandle = AssetHandle(itemNode["FontHandle"].as<uint64_t>());
                        if (itemNode["Text"]) text->text = itemNode["Text"].as<std::string>();
                        if (itemNode["Color"]) text->style.color = itemNode["Color"].as<glm::vec4>();
                        if (itemNode["Kerning"]) text->style.kerning = itemNode["Kerning"].as<float>();
                        if (itemNode["LineSpacing"]) text->style.lineSpacing = itemNode["LineSpacing"].as<float>();
                    }
                }

                widget->m_WidgetItems[id] = item;
                parentMap[id] = parentID;
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
                    const glm::vec2 rootSize = widget->m_Root->size;
                    if (rootSize.x > 0.0f && rootSize.y > 0.0f)
                    {
                        widget->m_ViewportSize =
                        {
                            static_cast<uint32_t>(std::max(rootSize.x, 1.0f)),
                            static_cast<uint32_t>(std::max(rootSize.y, 1.0f))
                        };
                    }
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

    WidgetID WidgetCanvas::AddButton(WidgetContainer *container, const std::string &text)
    {
        const WidgetID wID = GetNextItemId();
        Ref<WidgetButton> button = CreateRef<WidgetButton>(text, wID);

        if (container)
        {
            button->parent = container;
            container->children.push_back(button);
        }

        m_WidgetItems[wID] = button;
        return wID;
    }

    WidgetID WidgetCanvas::AddLabel(WidgetContainer *container, const std::string &text)
    {
        const WidgetID wID = GetNextItemId();
        Ref<WidgetLabel> label = CreateRef<WidgetLabel>(text, wID);

        if (container)
        {
            label->parent = container;
            container->children.push_back(label);
        }

        m_WidgetItems[wID] = label;
        return wID;
    }

    WidgetID WidgetCanvas::AddContainer(WidgetContainer *container)
    {
        const WidgetID wID = GetNextItemId();
        Ref<WidgetContainer> child = CreateRef<WidgetContainer>(wID);

        if (container)
        {
            child->parent = container;
            container->children.push_back(child);
        }

        m_WidgetItems[wID] = child;
        return wID;
    }

    WidgetContainer *WidgetCanvas::CreateRoot(uint32_t width, uint32_t height)
    {
        if (!m_Root)
        {
            m_ViewportSize = { width, height };
            const WidgetID wID = GetNextItemId();
            m_Root = CreateRef<WidgetContainer>(wID);
            m_Root->position = glm::vec2(0.0f);
            m_Root->size = glm::vec2(static_cast<float>(width), static_cast<float>(height));
            m_WidgetItems[wID] = m_Root;
        }

        return m_Root.get();
    }

    bool WidgetCanvas::RemoveItem(WidgetID id)
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
}
