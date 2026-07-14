// Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#include "runtime_layer.hpp"
#include "engine/serializer/serializer.hpp"
#include "engine/serializer/binary_serializer.hpp"
#include "engine/core/platform_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scene/scene_manager.hpp"
#include "renderer/graphics/ui_renderer.hpp"
#include "renderer/graphics/renderer.hpp"

#include "engine/asset/asset_importer.hpp"
#include "scripting/script_engine.hpp"
#include "engine/core/profiler/profiler.hpp"

namespace ignite
{
    RuntimeLayer::RuntimeLayer(const std::string &name)
        : Layer(name)
    {
    }

    RuntimeLayer::~RuntimeLayer()
    {
    }

    void RuntimeLayer::OnAttach() 
    {
        Layer::OnAttach();

        m_Device = Application::GetGraphicsDevice();
        m_CommandList = CommandList::Create();

        m_SceneRenderer.Create();

        const auto &cmdArgs = Application::GetInstance()->GetCreateInfo().cmdLineArgs;
        for (int i = 0; i < cmdArgs.count; ++i)
        {
            std::string args = cmdArgs[i];

            char projectArgs[] = "-project=";
            if (args.find(projectArgs) != std::string::npos)
            {
                std::string projectFilepath = args.substr(std::size(projectArgs) - 1, args.size() - std::size(projectArgs) + 1);
                OpenProject(projectFilepath);
            }
        }

        OpenProject();

        if (m_ActiveProject && m_ActiveScene)
        {
            m_SceneRenderer.SetActiveScene(m_ActiveScene);
            m_ActiveScene->OnStart();
            m_ViewportData.size = Application::GetInstance()->GetWindow()->GetFramebufferSize();

            // Create scene render target
            RenderTargetCreateInfo rtCreateInfo = {};
            rtCreateInfo.attachments =
            {
                FramebufferAttachments{ "[DepthAttachment]", nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite}, // Depth
                FramebufferAttachments{ "[ColorAttachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget} // Main Color
            };

            m_SceneRT = RenderTarget::Create(rtCreateInfo, "[SceneRT]");
            m_UIRT = RenderTarget::Create(rtCreateInfo, "[UIRT]");

            // Composite render target
            rtCreateInfo = {};
            rtCreateInfo.attachments = { FramebufferAttachments{ "[DepthAttachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget}}; // Main Color
            m_CompositeRT = RenderTarget::Create(rtCreateInfo, "[CompositeRT]");

            m_BindingSet = nullptr;

            Application::GetInstance()->GetWindow()->Show(); // Show window after initialization
        }
        else
        {
            if (!m_ActiveScene)
            {
                LOG_ERROR("[Runtime] No Default Scene");
            }

            if (!m_ActiveProject)
            {
                LOG_ERROR("[Runtime] Project is not valid");
            }

            Application::Shutdown();
        }        
    }

    void RuntimeLayer::OnDetach() 
    {
        if (m_ActiveScene)
        {
            m_ActiveScene->OnStop();
        }

        Layer::OnDetach();
    }

    void RuntimeLayer::OnUpdate(float deltaTime) 
    {
        IGN_PROFILE_FUNCTION();
        Layer::OnUpdate(deltaTime);
        const auto &window = Application::GetInstance()->GetWindow();

        Renderer::OnUpdate();

        if (m_ActiveProject && m_ActiveProject->GetActiveScene() != m_ActiveScene)
        {
            m_ActiveScene = m_ActiveProject->GetActiveScene();
            m_SceneRenderer.SetActiveScene(m_ActiveScene);
        }

        if (m_ActiveScene)
        {
            IGN_PROFILE_SCOPE("RuntimeLayer::SceneUpdate");
            m_ViewportData.position = glm::vec2(0.0f);
            m_ViewportData.size = window->GetFramebufferSize();
            m_ViewportData.mousePosition = Input::GetMousePosition();

            // glm::vec2 screenMousePos = {mousePos.x - viewportPos.x, mousePos.y - viewportPos.y};
            bool mousePressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            
            SceneRenderer::GetActive()->UpdateUIInput(m_ViewportData.mousePosition, m_ViewportData.position, m_ViewportData.size, mousePressed);

            m_ActiveScene->OnUpdateRuntimeSimulate(deltaTime);
        }
    }

    void RuntimeLayer::OnRender(nvrhi::IFramebuffer *framebuffer) 
    {
        IGN_PROFILE_FUNCTION();
        Layer::OnRender(framebuffer);

        if (m_ActiveScene)
        {
            CreatePipeline(framebuffer);

            if (Entity primaryCam = m_ActiveScene->GetPrimaryCamera())
            {
                ICamera *camera = &primaryCam.GetComponent<Camera>().camera;
                IGN_PROFILE_SCOPE("SceneRenderer::RenderTo");
                m_SceneRenderer.RenderTo(camera, m_SceneRT, m_UIRT, m_CompositeRT, camera->projectionType == ProjectionType::Perspective);
            }

            UpdateBindingSet();

            m_CommandList->Begin();
            auto cmd = m_CommandList->GetActiveHandle();
            
            auto graphicsState = nvrhi::GraphicsState();
            graphicsState.pipeline = m_CompositePipeline->GetHandle();
            graphicsState.framebuffer = framebuffer;
            graphicsState.vertexBuffers = { nvrhi::VertexBufferBinding { m_ScreenVertexBuffer->GetHandle(), 0, 0 } };
            graphicsState.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());
            graphicsState.bindings = { m_BindingSet };
            cmd->setGraphicsState(graphicsState);

            auto args = nvrhi::DrawArguments();
            args.instanceCount = 1;
            args.vertexCount = 6;
            cmd->draw(args);

            m_CommandList->Submit();
        }
    }

    void RuntimeLayer::OnEvent(Event &e) 
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<FramebufferResizeEvent>(BIND_CLASS_EVENT_FN(OnResizeEvent));
    }

    bool RuntimeLayer::OnResizeEvent(FramebufferResizeEvent &event)
    {
        if (m_ActiveScene)
        {
            // UIManager::GetInstance().SetViewportSize(event.GetWidth(), event.GetHeight());

            const uint32_t width = event.GetWidth();
            const uint32_t height = event.GetHeight();

            m_SceneRT->Resize(width, height);
            m_UIRT->Resize(width, height);
            m_CompositeRT->Resize(width, height);

            m_ActiveScene->Resize(width, height);

            m_SceneRenderer.GetUIRenderer()->Resize(width, height);

            m_BindingSet = nullptr;
        }
        return false;
    }

    void RuntimeLayer::UpdateBindingSet()
    {
        // Composite Binding set
        if (!m_BindingSet)
        {
            nvrhi::IDevice *device = Application::GetGraphicsDevice();
            auto bindingSetDesc = nvrhi::BindingSetDesc();
            bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, m_SceneRT->GetColorAttachment(0)));
            bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, m_UIRT->GetColorAttachment(0)));
            bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, Renderer::GetWhiteTexture()->GetSampler()));

            m_BindingSet = device->createBindingSet(bindingSetDesc, m_CompositePipeline->GetBindingLayout(0));
        }
    }

    void RuntimeLayer::CreatePipeline(nvrhi::IFramebuffer *framebuffer)
    {
        if (!m_CompositePipeline)
        {
            // Geometry
            VertexScreen vertices[]
            {
                { { -1.0f, -1.0f }, { 0.0f, 1.0f } },
                { { -1.0f,  1.0f }, { 0.0f, 0.0f } },
                { {  1.0f,  1.0f }, { 1.0f, 0.0f } },

                { {  1.0f,  1.0f }, { 1.0f, 0.0f } },
                { {  1.0f, -1.0f }, { 1.0f, 1.0f } },
                { { -1.0f, -1.0f }, { 0.0f, 1.0f } },
            };

            m_ScreenVertexBuffer = VertexBuffer::Create(sizeof(vertices));
            m_ScreenVertexBuffer->SetData(Buffer(vertices, sizeof(vertices)));

            GraphicsPipelineParams params;
            params.enableBlend = false;
            params.depthWrite = false;
            params.depthTest = false;
            params.enableDepthStencil = false;
            params.fillMode = nvrhi::RasterFillMode::Solid;
            params.cullMode = nvrhi::RasterCullMode::None;

            auto attributes = VertexScreen::GetAttributes();
            GraphicsPipelineCreateInfo pci;
            pci.attributes = attributes.data();
            pci.attributeCount = static_cast<uint32_t>(attributes.size());

            // Binding layout
            nvrhi::BindingLayoutDesc layoutDesc = {};
            layoutDesc.visibility = nvrhi::ShaderType::All;
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0)); // scene
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1)); // ui
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0)); // sampler
            nvrhi::BindingLayoutHandle bindingLayout = m_Device->createBindingLayout(layoutDesc);

            // Create pipeline
            m_CompositePipeline = GraphicsPipeline::Create();
            m_CompositePipeline->AddShader("screen.vertex.hlsl", nvrhi::ShaderType::Vertex)
                .AddShader("screen.pixel.hlsl", nvrhi::ShaderType::Pixel, "main")
                .AddBindingLayout(bindingLayout)
                .Build(framebuffer, params, pci);
        }
    }

    void RuntimeLayer::OnGuiRender() 
    {
        Layer::OnGuiRender();

        ImGui::Begin("Debug Window");

        ImGui::DragFloat2("position", glm::value_ptr(m_ViewportData.position));
        ImGui::DragFloat2("size", glm::value_ptr(m_ViewportData.size));
        ImGui::DragFloat2("mouse", glm::value_ptr(m_ViewportData.mousePosition));

        ImGui::End();
    }

    Ref<Project> RuntimeLayer::OpenProject()
    {
        std::filesystem::path filepath = FileDialogs::OpenFile("Ignite Project (*.ixproj)\0*.ixproj\0");
        Ref<Project> openedProject;
        if (!filepath.empty())
        {
            openedProject = OpenProject(filepath);
        }

        return openedProject;
    }

    Ref<Project> RuntimeLayer::OpenProject(const std::filesystem::path &filepath)
    {
        Ref<Project> project = ProjectSerializer::Deserialize(filepath);
        if (project)
        {
            m_ActiveProject = project;
            m_CurrentProjectPath = filepath;

            // Get Project default scene
            if (m_ActiveProject->GetInfo().defaultSceneHandle != AssetHandle(0))
            {
                if (Ref<Scene> activeScene = project->GetAsset<Scene>(m_ActiveProject->GetInfo().defaultSceneHandle))
                {
                    m_ActiveScene = SceneManager::Copy(activeScene);
                    AssetMetaData metadata = project->GetAssetManager().GetMetaData(activeScene->handle);
                    m_CurrentScenePath = project->GetAssetFilepath(metadata.filepath);
                }
            }
        }
        else
        {
            m_CurrentProjectPath.clear();
            m_CurrentScenePath.clear();
            m_ActiveProject = nullptr;
        }

        return project;
    }
} // namespace ignite
