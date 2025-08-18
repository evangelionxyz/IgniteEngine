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
#include "runtime_layer.hpp"

class RuntimeApp final : public ignite::Application
{
public:
    explicit RuntimeApp(const ignite::ApplicationCreateInfo &createInfo)
        : Application(createInfo)
    {
        PushLayer(new ignite::RuntimeLayer("Runtime Layer"));
    }
};

namespace ignite
{
    Application *CreateApplication(const ApplicationCommandLineArgs args)
    {
        ApplicationCreateInfo createInfo;
        createInfo.cmdLineArgs = args;
        createInfo.width = 1640;
        createInfo.height = 940;
        createInfo.useGui = true;
        createInfo.maximized = true;

        // vulkan by default
        createInfo.graphicsApi = nvrhi::GraphicsAPI::VULKAN;
        return new RuntimeApp(createInfo);
    }
}

