// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "widget_canvas.hpp"
#include "widget_container.hpp"
#include "widget_label.hpp"
#include "widget_button.hpp"
#include "widget_image.hpp"

#include "ignite/core/application.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/asset/asset_manager.hpp"

namespace ignite
{
    WidgetCanvas::WidgetCanvas(Scene *scene)
        : m_ViewportSize({ 1280, 720 })
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
        sr.AddKeyValue("Version", Application::GetVersion());
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
            sr.AddKeyValue("ZIndex", item->zIndex);

            // Layout properties
            sr.AddKeyValue("Position", item->layout.position);
            sr.AddKeyValue("WidthMode", static_cast<int>(item->layout.widthMode));
            sr.AddKeyValue("HeightMode", static_cast<int>(item->layout.heightMode));
            sr.AddKeyValue("Width", item->layout.width);
            sr.AddKeyValue("Height", item->layout.height);
            sr.AddKeyValue("MinWidth", item->layout.minWidth);
            sr.AddKeyValue("MinHeight", item->layout.minHeight);
            sr.AddKeyValue("MaxWidth", item->layout.maxWidth);
            sr.AddKeyValue("MaxHeight", item->layout.maxHeight);
            sr.AddKeyValue("Margin", item->layout.margin);
            sr.AddKeyValue("Padding", item->layout.padding);
            sr.AddKeyValue("VerticalAlignment", static_cast<int>(item->layout.verticalAlignment));
            sr.AddKeyValue("HorizontalAlignment", static_cast<int>(item->layout.horizontalAlignment));
            sr.AddKeyValue("PositionType", static_cast<int>(item->layout.positionType));
            sr.AddKeyValue("Overflow", static_cast<int>(item->layout.overflow));
            sr.AddKeyValue("Visibility", static_cast<int>(item->layout.visibility));

            // Flex properties
            sr.AddKeyValue("FlexDirection", static_cast<int>(item->layout.flex.direction));
            sr.AddKeyValue("JustifyContent", static_cast<int>(item->layout.flex.justifyContent));
            sr.AddKeyValue("AlignItems", static_cast<int>(item->layout.flex.alignItems));
            sr.AddKeyValue("AlignSelf", static_cast<int>(item->layout.flex.alignSelf));
            sr.AddKeyValue("FlexWrap", static_cast<int>(item->layout.flex.wrap));
            sr.AddKeyValue("FlexGrow", item->layout.flex.grow);
            sr.AddKeyValue("FlexShrink", item->layout.flex.shrink);
            sr.AddKeyValue("FlexBasis", item->layout.flex.basis);
            sr.AddKeyValue("FlexGap", item->layout.flex.gap);

            // UIStyle properties
            sr.AddKeyValue("BackgroundColor", item->baseStyle.backgroundColor);
            sr.AddKeyValue("BorderColor", item->baseStyle.borderColor);
            sr.AddKeyValue("BorderWidth", item->baseStyle.borderWidth);
            sr.AddKeyValue("CornerRadius", item->baseStyle.cornerRadius);
            sr.AddKeyValue("Opacity", item->baseStyle.opacity);

