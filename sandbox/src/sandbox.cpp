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

#include <ignite/entry_point.hpp>
#include <ignite/core/application.hpp>

#include "ignite/project/project.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/graphics/scene_renderer.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/renderer_2d.hpp"

#include "sandbox.hpp"

class SandboxApp final : public ignite::Application
{
public:
	explicit SandboxApp(const ignite::ApplicationCreateInfo &createInfo)
		: Application(createInfo)
	{
		PushLayer(new ignite::SandboxLayer("Runtime Layer"));
	}
};

namespace ignite
{
	Application *CreateApplication(const ApplicationCommandLineArgs args)
	{
		ApplicationCreateInfo createInfo;
		createInfo.cmdLineArgs = args;
		createInfo.name = "Ignite Runtime";
		createInfo.width = 1640;
		createInfo.height = 940;
		createInfo.useGui = true;
		createInfo.maximized = true;

		// vulkan by default
		createInfo.graphicsApi = nvrhi::GraphicsAPI::VULKAN;
		return new SandboxApp(createInfo);
	}

	SandboxLayer::SandboxLayer(const std::string &name)
		: Layer(name)
	{
	}

	SandboxLayer::~SandboxLayer()
	{
	}

	void SandboxLayer::OnAttach()
	{
		Layer::OnAttach();

		auto width = static_cast<float>(Application::GetInstance()->GetCreateInfo().width);
		auto height = static_cast<float>(Application::GetInstance()->GetCreateInfo().height);

		m_Device = DeviceManager::GetInstance()->GetDevice();
		m_SceneRenderer = CreateRef<SceneRenderer>();
		m_Cmd = m_Device->createCommandList();

		// Create camera
		{
			m_Camera = BasicCamera();
			m_Camera.distance = 5.5f;
			m_Camera.yaw = glm::radians(90.0f);
			m_Camera.pitch = 0.0f;
			m_Camera.projectionType = ProjectionType::Perspective;

			m_Camera.UpdateSphericalPosition();
			m_Camera.UpdateMatrices(width, height);
		}

		// Create default project and scene
		{
			ProjectInfo projectInfo;
			projectInfo.name = "New Project";
			m_Project = ProjectSerializer::Deserialize("D:/Dev/TestProject/TestProject.ixproj");
			m_Scene = Scene::Create(m_Project.get(), "new scene");

			m_SceneRenderer->SetActiveScene(m_Scene);
		}

		CreateRenderTargets();

		Application::GetInstance()->GetWindow()->Show();
	}

	void SandboxLayer::OnDetach()
	{
		Layer::OnDetach();
	}

	void SandboxLayer::OnUpdate(float deltaTime)
	{
		if (!m_Scene)
			return;

		UpdateCameraInput(deltaTime);
		m_Scene->OnUpdateEdit(deltaTime);
	}

	void SandboxLayer::OnRender(nvrhi::IFramebuffer *mainFramebuffer)
	{
		Layer::OnRender(mainFramebuffer);

		m_Cmd = m_Device->createCommandList();
		m_Cmd->open();

		// Update scene GPU data
		m_Scene->WriteBuffer(m_Cmd);

		// Setup camera constants
		CameraBuffer cameraBuffer = { m_Camera.projection, m_Camera.view, glm::vec4(m_Camera.position, 1.0f) };
		Renderer::GetCameraConstantBuffer()->SetData(m_Cmd, Buffer(&cameraBuffer, sizeof(CameraBuffer)));

		// Clear Render Targets
		// far depth = 1.0f == LessOrEqual
		m_UIRT->ClearColorAttachmentFloat(m_Cmd, 0);
		m_UIRT->ClearDepthAttachment(m_Cmd, 1.0f, 0);

		m_SceneRT->ClearColorAttachmentFloat(m_Cmd, 0);
		m_SceneRT->ClearDepthAttachment(m_Cmd, 1.0f, 0);

		m_CompositeRT->ClearColorAttachmentFloat(m_Cmd, 0);
		m_CompositeRT->ClearDepthAttachment(m_Cmd, 1.0f, 0);

		nvrhi::IFramebuffer *sceneFB = m_SceneRT->GetFramebuffer();

		// 2D Pass
		m_SceneRenderer->GetRenderer2D()->Begin(m_Cmd);

		m_SceneRenderer->GetRenderer2D()->DrawQuad(glm::mat4(1.0f), glm::vec4(1.0f));

		m_SceneRenderer->GetRenderer2D()->Flush(mainFramebuffer);
		m_SceneRenderer->GetRenderer2D()->End();

		// Composite Pass
		m_SceneRenderer->CompositePass(m_Cmd, m_CompositeRT->GetFramebuffer(), m_SceneRT->GetColorAttachment(0), m_UIRT->GetColorAttachment(0));
		m_Cmd->close();

		GPUUploadSync::DeviceWaitIdle(m_Device);
		{
			std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
			m_Device->executeCommandList(m_Cmd);
		}
	}

