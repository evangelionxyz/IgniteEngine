// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_PREFAB_HPP
#define IGN_PREFAB_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/entity.hpp"

namespace ignite
{
    class Project;

    class IGN_API Prefab final : public Asset
    {
    public:
        Prefab() = default;
        explicit Prefab(Project *project);
        ~Prefab() override = default;

        Ref<Scene> GetPrefabScene() const { return m_PrefabScene; }
        UUID GetRootEntityUUID() const { return m_RootEntityUUID; }
        void SetRootEntityUUID(UUID uuid) { m_RootEntityUUID = uuid; }

        bool Serialize(const ignite::Path &filepath) override;
        static Ref<Prefab> Deserialize(const ignite::Path &filepath, Project *project);

        static Ref<Prefab> CreateFromEntity(Entity entity, Scene *scene, Project *project);
        bool UpdateFromEntity(Entity entity, Scene *scene, Project *project);
        static Entity Instantiate(const Ref<Prefab> &prefab, Scene *targetScene);

        static AssetType GetStaticType() { return AssetType::Prefab; }
        AssetType GetAssetType() override { return GetStaticType(); }

    private:
        Ref<Scene> m_PrefabScene;
        UUID m_RootEntityUUID = UUID(0);
    };
}

#endif
