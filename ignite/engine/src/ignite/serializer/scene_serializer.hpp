// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_SCENE_SERIALIZER_HPP
#define IGN_SCENE_SERIALIZER_HPP

#include "serializer.hpp"

namespace ignite
{
    class Scene;
    class Project;

    class IGN_API SceneSerializer
    {
    public:
        SceneSerializer(const Ref<Scene> &scene, Project *project);
        bool Serialize(const std::filesystem::path &filepath);

        static Ref<Scene> Deserialize(const std::filesystem::path &filepath, Project *project);

    private:
        Ref<Scene> m_Scene;
        Project *m_Project;
    };
}

#endif
