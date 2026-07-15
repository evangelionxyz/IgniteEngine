/* MIT License
* 
* Copyright (c) 2026 Evangelion Manuhutu
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

#include "ignite/core/base.hpp"
#include "component_group.hpp"
#include "ignite/core/uuid.hpp"
#include "entity.hpp"
#include <type_traits>

namespace ignite
{
    class IGN_API SceneManager
    {
    public:
        static Entity CreateEntity(Scene *scene, const std::string &name, EntityType type, UUID uuid = UUID());
        
        static Entity CreateSprite(Scene *scene, const std::string &name, UUID uuid = UUID());
        static Entity CreateCircle(Scene *scene, const std::string &name, UUID uuid = UUID());
        static Entity CreatePointLight2D(Scene *scene, const std::string &name, UUID uuid = UUID());

        static Entity CreateMesh(Scene *scene, const std::string &name, UUID uuid = UUID());
        static Entity CreateCamera(Scene *scene, const std::string &name, UUID uuid = UUID());
        static Entity CreateWorldEnvironment(Scene *scene, const std::string &name, UUID uuid = UUID());
        static Entity CreateEmptyEntity(Scene *scene, const std::string &name, UUID uuid = UUID());

        static void RenameEntity(Scene *scene, Entity entity, const std::string &newName);
        static void DestroyEntity(Scene *scene, Entity entity);
        static void DestroyEntity(Scene *scene, UUID uuid);
        static Entity GetEntity(Scene *scene, UUID uuid);
        static Entity GetEntity(Scene *scene, const std::string &name);
        static Entity DuplicateEntity(Scene *scene, Entity entity, bool addToParent = true);

        static bool AddChild(Scene *scene, Entity destination, Entity source);
        static bool ChildExists(Scene *scene, Entity destination, Entity source);
        static bool IsParent(Scene *scene, UUID target, UUID source);
        static Entity FindChild(Scene *scene, Entity parent, UUID uuid);

        static Ref<Scene> Copy(Ref<Scene> &other);

        using EntityMap = std::unordered_map<UUID, entt::entity>;
        using EntityComponents = std::unordered_map<entt::entity, std::vector<IComponent *>>;

        template<typename... Component>
        static void CopyComponent(entt::registry *destRegistry, entt::registry *srcRegistry, const EntityMap &entityMap)
        {
            ([&]()
                {
                    auto view = srcRegistry->view<Component>();
                    for (auto srcEntity : view)
                    {
                        for (auto [uuid, destEntity] : entityMap)
                        {
                            // key (UUID)
                            if (uuid == srcRegistry->get<IDComponent>(srcEntity).uuid)
                            {
                                destRegistry->emplace_or_replace<Component>(destEntity, srcRegistry->get<Component>(srcEntity));
                            }
                        }
                    }
                }(), ...
            );
        }
    
        template<typename... Component>
        static void CopyComponent(ComponentGroup<Component...>, entt::registry *destRegistry, entt::registry *srcRegistry, const EntityMap &entityMap)
        {
            CopyComponent<Component...>(destRegistry, srcRegistry, entityMap);
        }
    
        template <typename... Component>
        static void CopyComponentIfExists(Entity dstEntity, Entity srcEntity)
        {
            ([&]()
            {
                if (srcEntity.HasComponent<Component>())
                {
                    dstEntity.AddOrReplaceComponent<Component>(srcEntity.GetComponent<Component>());
                }
            }(), ...);
        }
    
        template<typename... Component>
        static void CopyComponentIfExists(ComponentGroup<Component...>, Entity dstEntity, Entity srcEntity)
        {
            CopyComponentIfExists<Component...>(dstEntity, srcEntity);
        }

        static void TransitionTo(AssetHandle nextSceneHandle);
        static void ExecutePendingTransition();

    private:
        static bool s_TransitionPending;
        static AssetHandle s_PendingSceneHandle;
    };
}