	void SandboxLayer::OnGuiRender()
	{
		ImGui::ShowDemoWindow();
	}

	// Events
	void SandboxLayer::OnEvent(Event &event)
	{
		Layer::OnEvent(event);

		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<FramebufferResizeEvent>(BIND_CLASS_EVENT_FN(SandboxLayer::OnResizeEvent));
		dispatcher.Dispatch<KeyPressedEvent>(BIND_CLASS_EVENT_FN(SandboxLayer::OnKeyPressedEvent));
		dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_CLASS_EVENT_FN(SandboxLayer::OnMouseButtonPressedEvent));
		dispatcher.Dispatch<MouseMovedEvent>(BIND_CLASS_EVENT_FN(SandboxLayer::OnMouseMovedEvent));
	}

	bool SandboxLayer::OnKeyPressedEvent(KeyPressedEvent &event) { return false; }
	bool SandboxLayer::OnMouseButtonPressedEvent(MouseButtonPressedEvent &event) { return false;}
	bool SandboxLayer::OnMouseMovedEvent(MouseMovedEvent &event) { return false;}

	bool SandboxLayer::OnResizeEvent(FramebufferResizeEvent &event)
	{
		const uint32_t width = event.GetWidth();
		const uint32_t height = event.GetHeight();

		m_SceneRT->Resize(width, height);
		m_CompositeRT->Resize(width, height);
		m_UIRT->Resize(width, height);

		if (m_Scene)
		{
			m_Scene->Resize(width, height);
			m_Camera.UpdateMatrices(static_cast<float>(width), static_cast<float>(height));
		}

		return false;
	}

	// Helper function
	void SandboxLayer::CreateRenderTargets()
	{
		// Create scene render target
		RenderTargetCreateInfo rtCreateInfo = {};
		rtCreateInfo.attachments =
		{
			FramebufferAttachments{ "[Scene DepthAttachment]", nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite}, // Depth
			FramebufferAttachments{ "[Scene ColorAttachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget} // Main Color
		};
		m_SceneRT = RenderTarget::Create(rtCreateInfo, "[SceneViewportRT]");
		m_UIRT = RenderTarget::Create(rtCreateInfo, "[UIViewportRT]");

		// Composite render target
		{
			RenderTargetCreateInfo rtCreateInfo = {};
			rtCreateInfo.attachments =
			{
				//FramebufferAttachments{ "[Composite Depth Attachment]", nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite }, // Depth
				FramebufferAttachments{ "[Composite Color Attachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget} // Main Color
			};

			m_CompositeRT = RenderTarget::Create(rtCreateInfo, "[CompositeViewportRT]");
		}
	}

	
	void SandboxLayer::UpdateCameraInput(float deltaTime)
	{
		for (const Ref<Joystick> &j : JoystickManager::GetConnectedJoystick())
		{
			const glm::vec2 &camViewAxis = j->GetRightAxis();
			const glm::vec2 &camMoveAxis = j->GetLeftAxis();
			const glm::vec2 &l2r2 = j->GetTriggerAxis();

			m_Camera.yaw += deltaTime * camViewAxis.x;
			m_Camera.pitch += deltaTime * camViewAxis.y;

			// m_Camera.position += m_Camera.GetForwardDirection() * deltaTime * m_CameraData.moveSpeed * -camMoveAxis.y;
			// m_Camera.position += m_Camera.GetRightDirection() * deltaTime * m_CameraData.moveSpeed * camMoveAxis.x;

			// LOG_INFO(j->ToString());
		}

		m_Camera.UpdateMouseState();
		m_Camera.HandleOrbit(deltaTime);
		m_Camera.HandlePan(deltaTime);
		m_Camera.HandleZoom(deltaTime);
		m_Camera.ApplyInertia(deltaTime);
		m_Camera.UpdateCameraPosition();
	}

}
