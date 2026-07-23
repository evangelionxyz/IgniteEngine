// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_ENTITY_SERIALIZER_HPP
#define IGN_ENTITY_SERIALIZER_HPP

#include "serializer.hpp"
#include "ignite/scene/entity.hpp"

namespace ignite
{
    class Scene;
    class Project;

    class IGN_API EntitySerializer
    {
    public:
        static void SerializeEntity(Serializer &sr, Entity entity);
        static Entity DeserializeEntity(const YAML::Node &entityNode, Scene *scene, Project *project);
    };
}

#endif
