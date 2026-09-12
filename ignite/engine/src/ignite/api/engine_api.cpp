// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "engine_api.h"
#include "ignite/core/application.hpp"
#include "ignite/core/layer.hpp"
#include "ignite/graphics/window.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_camera.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/scene/editor_camera.hpp"
#include "ignite/core/input/mouse_event.hpp"

namespace ignite
{
    class EmbeddedViewportLayer final : public Layer
    {
    public:
        EmbeddedViewportLayer(uint32_t width, uint32_t height)
            : Layer("EmbeddedViewportLayer")
            , m_Width(width)
            , m_Height(height)
            , m_EditorCamera("Embedded Viewport Camera")
        {
            m_EditorCamera.SetTarget(glm::vec3(0.0f));
            m_EditorCamera.SetDistance(24.0f);
            m_EditorCamera.yaw = glm::radians(45.0f);
            m_EditorCamera.pitch = glm::radians(25.0f);
            m_EditorCamera.farPlane = 1000.0f;
            m_EditorCamera.UpdateSphericalPosition();
            m_EditorCamera.UpdateView();
            m_EditorCamera.UpdateProjection(width, height);
            m_EditorCamera.SetNavigationMode(EditorCamera::NavigationMode::Orbit);
        }

        void OnAttach() override
        {
            m_SceneRenderer = CreateRef<SceneRenderer>();
            m_FallbackScene = Scene::Create(nullptr);
        }

        void OnDetach() override
        {
            m_SceneRenderer = nullptr;
            m_FallbackScene = nullptr;
        }

        void Resize(uint32_t width, uint32_t height)
        {
            m_Width = width;
            m_Height = height;
            m_EditorCamera.UpdateProjection(width, height);
        }

        void OnUpdate(float deltaTime) override
        {
            Ref<Scene> activeScene = GetCurrentScene();
            if (activeScene)
            {
                activeScene->OnUpdateEdit(deltaTime);
            }

            // Native camera update using SDL3 InputSystem
            m_EditorCamera.UpdateMouseState();
            switch (m_EditorCamera.GetNavigationMode())
            {
            case EditorCamera::NavigationMode::Fly:
                m_EditorCamera.HandleFly(deltaTime);
                m_EditorCamera.HandlePan(deltaTime);
                m_EditorCamera.HandleZoom(deltaTime);
                break;
            case EditorCamera::NavigationMode::Mode2D:
                m_EditorCamera.HandlePan(deltaTime);
                m_EditorCamera.HandleZoom(deltaTime);
                break;
            case EditorCamera::NavigationMode::Orbit:
            default:
                m_EditorCamera.HandleOrbit(deltaTime);
                m_EditorCamera.HandlePan(deltaTime);
                m_EditorCamera.HandleZoom(deltaTime);
                break;
            }
            m_EditorCamera.ApplyInertia(deltaTime);
            m_EditorCamera.UpdateCameraPosition(deltaTime);
            m_EditorCamera.UpdateView();
        }

        void OnRender(nvrhi::IFramebuffer *mainFramebuffer) override
        {
            if (!m_SceneRenderer)
                return;

            Ref<Scene> activeScene = GetCurrentScene();
            if (!activeScene)
                return;

            ICamera *cameraToUse = &m_EditorCamera;

            m_SceneRenderer->ResizeFramebuffer(cameraToUse, m_Width, m_Height);
            m_SceneRenderer->SetActiveScene(activeScene);
            m_SceneRenderer->BeginFrame();

            FrameContext *frameContext = Renderer::GetCurrentFrameContext();
            m_SceneRenderer->Render(cameraToUse, frameContext, false, mainFramebuffer);
        }

        Ref<Scene> GetCurrentScene()
        {
            if (Ref<Project> activeProj = Project::GetActive())
            {
                if (Ref<Scene> projScene = activeProj->LockActiveScene())
                {
                    return projScene;
                }
            }
            return m_FallbackScene;
        }

