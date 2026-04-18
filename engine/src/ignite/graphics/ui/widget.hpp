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
#include <vector>
#include <type_traits>
#include <glm/glm.hpp>

namespace ignite
{

#define UI_COLOR_RED { 1.0f, 0.0f, 0.0f, 1.0f }
#define UI_COLOR_BLUE { 0.0f, 0.0f, 1.0f, 1.0f }
#define UI_COLOR_WHITE { 1.0f, 1.0f, 1.0f, 1.0f }
#define UI_COLOR_GRAY { 0.5f, 0.5f, 0.5f, 1.0f }
#define UI_COLOR_DARK_GRAY { 0.25f, 0.25f, 0.25f, 1.0f }
#define UI_COLOR_GREEN { 0.0f, 1.0f, 0.0f, 1.0f }
#define UI_COLOR_YELLOW { 1.0f, 1.0f, 0.0f, 1.0f }
#define UI_COLOR_GRAY { 0.5f, 0.5f, 0.5f, 1.0f }

    class WidgetCanvas;
    class Scene;
    class Texture;
    class Renderer2D;
    class AssetManager;
    class WidgetContainer;

    enum class SizingMode
    {
        Default,
        ExpandToParent,

        COUNT
    };

    enum class LayoutMode
    {
        Horizontal,
        Vertical,
        Grid,
        Absolute,

        COUNT
    };

    enum class WidgetAlignment
    {
        TopLeft,
        TopCenter,
        TopRight,
        CenterLeft,
        Center,
        CenterRight,
        BottomLeft,
        BottomCenter,
        BottomRight,

        COUNT
    };

    enum class WidgetType : uint8_t
    {
        Container = 0,
        Button,
        Label
    };

    // Base widget item
    class IWidgetItem : public std::enable_shared_from_this<IWidgetItem>
    {
    public:
        IWidgetItem *parent = nullptr;
        std::vector<Ref<IWidgetItem>> children;

        std::string name; // widget name
        int id = -1;

        // Layout info
        glm::vec2 position = glm::vec2(0.0f); // x, y
        glm::vec2 size = glm::vec2(150.0f, 50.0f); // width, height
        float padding = 0.0f;
        float margin = 0.0f;
        Rect worldRect;
        bool visible = true;

        WidgetAlignment alignment = WidgetAlignment::TopLeft;
        SizingMode sizingMode = SizingMode::Default;

    public:
        virtual ~IWidgetItem() = default;

        template<typename T, typename... Args>
        T *AddChild(Args&&... args)
        {
            static_assert(std::is_base_of_v<IWidgetItem, T>, "T must derive from IWidgetItem");
            if constexpr (std::is_abstract_v<T>)
            {
                return nullptr;
            }

            Ref<T> child = CreateRef<T>(std::forward<Args>(args)...);
            child->parent = this;
            children.emplace_back(child);

            auto *ptr = child.get();
            return ptr;
        }

        virtual void Update(float deltaTime, const glm::vec2 &mousePosition) = 0;

        virtual void Draw(Renderer2D *renderer, AssetManager *assetManager)
        {
            for (const Ref<IWidgetItem> &child : children)
            {
                if (!child || !child->IsVisible())
                {
                    continue;
                }

                child->Draw(renderer, assetManager);
            }
        }

        virtual void Measure()
        {
            for (const Ref<IWidgetItem> &child : children)
            {
                if (!child || !child->IsVisible())
                {
                    continue;
                }

                child->Measure();
            }
        }

        virtual void Arrange(const Rect &parentArea)
        {
            worldRect = CalculateAlignedRect(parentArea);
        }

        virtual bool HitTest(int px, int py)
        {
            return worldRect.Contains(glm::vec2(static_cast<float>(px), static_cast<float>(py)));
        }

        void SetVisible(bool isVisible) { visible = isVisible; }
        bool IsVisible() const { return visible; }
        const Rect &GetAlignedRect() const { return worldRect; }

