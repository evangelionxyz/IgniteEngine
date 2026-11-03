// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include <ignite/entry_point.hpp>
#include <ignite/core/application.hpp>
#include "runtime_layer.hpp"

class RuntimeApp final : public ignite::Application
{
public:
    explicit RuntimeApp(const ignite::ApplicationCreateInfo &createInfo)
        : Application(createInfo)
    {
        PushLayer(new ignite::RuntimeLayer("Ignite Runtime Layer", createInfo.cmdLineArgs));
    }
};

namespace ignite
{
    Application *CreateApplication(const ApplicationCommandLineArgs args)
    {
        ApplicationCreateInfo createInfo {};

        // Engine Versioning
        createInfo.version = version::MakeVersion(0, 1, 0);

        createInfo.cmdLineArgs = args;
        createInfo.name = "Ignite Runtime";
        createInfo.width = 1640;
        createInfo.height = 940;
        createInfo.useGui = true;
        createInfo.maximized = true;

        for (int i = 1; i < args.count; i++)
        {
            if (std::string_view(args[i]) == "--nogui")
            {
                createInfo.useGui = false;
            }
        }

        // vulkan by default
        createInfo.graphicsApi = nri::GraphicsAPI::VK;
        return new RuntimeApp(createInfo);
    }
}