        void OnEvent(Event &e) override
        {
            EventDispatcher dispatcher(e);
            dispatcher.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent &event) {
                m_EditorCamera.mouse.scroll = { static_cast<int>(event.GetXOffset()), static_cast<int>(event.GetYOffset()) };
                return false;
            });
        }

        void SetNavigationMode(int mode)
        {
            m_EditorCamera.SetNavigationMode(static_cast<EditorCamera::NavigationMode>(mode));
            m_EditorCamera.UpdateView();
            m_EditorCamera.UpdateProjection(m_Width, m_Height);
        }

        int GetNavigationMode() const
        {
            return static_cast<int>(m_EditorCamera.GetNavigationMode());
        }

        EditorCamera &GetEditorCamera() { return m_EditorCamera; }

    private:
        Ref<SceneRenderer> m_SceneRenderer;
        Ref<Scene> m_FallbackScene;
        EditorCamera m_EditorCamera;
        uint32_t m_Width = 1280;
        uint32_t m_Height = 720;
    };

    class NativeEmbeddedApp final : public Application
    {
    public:
        explicit NativeEmbeddedApp(const ApplicationCreateInfo &createInfo)
            : Application(createInfo)
        {
            m_ViewportLayer = new EmbeddedViewportLayer(createInfo.width, createInfo.height);
            PushLayer(m_ViewportLayer);
        }

        void ResizeEmbedded(uint32_t width, uint32_t height)
        {
            Application::Resize(width, height);
            if (m_ViewportLayer)
            {
                m_ViewportLayer->Resize(width, height);
            }
        }

        void SetCameraNavigationMode(int mode)
        {
            if (m_ViewportLayer)
                m_ViewportLayer->SetNavigationMode(mode);
        }

        int GetCameraNavigationMode() const
        {
            if (m_ViewportLayer)
                return m_ViewportLayer->GetNavigationMode();
            return 0;
        }

        EmbeddedViewportLayer *GetViewportLayer() { return m_ViewportLayer; }

    private:
        EmbeddedViewportLayer *m_ViewportLayer = nullptr;
    };

}


static ignite::NativeEmbeddedApp *s_EmbeddedApp = nullptr;

// Static string buffers for returning strings to C#
static std::string s_ProjectNameBuffer;
static std::string s_ProjectDirBuffer;
static std::string s_ProjectFilePathBuffer;
static std::string s_ProjectAssetDirBuffer;

