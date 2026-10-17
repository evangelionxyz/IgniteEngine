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
        PushLayer(new ignite::RuntimeLayer("Ignite Runtime Layer"));
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
        createInfo.useGui = false;
        createInfo.maximized = true;

        // vulkan by default
        createInfo.graphicsApi = nvrhi::GraphicsAPI::VULKAN;
        return new RuntimeApp(createInfo);
    }
}
