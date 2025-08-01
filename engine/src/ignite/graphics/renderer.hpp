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

#pragma once
#include "ignite/core/types.hpp"
#include "graphics_pipeline.hpp"
#include "command_list.hpp"

#include <nvrhi/nvrhi.h>
#include <ShaderMake/ShaderMake.h>

#include <string>
#include <unordered_map>

namespace ignite
{
    class DeviceManager;
    class Texture;
    class Shader;

    // vertex/pixel shader
    struct ShaderHandleContext
    {
        Ref<ShaderMake::ShaderContext> context;
        nvrhi::ShaderHandle handle;
    };

    enum class GLayoutMap
    {
        MESH,
        MESH_ANIM, 
        MATERIAL, 
        ENVIRONMENT
    };

    class ShaderLibrary
    {
    public:
        void Init(nvrhi::GraphicsAPI api);
        void Compile();
        void CompileShaders(const std::vector<Ref<ShaderMake::ShaderContext>> &contexts);
        void Load(const std::string &name, const std::string &filepath);
        bool Exists(const std::string &name) const;
        
        std::unordered_map<nvrhi::ShaderType, ShaderHandleContext> Get(const std::string &name);

        ShaderMake::Context *GetContext() const;

    private:
        std::unordered_map<std::string, std::unordered_map<nvrhi::ShaderType, ShaderHandleContext>> m_Shaders;
        Scope<ShaderMake::Context> m_ShaderContext = nullptr;
        ShaderMake::Options m_ShaderMakeOptions;
    };

    class Renderer
    {
    public:
        Renderer() = default;
        Renderer(DeviceManager *deviceManager, nvrhi::GraphicsAPI api);

        ~Renderer();
        
        static Ref<Texture> GetWhiteTexture();
        static Ref<Texture> GetBlackTexture();
        static nvrhi::GraphicsAPI GetGraphicsAPI();
        static nvrhi::BindingLayoutHandle GetBindingLayout(GLayoutMap type);

        static void OnUpdate();
        static void Submit(const std::function<void(nvrhi::ICommandList *)> &func);

        static ShaderLibrary &GetShaderLibrary();

    private:
        nvrhi::GraphicsAPI m_GraphicsAPI;
        ShaderLibrary m_ShaderLibrary;

        std::unordered_map<GLayoutMap, nvrhi::BindingLayoutHandle> m_BindingLayouts;
        Ref<CommandList> m_CommandList;

        Ref<Texture> m_WhiteTexture;
        Ref<Texture> m_BlackTexture;

        nvrhi::IDevice *m_Device;
        std::vector<std::function<void(nvrhi::ICommandList *)>> m_SubmitFuncs;

        friend class ShaderLibrary;
    };
}