            if (item->GetWidgetType() == WidgetType::Button)
            {
                if (const Ref<WidgetButton> button = item->As<WidgetButton>())
                {
                    sr.AddKeyValue("ImageHandle", static_cast<uint64_t>(button->imageHandle));
                    sr.AddKeyValue("NormalColor", button->style.color);
                    sr.AddKeyValue("HoverColor", button->style.hoverColor);
                    sr.AddKeyValue("PressedColor", button->style.pressedColor);
                    sr.AddKeyValue("BorderColor", button->style.borderColor);
                    sr.AddKeyValue("BorderWidth", button->style.borderWidth);
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

            if (item->GetWidgetType() == WidgetType::Image)
            {
                if (const Ref<WidgetImage> img = item->As<WidgetImage>())
                {
                    sr.AddKeyValue("ImageHandle", static_cast<uint64_t>(img->imageHandle));
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

                Ref<IWidgetItem> item = nullptr;
                switch (type)
                {
                    case WidgetType::Container: item = CreateRef<WidgetContainer>(id); break;
                    case WidgetType::Button:    item = CreateRef<WidgetButton>("", id); break;
                    case WidgetType::Label:     item = CreateRef<WidgetLabel>("", id); break;
                    case WidgetType::Image:     item = CreateRef<WidgetImage>(id); break;
                    case WidgetType::BoxSizing: item = CreateRef<WidgetBoxSizing>(id); break;
                    case WidgetType::Overlay:   item = CreateRef<WidgetOverlay>(id); break;
                    default: break;
                }

                if (!item)
                    continue;

                if (auto n = itemNode["Name"]) item->name = n.as<std::string>();
                if (auto n = itemNode["ID"]) item->id = n.as<int>();
                if (auto n = itemNode["ZIndex"]) item->zIndex = n.as<int>();

                // Position & Size fallback
                if (auto n = itemNode["Position"]) item->layout.position = n.as<glm::vec2>();
                if (auto n = itemNode["WidthMode"]) item->layout.widthMode = static_cast<SizeMode>(n.as<int>());
                if (auto n = itemNode["HeightMode"]) item->layout.heightMode = static_cast<SizeMode>(n.as<int>());

                if (auto n = itemNode["Width"]) item->layout.width = n.as<float>();
                else if (auto n = itemNode["Size"]) item->layout.width = n.as<glm::vec2>().x;

                if (auto n = itemNode["Height"]) item->layout.height = n.as<float>();
                else if (auto n = itemNode["Size"]) item->layout.height = n.as<glm::vec2>().y;

                if (auto n = itemNode["MinWidth"]) item->layout.minWidth = n.as<float>();
                if (auto n = itemNode["MinHeight"]) item->layout.minHeight = n.as<float>();
                if (auto n = itemNode["MaxWidth"]) item->layout.maxWidth = n.as<float>();
                if (auto n = itemNode["MaxHeight"]) item->layout.maxHeight = n.as<float>();

                // Margin & Padding fallback (vec4 or scalar)
                if (auto n = itemNode["Margin"])
                {
                    try { item->layout.margin = n.as<glm::vec4>(); }
                    catch (...) { item->layout.margin = glm::vec4(n.as<float>()); }
                }

                if (auto n = itemNode["Padding"])
                {
                    try { item->layout.padding = n.as<glm::vec4>(); }
                    catch (...) { item->layout.padding = glm::vec4(n.as<float>()); }
                }

                if (auto n = itemNode["VerticalAlignment"]) item->layout.verticalAlignment = static_cast<VerticalAlignment>(n.as<int>());
                if (auto n = itemNode["HorizontalAlignment"]) item->layout.horizontalAlignment = static_cast<HorizontalAlignment>(n.as<int>());
                if (auto n = itemNode["PositionType"]) item->layout.positionType = static_cast<PositionType>(n.as<int>());
                if (auto n = itemNode["Overflow"]) item->layout.overflow = static_cast<OverflowMode>(n.as<int>());

                if (auto n = itemNode["Visibility"])
                {
                    item->layout.visibility = static_cast<VisibilityMode>(n.as<int>());
                }
                else if (auto n = itemNode["Visible"])
                {
                    item->layout.visibility = n.as<bool>() ? VisibilityMode::Visible : VisibilityMode::Hidden;
                }

                // Flex fallback
                if (auto n = itemNode["FlexDirection"])
                {
                    item->layout.flex.direction = static_cast<FlexDirection>(n.as<int>());
                }
                else if (auto n = itemNode["Layout"])
                {
                    const int oldLayout = n.as<int>();
                    item->layout.flex.direction = (oldLayout == 0) ? FlexDirection::Row : FlexDirection::Column;
                }

                if (auto n = itemNode["JustifyContent"]) item->layout.flex.justifyContent = static_cast<JustifyContent>(n.as<int>());
                if (auto n = itemNode["AlignItems"]) item->layout.flex.alignItems = static_cast<AlignItems>(n.as<int>());
                if (auto n = itemNode["AlignSelf"]) item->layout.flex.alignSelf = static_cast<AlignSelf>(n.as<int>());
                if (auto n = itemNode["FlexWrap"]) item->layout.flex.wrap = static_cast<FlexWrap>(n.as<int>());
                if (auto n = itemNode["FlexGrow"]) item->layout.flex.grow = n.as<float>();
                if (auto n = itemNode["FlexShrink"]) item->layout.flex.shrink = n.as<float>();
                if (auto n = itemNode["FlexBasis"]) item->layout.flex.basis = n.as<float>();

                if (auto n = itemNode["FlexGap"]) item->layout.flex.gap = n.as<float>();
                else if (auto n = itemNode["Gap"]) item->layout.flex.gap = n.as<float>();

                // UIStyle
                if (auto n = itemNode["BackgroundColor"]) item->baseStyle.backgroundColor = n.as<glm::vec4>();
                if (auto n = itemNode["BorderColor"]) item->baseStyle.borderColor = n.as<glm::vec4>();
                if (auto n = itemNode["BorderWidth"]) item->baseStyle.borderWidth = n.as<float>();
                if (auto n = itemNode["CornerRadius"]) item->baseStyle.cornerRadius = n.as<float>();
                if (auto n = itemNode["Opacity"]) item->baseStyle.opacity = n.as<float>();

                if (type == WidgetType::Container)
                {
                    if (Ref<WidgetContainer> container = item->As<WidgetContainer>())
                    {
                        if (itemNode["Layout"])
                        {
                            const int oldLayout = itemNode["Layout"].as<int>();
                            container->layout.flex.direction = (oldLayout == 0) ? FlexDirection::Row : FlexDirection::Column;
                        }
                        if (itemNode["Padding"])
                        {
                            try { container->layout.padding = itemNode["Padding"].as<glm::vec4>(); }
                            catch (...) { container->layout.padding = glm::vec4(itemNode["Padding"].as<float>()); }
                        }
                        if (itemNode["Gap"]) container->layout.flex.gap = itemNode["Gap"].as<float>();
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
                        if (auto n = itemNode["BorderWidth"]) button->style.borderWidth = n.as<float>();
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
                        if (itemNode["FontSize"]) text->style.fontSize = itemNode["FontSize"].as<float>();
                        if (itemNode["Kerning"]) text->style.kerning = itemNode["Kerning"].as<float>();
                        if (itemNode["LineSpacing"]) text->style.lineSpacing = itemNode["LineSpacing"].as<float>();
                    }
                }

                else if (type == WidgetType::Image)
                {
                    if (Ref<WidgetImage> img = item->As<WidgetImage>())
                    {
                        if (auto n = itemNode["ImageHandle"]) img->imageHandle = AssetHandle(n.as<uint64_t>());
                    }
                }

                widget->m_WidgetItems[id] = item;
                parentMap[id] = parentID;
                widget->m_NextWidgetItemId = std::max(widget->m_NextWidgetItemId, id + 1);
            }
        }

        for (auto &[id, item] : widget->m_WidgetItems)
        {
            if (item)
            {
                item->children.clear();
            }
        }

        for (auto &[id, item] : widget->m_WidgetItems)
        {
            if (!item)
            {
                continue;
            }

            const auto parentIt = parentMap.find(id);
            const int parentID = (parentIt != parentMap.end()) ? parentIt->second : -1;

            if (id == parentID || parentID < 0)
            {
                if (item->GetWidgetType() == WidgetType::Container && !widget->m_Root)
                {
                    widget->m_Root = item->As<WidgetContainer>();
                    const glm::vec2 rootSize = widget->m_Root->GetSize();
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

            const auto ownerIt = widget->m_WidgetItems.find(parentID);
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
        button->MarkLayoutDirty();
        button->MarkPaintDirty();
        SetDirtyFlag(true);
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
        label->MarkLayoutDirty();
        label->MarkPaintDirty();
        SetDirtyFlag(true);
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
        child->MarkLayoutDirty();
        child->MarkPaintDirty();
        SetDirtyFlag(true);
        return wID;
    }

    WidgetID WidgetCanvas::AddImage(WidgetContainer *container)
    {
        const WidgetID wID = GetNextItemId();
        Ref<WidgetImage> image = CreateRef<WidgetImage>(wID);

        if (container)
        {
            image->parent = container;
            container->children.push_back(image);
        }

        m_WidgetItems[wID] = image;
        image->MarkLayoutDirty();
        image->MarkPaintDirty();
        SetDirtyFlag(true);
        return wID;
    }

    WidgetID WidgetCanvas::AddBoxSizing(WidgetContainer *container)
    {
        const WidgetID wID = GetNextItemId();
        Ref<WidgetBoxSizing> box = CreateRef<WidgetBoxSizing>(wID);

        if (container)
        {
            box->parent = container;
            container->children.push_back(box);
        }

        m_WidgetItems[wID] = box;
        box->MarkLayoutDirty();
        box->MarkPaintDirty();
        SetDirtyFlag(true);
        return wID;
    }

    WidgetID WidgetCanvas::AddOverlay(WidgetContainer *container)
    {
        const WidgetID wID = GetNextItemId();
        Ref<WidgetOverlay> overlay = CreateRef<WidgetOverlay>(wID);

        if (container)
        {
            overlay->parent = container;
            container->children.push_back(overlay);
        }

        m_WidgetItems[wID] = overlay;
        overlay->MarkLayoutDirty();
        overlay->MarkPaintDirty();
        SetDirtyFlag(true);
        return wID;
    }

    bool WidgetCanvas::ReparentItem(WidgetID id, WidgetContainer *newParent)
    {
        if (!newParent)
            return false;

        const auto it = m_WidgetItems.find(id);
        if (it == m_WidgetItems.end() || !it->second)
            return false;

        Ref<IWidgetItem> item = it->second;

        // Prevent reparenting root or to itself
        if (m_Root && item.get() == m_Root.get())
            return false;
        if (item.get() == newParent)
            return false;

        // Prevent reparenting to a descendant (cycle check)
        IWidgetItem *check = newParent;
        while (check)
        {
            if (check == item.get())
                return false;
            check = check->parent;
        }

        // Remove from old parent
        if (item->parent)
        {
            auto &siblings = item->parent->children;
            siblings.erase(std::remove_if(siblings.begin(), siblings.end(),
                [&](const Ref<IWidgetItem> &c) { return c.get() == item.get(); }),
                siblings.end());
        }

        // Attach to new parent
        item->parent = newParent;
        newParent->children.push_back(item);
        if (m_Root)
        {
            m_Root->MarkLayoutDirty();
            m_Root->MarkPaintDirty();
        }
        SetDirtyFlag(true);
        return true;
    }

    bool WidgetCanvas::ReorderItem(WidgetID draggedId, WidgetID targetId, bool insertAfter)
    {
        if (draggedId == targetId)
            return false;

        const auto draggedIt = m_WidgetItems.find(draggedId);
        const auto targetIt = m_WidgetItems.find(targetId);
        if (draggedIt == m_WidgetItems.end() || targetIt == m_WidgetItems.end())
            return false;

        Ref<IWidgetItem> dragged = draggedIt->second;
        Ref<IWidgetItem> target = targetIt->second;
        if (!dragged || !target || !target->parent)
            return false;

        // Cannot reorder root
        if (m_Root && dragged.get() == m_Root.get())
            return false;

        // Prevent moving an item inside its own descendant
        IWidgetItem *check = target->parent;
        while (check)
        {
            if (check == dragged.get())
                return false;
            check = check->parent;
        }

        // Remove dragged from current parent
        if (dragged->parent)
        {
            auto &oldSiblings = dragged->parent->children;
            oldSiblings.erase(std::remove_if(oldSiblings.begin(), oldSiblings.end(),
                [&](const Ref<IWidgetItem> &c) { return c.get() == dragged.get(); }),
                oldSiblings.end());
        }

        // Assign new parent
        IWidgetItem *newParent = target->parent;
        dragged->parent = newParent;

        // Insert into newParent->children relative to target
        auto &newSiblings = newParent->children;
        auto insertIt = std::find_if(newSiblings.begin(), newSiblings.end(),
            [&](const Ref<IWidgetItem> &c) { return c.get() == target.get(); });

        if (insertIt != newSiblings.end())
        {
            if (insertAfter)
                ++insertIt;
            newSiblings.insert(insertIt, dragged);
        }
        else
        {
            newSiblings.push_back(dragged);
        }

        if (m_Root)
        {
            m_Root->MarkLayoutDirty();
            m_Root->MarkPaintDirty();
        }
        SetDirtyFlag(true);
        return true;
    }

    bool WidgetCanvas::MoveItemUp(WidgetID id)
    {
        const auto it = m_WidgetItems.find(id);
        if (it == m_WidgetItems.end() || !it->second || !it->second->parent)
            return false;

        Ref<IWidgetItem> item = it->second;
        auto &siblings = item->parent->children;

        for (size_t i = 1; i < siblings.size(); ++i)
        {
            if (siblings[i].get() == item.get())
            {
                std::swap(siblings[i], siblings[i - 1]);
                if (m_Root)
                {
                    m_Root->MarkLayoutDirty();
                    m_Root->MarkPaintDirty();
                }
                SetDirtyFlag(true);
                return true;
            }
        }

        return false;
    }

    bool WidgetCanvas::MoveItemDown(WidgetID id)
    {
        const auto it = m_WidgetItems.find(id);
        if (it == m_WidgetItems.end() || !it->second || !it->second->parent)
            return false;

        Ref<IWidgetItem> item = it->second;
        auto &siblings = item->parent->children;

        for (size_t i = 0; i + 1 < siblings.size(); ++i)
        {
            if (siblings[i].get() == item.get())
            {
                std::swap(siblings[i], siblings[i + 1]);
                if (m_Root)
                {
                    m_Root->MarkLayoutDirty();
                    m_Root->MarkPaintDirty();
                }
                SetDirtyFlag(true);
                return true;
            }
        }

        return false;
    }

    WidgetContainer *WidgetCanvas::CreateRoot(uint32_t width, uint32_t height)
    {
        if (!m_Root)
        {
            m_ViewportSize = { width, height };
            const WidgetID wID = GetNextItemId();
            m_Root = CreateRef<WidgetContainer>(wID);
            m_Root->SetPosition(glm::vec2(0.0f));
            m_Root->SetSize(glm::vec2(static_cast<float>(width), static_cast<float>(height)));
            m_Root->layout.flex.direction = FlexDirection::Column;
            m_Root->layout.flex.justifyContent = JustifyContent::SpaceBetween;
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
        if (!item)
        {
            return false;
        }

        IWidgetItem *parentPtr = item->parent;

        // Recursive lambda to collect and remove item and all descendant children from m_WidgetItems
        std::function<void(const Ref<IWidgetItem> &)> removeSubtree = [&](const Ref<IWidgetItem> &node)
        {
            if (!node)
                return;

            for (const Ref<IWidgetItem> &child : node->children)
            {
                removeSubtree(child);
            }

            m_WidgetItems.erase(node->id);
        };

        if (parentPtr)
        {
            auto &siblings = parentPtr->children;
            siblings.erase(std::remove_if(siblings.begin(), siblings.end(), [&](const Ref<IWidgetItem> &child)
            {
                return child.get() == item.get();
            }), siblings.end());

            parentPtr->MarkLayoutDirty();
            parentPtr->MarkPaintDirty();
        }

        if (m_Root && m_Root.get() == item.get())
        {
            m_Root = nullptr;
        }

        removeSubtree(item);

        if (m_Root)
        {
            m_Root->MarkLayoutDirty();
            m_Root->MarkPaintDirty();
        }

        SetDirtyFlag(true);
        return true;
    }
}
