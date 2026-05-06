//Copyright (c) 2026 Evangelion Manuhutu

#include <ignite/core/application.hpp>
#include "editor_layer.hpp"
#include "editor_app.hpp"

namespace
{
    void *g_EditorHostWindowHandle = nullptr;
    std::function<void()> g_EditorPlatformEventPump;
}

class EditorApp final : public ignite::Application
{
public:
    explicit EditorApp(const ignite::ApplicationCreateInfo &createInfo)
        : Application(createInfo)
    {
        PushLayer(new ignite::EditorLayer("Ignite Editor Layer"));
    }
};

namespace ignite
{
    void ConfigureEditorHost(void *nativeHostWindowHandle, std::function<void()> platformEventPump)
    {
        g_EditorHostWindowHandle = nativeHostWindowHandle;
        g_EditorPlatformEventPump = std::move(platformEventPump);
    }

    Application *CreateApplication(const ApplicationCommandLineArgs args)
    {
        ApplicationCreateInfo createInfo;
        createInfo.cmdLineArgs = args;
		createInfo.name = "Ignite Editor";
        createInfo.width = 1640;
        createInfo.height = 940;
        createInfo.useGui = true;
        createInfo.maximized = false;
        createInfo.nativeHostWindowHandle = g_EditorHostWindowHandle;
        createInfo.platformEventPump = g_EditorPlatformEventPump;

        // vulkan by default
        createInfo.graphicsApi = nvrhi::GraphicsAPI::VULKAN;
        return new EditorApp(createInfo);
    }
}

