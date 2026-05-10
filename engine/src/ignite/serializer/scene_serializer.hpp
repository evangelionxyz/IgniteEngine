// Copyright (c) 2026 Evangelion Manuhutu

#ifndef SCENE_SERIALIZER_HPP
#define SCENE_SERIALIZER_HPP

#include "serializer.hpp"

namespace ignite
{
    class Scene;
    class Project;

    class SceneSerializer
    {
    public:
        SceneSerializer(const Ref<Scene> &scene, Project *project);
        bool Serialize(const ignite::Path &filepath);

        static Ref<Scene> Deserialize(const ignite::Path &filepath, Project *project);

    private:
        Ref<Scene> m_Scene;
        Project *m_Project;
    };
}

#endif