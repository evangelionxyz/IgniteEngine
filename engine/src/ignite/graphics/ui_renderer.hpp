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
#include "ui/widget.hpp"
#include "ui/ui_manager.hpp"

#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

namespace ignite
{
    class UIWidget;
    class RenderTarget;
    class Renderer2D;

    class UIRenderer
    {
    public:
        UIRenderer(uint32_t width, uint32_t height);
        ~UIRenderer();

        void Update(float deltaTime);
        void Render(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);
        void Resize(uint32_t width, uint32_t height);

        // UI Manager integration
        void SetUIManager(UIManager* uiManager) { m_UIManager = uiManager; }
        UIManager* GetUIManager() const { return m_UIManager; }

        static Ref<UIRenderer> Create(uint32_t width, uint32_t height);
        
        Ref<Renderer2D> GetRenderer() { return m_Renderer; }
        const uint32_t &GetWidth() { return m_Width; }
        const uint32_t &GetHeight() { return m_Height; }

    private:
        void RenderLayoutGrid(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);
        void RenderWidgets(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);
        void RenderButton(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, Ref<UIButton> button);

        uint32_t m_Width;
        uint32_t m_Height;
        Ref<Renderer2D> m_Renderer;

        std::vector<Ref<UIWidget>> m_Widgets;
        UIManager* m_UIManager = nullptr;

        glm::mat4 m_Projection;
    };
}
