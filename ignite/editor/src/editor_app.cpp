//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#include "pch.hpp"

#include <ignite/entry_point.hpp>
#include <ignite/core/application.hpp>
#include "editor_layer.hpp"

class EditorApp final : public ignite::Application
{
public:
    explicit EditorApp(const ignite::ApplicationCreateInfo &createInfo)
        : Application(createInfo)
    {
        if (ImGuiContext *sharedContext = GetImGuiContext())
        {
            ImGui::SetCurrentContext(sharedContext);
        }

        PushLayer(new ignite::EditorLayer("Ignite Editor Layer"));
    }
};

namespace ignite
{
    Application *CreateApplication(const ApplicationCommandLineArgs args)
    {
        ApplicationCreateInfo createInfo{};

        // Engine Versioning
        createInfo.version = version::MakeVersion(0, 1, 0);

        createInfo.cmdLineArgs = args;
        createInfo.name = "Ignite Editor";
        createInfo.width = 1640;
        createInfo.height = 940;
        createInfo.useGui = true;
        createInfo.maximized = true;

        // vulkan by default
        createInfo.graphicsApi = nvrhi::GraphicsAPI::VULKAN;
        return new EditorApp(createInfo);
    }
}