        Rect CalculateAlignedRect(const Rect &parentArea) const
        {
            const glm::vec2 marginVec = glm::vec2(margin);
            const glm::vec2 availableMin = parentArea.min + marginVec;
            const glm::vec2 availableMax = parentArea.max - marginVec;
            const glm::vec2 availableSize = glm::max(availableMax - availableMin, glm::vec2(0.0f));

            glm::vec2 resolvedSize = size;
            if (sizingMode == SizingMode::ExpandToParent)
            {
                resolvedSize = availableSize;
            }

            glm::vec2 alignedMin = availableMin;
            switch (alignment)
            {
                case WidgetAlignment::TopLeft:
                    break;
                case WidgetAlignment::TopCenter:
                    alignedMin.x += (availableSize.x - resolvedSize.x) * 0.5f;
                    break;
                case WidgetAlignment::TopRight:
                    alignedMin.x += (availableSize.x - resolvedSize.x);
                    break;
                case WidgetAlignment::CenterLeft:
                    alignedMin.y += (availableSize.y - resolvedSize.y) * 0.5f;
                    break;
                case WidgetAlignment::Center:
                    alignedMin += (availableSize - resolvedSize) * 0.5f;
                    break;
                case WidgetAlignment::CenterRight:
                    alignedMin.x += (availableSize.x - resolvedSize.x);
                    alignedMin.y += (availableSize.y - resolvedSize.y) * 0.5f;
                    break;
                case WidgetAlignment::BottomLeft:
                    alignedMin.y += (availableSize.y - resolvedSize.y);
                    break;
                case WidgetAlignment::BottomCenter:
                    alignedMin.x += (availableSize.x - resolvedSize.x) * 0.5f;
                    alignedMin.y += (availableSize.y - resolvedSize.y);
                    break;
                case WidgetAlignment::BottomRight:
                    alignedMin += (availableSize - resolvedSize);
                    break;
                default:
                    break;
            }

            alignedMin += position;
            return Rect(alignedMin, alignedMin + resolvedSize);
        }

        // Event callbacks
        void SetOnClick(std::function<void()> callback) { m_OnClick = callback; }
        void SetOnPressed(std::function<void()> callback) { m_OnPressed = callback; }
        void SetOnReleased(std::function<void()> callback) { m_OnReleased = callback; }
        void SetOnHoverEnter(std::function<void()> callback) { m_OnHoverEnter = callback; }
        void SetOnHoverExit(std::function<void()> callback) { m_OnHoverExit = callback; }

        template<typename T>
        Ref<T> As() { return std::dynamic_pointer_cast<T>(shared_from_this()); }

        virtual WidgetType GetWidgetType() const = 0;

    protected:
        std::function<void()> m_OnClick;
        std::function<void()> m_OnPressed;
        std::function<void()> m_OnReleased;
        std::function<void()> m_OnHoverEnter;
        std::function<void()> m_OnHoverExit;
    };

    class WidgetLabel : public IWidgetItem
    {
    public:
        AssetHandle fontHandle = AssetHandle(0);
        std::string text;
        glm::vec4 color;
        float fontSize = 16.0f; // In Pixel
        float kerning = 0.0f;
        float lineSpacing = -0.025f;

    public:
        WidgetLabel(const std::string &text);

        void SetText(const std::string &newText) { text = newText; }
        const std::string &GetText() const { return text; }
        void SetColor(const glm::vec4 &newColor) { color = newColor; }
        const glm::vec4 &GetColor() const { return color; }
        void SetFontHandle(AssetHandle handle) { fontHandle = handle; }
        AssetHandle GetFontHandle() const { return fontHandle; }
        void SetFontSize(float newFontSize) { fontSize = newFontSize; }
        float GetFontSize() const { return fontSize; }
        void SetKerning(float newKerning) { kerning = newKerning; }
        float GetKerning() const { return kerning; }
        void SetLineSpacing(float newLineSpacing) { lineSpacing = newLineSpacing; }
        float GetLineSpacing() const { return lineSpacing; }
        
