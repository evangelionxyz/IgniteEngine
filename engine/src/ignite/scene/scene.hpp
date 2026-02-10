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

#define GLM_ENABLE_EXPERIMENTAL
#include <entt/entt.hpp>

#include "ignite/core/types.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/math/aabb.hpp"

#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/gpu_data.hpp"

#include <unordered_map>

namespace ignite
{
    class CameraComponent;
    class Physics2D;
    class JoltScene;
    class Entity;
    class Environment;
    class SceneRenderer;
    class Project;

    class Scene : public Asset
    {
    public:
        Scene() = default;
        explicit Scene(Project *project, const std::string &name);

        ~Scene();

        void OnStart();
        void OnStop();

        void UpdateTransforms(float deltaTime);
        void UpdateTransformRecursive(Entity entity, const glm::mat4 &parentWorldTransform);
        
        void OnUpdateRuntimeSimulate(f32 deltaTime);
        void OnUpdateEdit(f32 deltaTime);
        void Resize(uint32_t width, uint32_t height);
        void WriteBuffer(nvrhi::ICommandList* cmd);
        void SetSceneRenderer(SceneRenderer *sceneRenderer) { m_SceneRenderer = sceneRenderer; }

        template<typename T>
        void OnComponentAdded(Entity entity, T &comp);

        Entity GetPrimaryCamera();
        Project *GetProject() { return m_Project; }

        std::string name;
        entt::registry *registry;
        Scope<Physics2D> physics2D;
        Scope<JoltScene> physics;
        std::unordered_map<UUID, entt::entity> entities; // uuid to entity
        Scene_GPUData gpuData;
        
        bool IsPlaying() const { return m_Playing; }
        
        static Ref<Scene> Create(Project *project, const std::string &name);
        
        SceneRenderer *GetSceneRenderer() { return m_SceneRenderer; }
        Ref<ConstantBuffer> GetSceneGPUDataBuffer() { return m_SceneGPUDataBuffer; }
        Ref<ConstantBuffer> GetCSMGPUDataBuffer() { return m_CSMGPUDataBuffer; }

        glm::vec3 physicsGravity{ 0.0f, -9.8f, 0.0f };
        float timeInSeconds = 0.0f;
        uint32_t viewportWidth = 1280, viewportHeight = 720;

        static AssetType GetStaticType() { return AssetType::Scene; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

    private:
        SceneRenderer *m_SceneRenderer;
        Ref<ConstantBuffer> m_SceneGPUDataBuffer;
        Ref<ConstantBuffer> m_CSMGPUDataBuffer;

        Project *m_Project;

        bool m_Playing = false;
    };
}
