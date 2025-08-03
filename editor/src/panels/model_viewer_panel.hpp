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

#include "ipanel.hpp"
#include "ignite/core/input/event.hpp"
#include "ignite/graphics/command_list.hpp"
#include "ignite/graphics/mesh.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"

#include "editor_camera.hpp"
#include <nvrhi/nvrhi.h>
#include <filesystem>

namespace ignite
{
    class ModelViewerPanel : public IPanel
    {
    public:
        ModelViewerPanel();
        ~ModelViewerPanel();

        virtual void OnGuiRender() override;
        virtual void OnUpdate(f32 deltaTime) override;
        
        void OnEvent(Event& e);

        void OnRender();
    private:
        void LoadModel(const std::filesystem::path& filepath);
        std::filesystem::path m_ModelFilepath;
        Ref<MeshAsset> m_MeshAsset;        
        struct Model
        {
            std::vector<Ref<MeshInstance>> meshes;
            void CreateMeshes(const std::vector<MeshData> &meshData, const std::vector<Ref<Material>> &materials);
        };

        Ref<Model> m_Model;

        Ref<GraphicsPipeline> m_Pipeline;
        Ref<RenderTarget> m_RenderTarget;
        Ref<CommandList> m_CommandList;
        nvrhi::BufferHandle m_CameraBuffer;

        EditorCamera m_Camera;
    };
}