        virtual void Update(float deltaTime, const glm::vec2 &mousePosition) override;
        virtual void Draw(Renderer2D *renderer, AssetManager *assetManager) override;
        virtual void Measure() override;
        virtual void Arrange(const Rect &) override;
        virtual bool HitTest(int px, int py) override;

        virtual WidgetType GetWidgetType() const override { return WidgetType::Label; }
    };

    class WidgetButton : public IWidgetItem
    {
    public:
        bool hovered = false;
        bool pressed = false;

        glm::vec4 normalColor;
        glm::vec4 hoverColor;
        glm::vec4 pressedColor;
        glm::vec4 borderColor;

        AssetHandle imageHandle = AssetHandle(0);
        Ref<Texture> image = nullptr;

        Scope<WidgetLabel> label;

    public:
        WidgetButton(const std::string &text);
        virtual ~WidgetButton() override;
        const glm::vec4 &GetCurrentColor() const;

        void SetText(const std::string &text);
        const std::string &GetText() const;
        void SetImageHandle(AssetHandle handle) { imageHandle = handle; }
        AssetHandle GetImageHandle() const { return imageHandle; }
        void SetImage(const Ref<Texture> &newImage) { image = newImage; }
        Ref<Texture> GetImage() const { return image; }

        void SetColors(const glm::vec4 &normal, const glm::vec4 &hover, const glm::vec4 &pressed);
        void SetTextColor(const glm::vec4 &textColor);
        const glm::vec4 &GetTextColor() const;
        void SetBorderColor(const glm::vec4 &newBorderColor) { borderColor = newBorderColor; }

        void SetFontHandle(AssetHandle handle);
        AssetHandle GetFontHandle() const;
        void SetFontSize(float newFontSize);
        float GetFontSize() const;
        void SetKerning(float newKerning);
        float GetKerning() const;
        void SetLineSpacing(float newLineSpacing);
        float GetLineSpacing() const;

        void OnMouseClick(const glm::uvec2 &mousePos, bool isPressed);

        virtual void Update(float deltaTime, const glm::vec2 &mousePosition) override;
        virtual void Draw(Renderer2D *renderer, AssetManager *assetManager) override;
        virtual void Measure() override;
        virtual void Arrange(const Rect &) override;
        virtual bool HitTest(int px, int py) override;

        virtual WidgetType GetWidgetType() const override { return WidgetType::Button; }

    private:
        std::function<void()> m_OnClick;
        std::function<void()> m_OnPressed;
        std::function<void()> m_OnReleased;
        std::function<void()> m_OnHoverEnter;
        std::function<void()> m_OnHoverExit;
    };

    struct WidgetChildEntry
    {
        AssetHandle handle = AssetHandle(0);
        bool enabled = true;
        bool blockWidgetsBelow = true;
    };

    // Widget for widget items container
    class WidgetCanvas : public Asset
    {
    public:
        WidgetCanvas(Scene *scene = nullptr);
        ~WidgetCanvas();

        void Update(float deltaTime, const glm::uvec2 &mousePos);

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

        int AddButton(WidgetContainer *container, const std::string &text);
        int AddLabel(WidgetContainer *container, const std::string &text);
        int AddContainer(WidgetContainer *container = nullptr);
        bool RemoveItem(int id);

        WidgetContainer *CreateRoot(uint32_t width, uint32_t height);
        WidgetContainer *GetRoot() const { return m_Root.get(); }

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
        Ref<WidgetContainer> m_Root = nullptr;
        int m_NextWidgetItemId = 1;
        bool m_Enabled = true;
        bool m_BlocksWidgetsBelow = false;

        Scene *m_Scene;
        glm::uvec2 m_ViewportSize;
    };
}

#endif