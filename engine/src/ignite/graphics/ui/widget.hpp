// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef WIDGET_HPP
#define WIDGET_HPP

#include "ignite/core/types.hpp"
#include "ignite/math/math.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/core/logger.hpp"

#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>

namespace ignite
{

#define UI_COLOR_RED glm::vec4{ 1.0f, 0.0f, 0.0f, 1.0f }
#define UI_COLOR_BLUE glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f }
#define UI_COLOR_WHITE glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f }
#define UI_COLOR_GREEN glm::vec4{ 0.0f, 1.0f, 0.0f, 1.0f }
#define UI_COLOR_YELLOW glm::vec4{ 1.0f, 1.0f, 0.0f, 1.0f }
#define UI_COLOR_GRAY glm::vec4{ 0.5f, 0.5f, 0.5f, 1.0f }

    class Widget;
    class Scene;
    class Texture;

    enum class WidgetAlignment
    {
        TOP_LEFT,
        TOP_CENTER,
        TOP_RIGHT,
        CENTER_LEFT,
        CENTER,
        CENTER_RIGHT,
        BOTTOM_LEFT,
        BOTTOM_CENTER,
        BOTTOM_RIGHT,
        COUNT
    };

    enum class WidgetItemType : uint8_t
    {
        Button = 0,
        Text = 1
    };

    // Base widget item
    class IWidgetItem : public std::enable_shared_from_this<IWidgetItem>
    {
    public:
        IWidgetItem(Widget *widget);
        virtual ~IWidgetItem() = default;

        virtual void Update(float deltaTime, const glm::uvec2& mousePos) {}
        virtual void SetPosition(const glm::vec2 &position) { m_Rect.min = position; }
        virtual void SetSize(const glm::vec2 &size) { m_Rect.max = m_Rect.min + size; }
        virtual void SetAlignment(WidgetAlignment alignment) { m_Alignment = alignment; }
        virtual void SetVisible(bool visible) { m_Visible = visible; }

        virtual const glm::vec2 GetSize() const { return m_Rect.GetSize(); }
        virtual const Rect &GetRect() const { return m_Rect; }
        virtual const WidgetAlignment GetAlignment() const { return m_Alignment; }
        virtual WidgetItemType GetItemType() const = 0;
        virtual bool IsVisible() const { return m_Visible; }

        virtual bool Contains(const glm::vec2& point) const { return GetAlignedRect().Contains(point); }
        virtual Rect GetAlignedRect() const;

        Widget *GetWidget() { return m_Widget; }

        template<typename T>
        Ref<T> As() { return std::dynamic_pointer_cast<T>(shared_from_this()); }

    protected:
        Widget *m_Widget = nullptr;
        Rect m_Rect = Rect({0.0f, 0.0f}, {100.0f, 50.0f});
        WidgetAlignment m_Alignment = WidgetAlignment::TOP_LEFT;
        bool m_Visible = true;
    };

    class WidgetButton : public IWidgetItem
    {
    public:
        WidgetButton(Widget *widget, const std::string &text = "Button");
        virtual ~WidgetButton() override;

        void Update(float deltaTime, const glm::uvec2 &mousePos) override;

        // Handle mouse click (call this from input system)
        void OnMouseClick(const glm::uvec2 &mousePos, bool isPressed);

        // Getters
        const std::string& GetText() const { return m_Text; }
        const Ref<Texture> &GetImage() const { return m_Image; }
        AssetHandle GetImageHandle() const { return m_ImageHandle; }

        virtual WidgetItemType GetItemType() const override { return WidgetItemType::Button; }

        bool IsHovered() const { return m_IsHovered; }
        bool IsPressed() const { return m_IsPressed; }

        const glm::vec4 &GetCurrentColor() const;

        const glm::vec4 &GetNormalColor() const { return m_NormalColor; }
        const glm::vec4 &GetHoverColor() const { return m_HoverColor; }
        const glm::vec4 &GetPressedColor() const { return m_PressedColor; }

        const glm::vec4& GetTextColor() const { return m_TextColor; }
        const glm::vec4& GetBorderColor() const { return m_BorderColor; }

