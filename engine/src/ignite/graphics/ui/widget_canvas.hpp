// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef WIDGET_CANVAS_HPP
#define WIDGET_CANVAS_HPP

#include "widget.hpp"

namespace ignite
{
    struct WidgetChildEntry
    {
        AssetHandle handle = AssetHandle(0);
        bool enabled = true;
        bool blockWidgetsBelow = true;
    };

    class WidgetCanvas : public Asset
    {
    public:
        WidgetCanvas(Scene *scene = nullptr);
        ~WidgetCanvas();

        WidgetID GetNextItemId();

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<WidgetCanvas> Deserialize(const std::filesystem::path &filepath);

        void SetViewportSize(const uint32_t width, const uint32_t height) { m_ViewportSize = { width, height }; }
        const glm::uvec2 &GetViewportSize() const { return m_ViewportSize; }
        void SetScene(Scene *scene) { m_Scene = scene; }
        Scene *GetScene() const { return m_Scene; }

        void SetName(const std::string &newName) { name = newName; }
        const std::string &GetName() const { return name; }

        void SetEnabled(bool enabled) { m_Enabled = enabled; }
        bool IsEnabled() const { return m_Enabled; }

        void SetBlocksWidgetsBelow(bool blockWidgetsBelow) { m_BlocksWidgetsBelow = blockWidgetsBelow; }
        bool BlocksWidgetsBelow() const { return m_BlocksWidgetsBelow; }

        WidgetID AddButton(WidgetContainer *container, const std::string &text);
        WidgetID AddLabel(WidgetContainer *container, const std::string &text);
        WidgetID AddContainer(WidgetContainer *container = nullptr);
        WidgetID AddImage(WidgetContainer *container = nullptr);
        WidgetID AddBoxSizing(WidgetContainer *container = nullptr);
        WidgetID AddOverlay(WidgetContainer *container = nullptr);
        bool ReparentItem(WidgetID id, WidgetContainer *newParent);
        bool RemoveItem(WidgetID id);

        WidgetContainer *CreateRoot(uint32_t width, uint32_t height);
        WidgetContainer *GetRoot() const { return m_Root.get(); }

        static AssetType GetStaticAssetType() { return AssetType::Widget; }
        virtual AssetType GetAssetType() override { return GetStaticAssetType(); }

        std::unordered_map<WidgetID, Ref<IWidgetItem>> &GetItems() { return m_WidgetItems; }
        const std::unordered_map<WidgetID, Ref<IWidgetItem>> &GetItems() const { return m_WidgetItems; }

        std::vector<WidgetChildEntry> &GetChildWidgets() { return m_ChildWidgets; }
        const std::vector<WidgetChildEntry> &GetChildWidgets() const { return m_ChildWidgets; }

    private:
        std::string name;

        // ID, Widget
        std::unordered_map<WidgetID, Ref<IWidgetItem>> m_WidgetItems;
        std::vector<WidgetChildEntry> m_ChildWidgets;

        Ref<WidgetContainer> m_Root = nullptr;
        bool m_Enabled = true;
        bool m_BlocksWidgetsBelow = false;
        WidgetID m_NextWidgetItemId = 0;

        Scene *m_Scene;
        glm::uvec2 m_ViewportSize;
    };
}

#endif