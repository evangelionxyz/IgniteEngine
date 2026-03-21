// Copyright (c) 2026 Evangelion Manuhutu

#ifndef SCRIPT_GLUE_HPP
#define SCRIPT_GLUE_HPP

#include <cstdint>
#include <glm/glm.hpp>

namespace ignite
{
    struct ScriptGlueAPI
    {
        void (*Debug_Log)(const char *message);

        bool (*Entity_HasComponent)(uint64_t entityID, const char *componentTypeName);
        void (*Entity_AddComponent)(uint64_t entityID, const char *componentTypeName);
        uint64_t (*Entity_FindEntityByName)(const char *name);
        uint64_t (*Entity_Instantiate)(uint64_t entityID, glm::vec3 value);
        void (*Entity_Destroy)(uint64_t entityID);
        void (*Entity_SetVisibility)(uint64_t entityID, bool value);
        void (*Entity_GetVisibility)(uint64_t entityID, bool *result);

        void (*TransformComponent_GetForward)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetForward)(uint64_t entityID, glm::vec3 value);
        void (*TransformComponent_GetRight)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetRight)(uint64_t entityID, glm::vec3 value);
        void (*TransformComponent_GetUp)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetUp)(uint64_t entityID, glm::vec3 value);
        void (*TransformComponent_GetTranslation)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetTranslation)(uint64_t entityID, glm::vec3 value);
        void (*TransformComponent_GetRotation)(uint64_t entityID, glm::quat *result);
        void (*TransformComponent_SetRotation)(uint64_t entityID, glm::quat value);
        void (*TransformComponent_GetEulerAngles)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetEulerAngles)(uint64_t entityID, glm::vec3 value);
        void (*TransformComponent_GetScale)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetScale)(uint64_t entityID, glm::vec3 value);
    };

    class ScriptGlue
    {
    public:
        static void RegisterComponents();
        static void RegisterFunctions();
        static const ScriptGlueAPI *GetAPI();
    };
}

#endif