        // Setters
        void SetText(const std::string& text) { m_Text = text; }
        void SetColors(const glm::vec4& normal, const glm::vec4& hover, const glm::vec4& pressed)
        {
            m_NormalColor = normal;
            m_HoverColor = hover;
            m_PressedColor = pressed;
        }
        void SetTextColor(const glm::vec4& color) { m_TextColor = color; }
        void SetBorderColor(const glm::vec4& color) { m_BorderColor = color; }
        void SetImage(const Ref<Texture> &image)
        {
            m_Image = image;
        }
        void SetImageHandle(AssetHandle imageHandle) { m_ImageHandle = imageHandle; }

        // Event callbacks
        void SetOnClick(std::function<void()> callback) { m_OnClick = callback; }
        void SetOnPressed(std::function<void()> callback) { m_OnPressed = callback; }
        void SetOnReleased(std::function<void()> callback) { m_OnReleased = callback; }
        void SetOnHoverEnter(std::function<void()> callback) { m_OnHoverEnter = callback; }
        void SetOnHoverExit(std::function<void()> callback) { m_OnHoverExit = callback; }

    private:
        std::string m_Text;
        bool m_IsHovered = false;
        bool m_IsPressed = false;

        Ref<Texture> m_Image;
        AssetHandle m_ImageHandle = AssetHandle(0);

        glm::vec4 m_NormalColor;
        glm::vec4 m_HoverColor;
        glm::vec4 m_PressedColor;
        glm::vec4 m_TextColor;
        glm::vec4 m_BorderColor;

        std::function<void()> m_OnClick;
        std::function<void()> m_OnPressed;
        std::function<void()> m_OnReleased;
        std::function<void()> m_OnHoverEnter;
        std::function<void()> m_OnHoverExit;
    };

    class WidgetText : public IWidgetItem
    {
    public:
        WidgetText(Widget *widget, const std::string& text = "Text")
            : IWidgetItem(widget), m_Text(text), m_Color(UI_COLOR_WHITE)
        {
        }

        virtual WidgetItemType GetItemType() const override { return WidgetItemType::Text; }

        const std::string& GetText() const { return m_Text; }
        const glm::vec4& GetColor() const { return m_Color; }
        AssetHandle GetFontHandle() const { return m_FontHandle; }
        float GetKerning() const { return m_Kerning; }
        float GetLineSpacing() const { return m_LineSpacing; }

        void SetText(const std::string& text) { m_Text = text; }
        void SetColor(const glm::vec4& color) { m_Color = color; }
        void SetFontHandle(AssetHandle fontHandle) { m_FontHandle = fontHandle; }
        void SetKerning(float kerning) { m_Kerning = kerning; }
        void SetLineSpacing(float lineSpacing) { m_LineSpacing = lineSpacing; }

    private:

        std::string m_Text;
        glm::vec4 m_Color;
        AssetHandle m_FontHandle = AssetHandle(0);
        float m_Kerning = 0.0f;
        float m_LineSpacing = -0.025f;
    };

    struct WidgetChildEntry
    {
        AssetHandle handle = AssetHandle(0);
        bool enabled = true;
        bool blockWidgetsBelow = true;
    };

    // Widget for widget items container
    class Widget : public Asset
    {
    public:
        Widget(Scene *scene = nullptr);
        ~Widget();

        void Update(float deltaTime, const glm::uvec2 &mousePos);

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<Widget> Deserialize(const std::filesystem::path &filepath);

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

        int AddButton(const std::string &text = "Button");
        int AddText(const std::string &text = "Text");
        bool RemoveItem(int id);

        static AssetType GetStaticAssetType() { return AssetType::Widget; }
        virtual AssetType GetAssetType() override { return GetStaticAssetType(); }

        // ID, Widget
        std::unordered_map<int, Ref<IWidgetItem>> &GetItems() { return m_WidgetItems; }
        const std::unordered_map<int, Ref<IWidgetItem>> &GetItems() const { return m_WidgetItems; }

        std::vector<WidgetChildEntry> &GetChildWidgets() { return m_ChildWidgets; }
        const std::vector<WidgetChildEntry> &GetChildWidgets() const { return m_ChildWidgets; }

    private:
        int GetNextItemId();

        std::string name;

        // ID, Widget
        std::unordered_map<int, Ref<IWidgetItem>> m_WidgetItems;
        std::vector<WidgetChildEntry> m_ChildWidgets;
        int m_NextWidgetItemId = 1;
        bool m_Enabled = true;
        bool m_BlocksWidgetsBelow = false;

        Scene *m_Scene;
        glm::uvec2 m_ViewportSize;
    };
}

#endif