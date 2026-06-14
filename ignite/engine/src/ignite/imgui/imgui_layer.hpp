// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_IMGUI_LAYER_HPP
#define IGN_IMGUI_LAYER_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "imgui_nvrhi.hpp"
#include "ignite/core/input/event.hpp"
#include "ignite/core/layer.hpp"
#include "ignite/core/input/app_event.hpp"

#include "ignite/core/buffer.hpp"

#include <SDL3/SDL.h>

#include "ignite/core/path.hpp"
#include <optional>

namespace ignite
{
    class ShaderFactory;

    class GuiFont
    {
    public:
        GuiFont();
        GuiFont(float size);
        GuiFont(Buffer buffer, bool isCompressed, float size);

        [[nodiscard]]
        bool HasFontData() const { return !m_Buffer.IsEmpty(); }
        ImFont *GetScaledFont() const { return m_ImFont; }

    protected:
        friend class ImGuiLayer;
        Buffer m_Buffer;

        bool const m_IsDefault;
        bool const m_IsCompressed;
        float const m_SizeAtDefaultScale;
        ImFont *m_ImFont = nullptr;

        void CreateScaledFont(float displayScale);
        void ReleaseScaledFont();
    };

    class IGN_API ImGuiLayer : public Layer
    {
    public:
        virtual ~ImGuiLayer() = default;

        ImGuiLayer(DeviceManager *deviceManager);
        void OnAttach() override;
        void OnDetach() override;

        void BeginFrame();
        void EndFrame(nvrhi::IFramebuffer* framebuffer);
        void RenderPlatformWindows();

        void PollEvent(const SDL_Event &event);

        void SetBlock(bool block) { m_BlockEvents = block; }

        void OnEvent(Event &event) override;
        bool OnFramebufferResize(FramebufferResizeEvent &event) const;
        bool OnDPIScaleChanged(WindowDPIScaleChangedEvent &event);

    private:
        Scope<ImGui_NVRHI> imguiNVRHI;
        Ref<GuiFont> m_Font;

        bool m_SupportExplicitDisplayScaling;
        bool m_BeginFrameCalled = false;
        bool m_BlockEvents = true;

        DeviceManager *m_DeviceManager = nullptr;
        
        // Store original style for proper scaling
        ImGuiStyle m_OriginalStyle;
        float m_CurrentDPIScale = 1.0f;
    };
}

#endif
