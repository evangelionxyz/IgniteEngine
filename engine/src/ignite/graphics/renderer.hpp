/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu
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

#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "ignite/core/types.hpp"
#include "graphics_pipeline.hpp"

#include <nvrhi/nvrhi.h>
#include <string>
#include <unordered_map>

namespace ignite
{
#define RENDER_MODE_COLOR 0
#define RENDER_MODE_DIFFUSE 1
#define RENDER_MODE_NORMALS 2
#define RENDER_MODE_METALLIC 3
#define RENDER_MODE_ROUGHNESS 4

    class DeviceManager;
    class Texture;
    class Shader;
	class ConstantBuffer;

    enum class GLayoutMap
    {
        MESH,
        MESH_ANIM, 
        MATERIAL,
        ENVIRONMENT,
    };

    struct RendererStats
    {
        // 2D Statistics
        size_t quadCount = 0;
        size_t lineCount = 0;
        size_t circleCount = 0;
        size_t textCount = 0;
        size_t pointLight2dCount = 0;

        size_t quadVerticesSize = 0;
        size_t quadIndicesSize = 0;
        size_t lineVerticesSize = 0;
        size_t circleVerticesSize = 0;
        size_t circleIndicesSize = 0;
        size_t textVerticesSize = 0;
        size_t textIndicesSize = 0;
    };

    class Renderer
    {
    public:
        Renderer() = default;
        Renderer(DeviceManager *deviceManager, nvrhi::GraphicsAPI api);

        ~Renderer();

        static void BeginStats();
        
        static Ref<Texture> GetWhiteTexture();
        static Ref<Texture> GetBlackTexture();
        static Ref<Texture> GetMagentaTexture();

        static nvrhi::GraphicsAPI GetGraphicsAPI();
        static nvrhi::BindingLayoutHandle GetBindingLayout(GLayoutMap type);

        static Ref<DXCInstance> GetDXCInstance();

        static RendererStats Stats;

    private:

        nvrhi::GraphicsAPI m_GraphicsAPI;
        Ref<DXCInstance> m_DxcInstance;

        std::unordered_map<GLayoutMap, nvrhi::BindingLayoutHandle> m_BindingLayouts;

        Ref<Texture> m_WhiteTexture;
        Ref<Texture> m_BlackTexture;
        Ref<Texture> m_MagentaTexture;

        nvrhi::IDevice *m_Device;
        std::vector<std::function<void(nvrhi::ICommandList *)>> m_SubmitFuncs;

        friend class ShaderLibrary;
    };
}

#endif

