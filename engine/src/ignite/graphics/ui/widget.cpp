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

#include "widget.hpp"
#include "ui_manager.hpp"

namespace ignite {

    Rect UIWidget::GetAlignedRect() const
    {
        Rect alignedRect = m_Rect;
        glm::vec2 &position = alignedRect.min;
        glm::vec2 &size = alignedRect.max;
        
        UIManager &uiManager = UIManager::GetInstance();
        const glm::vec2 viewportSize = { static_cast<float>(uiManager.GetViewportWidth()), static_cast<float>(uiManager.GetViewportHeight()) };
        
        switch (m_Alignment)
        {
            case UIAlignment::TOP_CENTER:
                position.x = viewportSize.x / 2.0f + m_Rect.min.x - m_Rect.GetSize().x / 2.0f;
                size.x = viewportSize.x / 2.0f + m_Rect.min.x + m_Rect.GetSize().x / 2.0f;
            break;
            case UIAlignment::TOP_RIGHT:
                position.x = viewportSize.x - m_Rect.min.x - m_Rect.GetSize().x;
                size.x = viewportSize.x - m_Rect.min.x;
                break;
            case UIAlignment::CENTER_LEFT:
                position.y = viewportSize.y / 2.0f + m_Rect.min.y - m_Rect.GetSize().y / 2.0f;
                size.y = viewportSize.y / 2.0f + m_Rect.min.y + m_Rect.GetSize().y / 2.0f;
                break;
            case UIAlignment::CENTER:
                position.x = viewportSize.x / 2.0f + m_Rect.min.x - m_Rect.GetSize().x / 2.0f;
                size.x = viewportSize.x / 2.0f + m_Rect.min.x + m_Rect.GetSize().x / 2.0f;
                position.y = viewportSize.y / 2.0f + m_Rect.min.y - m_Rect.GetSize().y / 2.0f;
                size.y = viewportSize.y / 2.0f + m_Rect.min.y + m_Rect.GetSize().y / 2.0f;
                break;
            case UIAlignment::CENTER_RIGHT:
                position.x = viewportSize.x - m_Rect.min.x - m_Rect.GetSize().x;
                size.x = viewportSize.x - m_Rect.min.x;
                position.y = viewportSize.y / 2.0f + m_Rect.min.y - m_Rect.GetSize().y / 2.0f;
                size.y = viewportSize.y / 2.0f + m_Rect.min.y + m_Rect.GetSize().y / 2.0f;
                break;
            case UIAlignment::BOTTOM_LEFT:
                position.y = viewportSize.y - m_Rect.max.y;
                size.y = viewportSize.y - m_Rect.min.y;
                break;
            case UIAlignment::BOTTOM_CENTER:
                position.x = viewportSize.x / 2.0f + m_Rect.min.x - m_Rect.GetSize().x / 2.0f;
                size.x = viewportSize.x / 2.0f + m_Rect.min.x + m_Rect.GetSize().x / 2.0f;
                position.y = viewportSize.y - m_Rect.max.y;
                size.y = viewportSize.y - m_Rect.min.y;
                break;
            case UIAlignment::BOTTOM_RIGHT:
                position.x = viewportSize.x - m_Rect.min.x - m_Rect.GetSize().x;
                size.x = viewportSize.x - m_Rect.min.x; 
                position.y = viewportSize.y - m_Rect.max.y;
                size.y = viewportSize.y - m_Rect.min.y;
                break;
            case UIAlignment::TOP_LEFT:
            default: break;
        }

        return alignedRect;
    }
}