extern "C" {

IGN_API bool Ignite_Init(const IgniteAppConfig *config)
{
    if (s_EmbeddedApp)
    {
        return true;
    }

    ignite::Logger::Init();

    ignite::ApplicationCreateInfo createInfo{};
    createInfo.name = "Ignite Viewport";
    createInfo.width = config ? config->width : 1280;
    createInfo.height = config ? config->height : 720;
    createInfo.offscreen = config ? config->offscreen : false;
    createInfo.nativeWindowHandle = config ? config->nativeWindowHandle : nullptr;
    createInfo.useGui = false; // Avalonia manages UI

    if (config)
    {
        createInfo.graphicsApi = (config->graphicsApi == 1) ? nvrhi::GraphicsAPI::D3D12 : nvrhi::GraphicsAPI::VULKAN;
    }
    else
    {
        createInfo.graphicsApi = nvrhi::GraphicsAPI::VULKAN;
    }

    s_EmbeddedApp = new ignite::NativeEmbeddedApp(createInfo);
    return s_EmbeddedApp != nullptr;
}

IGN_API void Ignite_Tick(float deltaTime)
{
    if (s_EmbeddedApp)
    {
        s_EmbeddedApp->Step(deltaTime);
    }
}

IGN_API void Ignite_Resize(uint32_t width, uint32_t height)
{
    if (s_EmbeddedApp)
    {
        s_EmbeddedApp->ResizeEmbedded(width, height);
    }
}

IGN_API void Ignite_Shutdown()
{
    if (s_EmbeddedApp)
    {
        delete s_EmbeddedApp;
        s_EmbeddedApp = nullptr;
        ignite::Logger::Shutdown();
    }
}

IGN_API void *Ignite_GetNativeWindow()
{
    if (s_EmbeddedApp && s_EmbeddedApp->GetWindow())
    {
        return (void*)s_EmbeddedApp->GetWindow()->GetNativeWindow();
    }
    return nullptr;
}

IGN_API void *Ignite_GetSharedTextureHandle()
{
    ignite::DeviceManager *dm = ignite::DeviceManager::GetInstance();
    if (dm)
    {
        return dm->GetSharedBackBufferHandle();
    }
    return nullptr;
}

IGN_API bool Ignite_ReadbackViewportPixels(void *outBuffer, uint32_t bufferSize)
{
    ignite::DeviceManager *dm = ignite::DeviceManager::GetInstance();
    if (dm)
    {
        return dm->ReadbackBackBuffer(outBuffer, bufferSize);
    }
    return false;
}

IGN_API void Ignite_SetLogCallback(IgniteLogCallback callback)
{
    ignite::Logger::SetLogCallback(callback);
}

// ======================================
// Project API
// ======================================
IGN_API bool Ignite_Project_New(const char *name, const char *parentDirectory)
{
    if (!name || !parentDirectory)
        return false;

    bool success = false;
    std::string nameStr = name;
    std::string dirStr = parentDirectory;

    std::thread t([&]() {
        Ref<ignite::Project> proj = ignite::Project::New(nameStr, dirStr);
        success = (proj != nullptr);
    });

    if (t.joinable())
        t.join();
    return success;
}

IGN_API bool Ignite_Project_Open(const char *filepath)
{
    if (!filepath)
        return false;

    bool success = false;
    std::string pathStr = filepath;

    std::thread t([&]() {
        Ref<ignite::Project> proj = ignite::Project::Open(pathStr);
        success = (proj != nullptr);
    });

    if (t.joinable())
        t.join();
    return success;
}

IGN_API bool Ignite_Project_Save()
{
    return ignite::Project::SaveActive();
}

IGN_API void Ignite_Project_Close()
{
    ignite::Project::CloseActive();
}

IGN_API bool Ignite_Project_IsOpen()
{
    return ignite::Project::GetActive() != nullptr;
}

IGN_API const char *Ignite_Project_GetName()
{
    if (Ref<ignite::Project> active = ignite::Project::GetActive())
    {
        s_ProjectNameBuffer = active->GetInfo().name;
        return s_ProjectNameBuffer.c_str();
    }
    return "";
}

IGN_API const char *Ignite_Project_GetDirectory()
{
    if (Ref<ignite::Project> active = ignite::Project::GetActive())
    {
        s_ProjectDirBuffer = active->GetDirectory().generic_string();
        return s_ProjectDirBuffer.c_str();
    }
    return "";
}

IGN_API const char *Ignite_Project_GetFilePath()
{
    if (Ref<ignite::Project> active = ignite::Project::GetActive())
    {
        s_ProjectFilePathBuffer = active->GetFilepath().generic_string();
        return s_ProjectFilePathBuffer.c_str();
    }
    return "";
}

IGN_API const char *Ignite_Project_GetAssetDirectory()
{
    if (Ref<ignite::Project> active = ignite::Project::GetActive())
    {
        s_ProjectAssetDirBuffer = active->GetAssetDirectory().generic_string();
        return s_ProjectAssetDirBuffer.c_str();
    }
    return "";
}

// Asset Manager API
IGN_API uint32_t Ignite_AssetManager_GetAssetCount()
{
    if (auto am = ignite::AssetManager::GetInstance())
    {
        return static_cast<uint32_t>(am->GetAssetAssetRegistry().size());
    }
    return 0;
}

IGN_API void Ignite_AssetManager_Refresh()
{
    if (auto am = ignite::AssetManager::GetInstance())
    {
        am->SyncFromRust();
    }
}

IGN_API void Ignite_AssetManager_SyncFromRust()
{
    if (auto am = ignite::AssetManager::GetInstance())
    {
        am->SyncFromRust();
    }
}

// ======================================
// Editor Camera API
// ======================================

IGN_API void Ignite_Camera_SetNavigationMode(int mode)
{
    if (s_EmbeddedApp)
        s_EmbeddedApp->SetCameraNavigationMode(mode);
}

IGN_API int Ignite_Camera_GetNavigationMode()
{
    if (s_EmbeddedApp)
        return s_EmbeddedApp->GetCameraNavigationMode();
    return 0;
}

}
