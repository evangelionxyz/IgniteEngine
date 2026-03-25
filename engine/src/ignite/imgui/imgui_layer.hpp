/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include "ignite/core/types.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "imgui_nvrhi.hpp"
#include "ignite/core/input/event.hpp"
#include "ignite/core/layer.hpp"
#include "ignite/core/input/app_event.hpp"

#include "ignite/core/buffer.hpp"

#include <SDL3/SDL.h>

#include <filesystem>
#include <optional>

namespace ignite
{
    class ShaderFactory;

    class GuiFont
    {
    public:
        GuiFont();
        GuiFont(f32 size);
        GuiFont(Buffer data, bool isCompressed, f32 size);

        bool HasFontData() const { return m_Data.data != nullptr; }
        ImFont *GetScaledFont() const { return m_ImFont; }

    protected:
        friend class ImGuiLayer;

        Buffer m_Data;
        bool const m_IsDefault;
        bool const m_IsCompressed;
        f32 const m_SizeAtDefaultScale;
        ImFont *m_ImFont = nullptr;

        void CreateScaledFont(f32 displayScale);
        void ReleaseScaledFont();
    };

    class ImGuiLayer : public Layer
    {
    public:
        virtual ~ImGuiLayer() = default;

        ImGuiLayer(DeviceManager *deviceManager);
        void OnAttach() override;
        void OnDetach() override;

        void BeginFrame();
        void EndFrame(nvrhi::IFramebuffer* framebuffer);

        void PollEvent(const SDL_Event &event);

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
        f32 m_CurrentDPIScale = 1.0f;
    };
}
