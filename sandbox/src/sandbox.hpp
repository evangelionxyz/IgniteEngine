/* MIT License
*
* Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO
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
#include "ignite/core/layer.hpp"

#include "ignite/core/input/app_event.hpp"
#include "ignite/core/input/key_event.hpp"
#include "ignite/core/input/mouse_event.hpp"
#include "ignite/core/input/joystick_event.hpp"

#include <nvrhi/nvrhi.h>
#include <filesystem>

#include "camera.hpp"

namespace ignite
{
	class Scene;
	class Project;
	class RenderTarget;
	class GraphicsPipeline;
	class SceneRenderer;

	class SandboxLayer : public Layer
	{
	public:
		SandboxLayer(const std::string &name);
		~SandboxLayer();

	private:
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(float deltaTime) override;
		virtual void OnRender(nvrhi::IFramebuffer *framebuffer) override;
		virtual void OnGuiRender() override;

	private:// Events
		virtual void OnEvent(Event &event) override;
		bool OnKeyPressedEvent(KeyPressedEvent &event);
		bool OnMouseButtonPressedEvent(MouseButtonPressedEvent &event);
		bool OnMouseMovedEvent(MouseMovedEvent &event);
		bool OnResizeEvent(FramebufferResizeEvent &event);

	private: // Helper functions
		void CreateRenderTargets();
		void UpdateCameraInput(float deltaTime);

	private: // Data
		Ref<Project> m_Project;
		Ref<Scene> m_Scene;
		Ref<SceneRenderer> m_SceneRenderer;
		Ref<RenderTarget> m_SceneRT;
		Ref<RenderTarget> m_UIRT;
		Ref<RenderTarget> m_CompositeRT;
		Ref<GraphicsPipeline> m_CompositePipeline;
		nvrhi::DeviceHandle m_Device;
		nvrhi::CommandListHandle m_Cmd;

		BasicCamera m_Camera;

		struct
		{
			glm::vec2 position;
			glm::vec2 size;
			glm::vec2 mousePosition;
		} m_ViewportData;
	};
}
