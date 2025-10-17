/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to do so, subject to the following conditions:
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

#include "layer.hpp"

#include "imgui.h"
#include <cinttypes>
#include <GLFW/glfw3.h>
#ifdef _WIN32

#ifndef GLFW_EXPOSE_NATIVE_WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
#endif GLFW_EXPOSE_NATIVE_WIN32

#include <windows.h>
#endif

namespace ignite
{
    HubLayer::HubLayer(const std::string &name)
        : Layer(name)
    {
    }

    HubLayer::~HubLayer()
    {
    }

    void HubLayer::OnAttach()
    {
        Layer::OnAttach();

        Application::GetInstance()->GetWindow()->Show(); // Show window after initialization
    }

    void HubLayer::OnDetach()
    {
        Layer::OnDetach();
    }

    void HubLayer::OnUpdate(f32 deltaTime)
    {
        Layer::OnUpdate(deltaTime);

        Renderer::OnUpdate();
    }

    void HubLayer::OnEvent(Event &e)
    {
        Layer::OnEvent(e);

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>(BIND_CLASS_EVENT_FN(HubLayer::OnKeyPressedEvent));
        dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_CLASS_EVENT_FN(HubLayer::OnMouseButtonPressed));
    }

    bool HubLayer::OnKeyPressedEvent(KeyPressedEvent &event)
    {
        return false;
    }

    bool HubLayer::OnMouseButtonPressed(MouseButtonPressedEvent &event)
    {
        return false;
    }

    void HubLayer::OnRender(nvrhi::IFramebuffer *mainFramebuffer)
    {
        Layer::OnRender(mainFramebuffer);
    }

    // Helper for centering content
    static void CenterCursor(float contentWidth)
    {
        float availX = ImGui::GetContentRegionAvail().x;
        float off = (availX - contentWidth) * 0.5f;
        if (off > 0.f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
    }

    // Platform-independent helper to get global cursor position in screen coordinates
    static ImVec2 GetGlobalCursorPos(GLFWwindow* window)
    {
#ifdef _WIN32
        POINT p; GetCursorPos(&p); return ImVec2((float)p.x, (float)p.y);
#else
        // On non-Windows platforms glfwGetCursorPos returns window-relative coords.
        // Add window position to get a pseudo-global coordinate.
        double cx, cy; glfwGetCursorPos(window, &cx, &cy);
        int wx, wy; glfwGetWindowPos(window, &wx, &wy);
        return ImVec2((float)(wx + cx), (float)(wy + cy));
#endif
    }

    void HubLayer::OnGuiRender()
    {
        ImGuiViewport *viewport = ImGui::GetMainViewport();

        // Background full-window host (invisible) -------------------------------------------------
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        ImGui::Begin("##IgniteHubHost", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        // Custom Title Bar ------------------------------------------------------------------------
        constexpr float titleBarHeight = 48.f;
        ImVec2 titleBarPos = ImGui::GetWindowPos();
        ImVec2 titleBarSize = ImVec2(ImGui::GetWindowWidth(), titleBarHeight);

        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImVec4 col = ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive];
        col.w = 1.f;
        dl->AddRectFilled(titleBarPos, ImVec2(titleBarPos.x + titleBarSize.x, titleBarPos.y + titleBarSize.y), ImColor(col));

        ImGui::SetCursorScreenPos(ImVec2(titleBarPos.x, titleBarPos.y));
        ImGui::InvisibleButton("##title_bar_drag", ImVec2(titleBarSize.x - 90.0f, titleBarSize.y));

        // Drag window when holding on title bar (avoid button areas automatically because they overlap later)
        static bool dragging = false;
        static ImVec2 dragStartMouseGlobal{}; // global screen coordinates at drag start
        static ImVec2 dragStartWindowPos{};   // window pos at drag start

        Window *window = Application::GetInstance()->GetWindow();
        GLFWwindow *native = window->GetWindowHandle();

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            dragging = true;
            dragStartMouseGlobal = GetGlobalCursorPos(native);
            glm::vec2 winPos = window->GetPosition();
            dragStartWindowPos = ImVec2(winPos.x, winPos.y);
        }

        if (dragging)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                ImVec2 curGlobal = GetGlobalCursorPos(native);
                ImVec2 delta(curGlobal.x - dragStartMouseGlobal.x, curGlobal.y - dragStartMouseGlobal.y);
                int newX = static_cast<int>(dragStartWindowPos.x + delta.x);
                int newY = static_cast<int>(dragStartWindowPos.y + delta.y);
                glfwSetWindowPos(native, newX, newY);
            }
            else
            {
                dragging = false;
            }
        }

        // Buttons (place them on top after drawing the drag region) --------------------------------
        ImGui::SetCursorScreenPos(ImVec2(titleBarPos.x + titleBarSize.x - 90.f, titleBarPos.y));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.f, 14.f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.1f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1,1,1,0.15f));

		const ImVec2 buttonSize = ImVec2(45.0f, titleBarSize.y);
        if (ImGui::Button("_", buttonSize))
        {
            window->Iconify();
        }
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f,0.0f,0.0f,0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f,0.1f,0.1f,0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f,0.2f,0.2f,1.0f));
        if (ImGui::Button("X", buttonSize))
        {
            window->Shutdown();
        }
        ImGui::PopStyleColor(3);

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);

        // Title centered --------------------------------------------------------------------------
        const char *title = "Ignite Hub";
        ImVec2 textSize = ImGui::CalcTextSize(title);
        float centerX = titleBarPos.x + titleBarSize.x * 0.5f - textSize.x * 0.5f;
        float centerY = titleBarPos.y + titleBarSize.y * 0.5f - textSize.y * 0.5f;
        dl->AddText(ImVec2(centerX, centerY), ImGui::GetColorU32(ImGuiCol_Text), title);

        // Main Hub Content ------------------------------------------------------------------------
        ImGui::SetCursorScreenPos(ImVec2(titleBarPos.x, titleBarPos.y + titleBarHeight));
        ImGui::BeginChild("##hub_content", ImVec2(titleBarSize.x, ImGui::GetWindowHeight() - titleBarHeight), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration);

        ImGui::Dummy(ImVec2(0, 40));
        {
            const char *heading = "Project Manager";
            ImVec2 hs = ImGui::CalcTextSize(heading);
            CenterCursor(hs.x);
            ImGui::TextUnformatted(heading);
        }

        ImGui::Dummy(ImVec2(0, 20));

        ImVec2 btnSize(260, 60);
        CenterCursor(btnSize.x);
        if (ImGui::Button("Create New Project", btnSize))
        {
            // TODO implement project creation
        }

        ImGui::Dummy(ImVec2(0, 16));
        CenterCursor(btnSize.x);
        if (ImGui::Button("Open Existing Project", btnSize))
        {
            // TODO implement project loading
        }

        ImGui::Dummy(ImVec2(0, 32));
        const char* desc = "Welcome to Ignite Hub. Create a new project or open an existing one to get started.";
        float width = ImGui::CalcTextSize(desc).x;
        CenterCursor(width);
        ImGui::TextWrapped(desc);

        ImGui::EndChild();

        ImGui::End(); // host window
    }
}
