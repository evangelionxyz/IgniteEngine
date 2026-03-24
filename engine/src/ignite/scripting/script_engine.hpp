// Copyright (c) 2026 Evangelion Manuhutu

#ifndef SCRIPT_ENGINE_HPP
#define SCRIPT_ENGINE_HPP

#include "ignite/scene/scene.hpp"
#include "ignite/scene/entity.hpp"
#include "script_instance.hpp"
#include "script_host.hpp"

#include "FileWatch.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace ignite
{
    class Project;
    class Scene;

    class ScriptEngine
    {
    public:
        ScriptEngine(Project *project);
        ~ScriptEngine();

        void RegisterCoreClassesAndFunctions();

        bool LoadCoreAssembly(const std::filesystem::path &filepath);
        bool LoadAppAssembly(const std::filesystem::path &filepath);
        void ReloadAssembly();
        void SetSceneContext(Scene *scene);
        void ClearSceneContext();
        
        bool EntityClassExists(const std::string &fullClassName);
        
        void OnCreateEntity(Entity entity);
        void OnDestroyEntity(Entity entity);
        void OnUpdateEntity(Entity entity, float time);
        
        std::shared_ptr<ScriptClass> GetEntityClassesByName(const std::string &name);
        std::unordered_map<std::string, std::shared_ptr<ScriptClass>> GetEntityClasses();
        
        std::shared_ptr<ScriptInstance> GetEntityScriptInstance(UUID uuid);
        std::vector<std::string> GetScriptClassStorage();
        Scene *GetSceneContext();
        ScriptHost *GetScriptHost();

        static ScriptEngine *GetInstance();

    private:
        void InitHostFxr();
        void ShutdownHostFxr();
        static void OnAppAssemblyFileSystemEvent(const std::string &path, const filewatch::Event eventType);

        void LoadAppAssemblyClasses();

        Project *m_Project;
        Scene *m_Scene;

        friend class ScriptClass;
    };

    namespace Utils
    {
        inline const char *ScriptFieldTypeToString(ScriptFieldType type)
        {
            switch (type)
            {
            case ScriptFieldType::Invalid: return "Invalid";
            case ScriptFieldType::Float:   return "Float";
            case ScriptFieldType::Double:  return "Double";
            case ScriptFieldType::Bool:    return "Boolean";
            case ScriptFieldType::Char:    return "Char";
            case ScriptFieldType::Byte:    return "Byte";
            case ScriptFieldType::Short:   return "Short";
            case ScriptFieldType::Int:     return "Int";
            case ScriptFieldType::Long:    return "Long";
            case ScriptFieldType::UByte:   return "UByte";
            case ScriptFieldType::UShort:  return "UShort";
            case ScriptFieldType::UInt:    return "UInt";
            case ScriptFieldType::ULong:   return "ULong";
            case ScriptFieldType::Vector2: return "Vec2";
            case ScriptFieldType::Vector3: return "Vec3";
            case ScriptFieldType::Vector4: return "Vec4";
            case ScriptFieldType::Entity:  return "Entity";
            }

            LOG_ASSERT(false, "Invalid Script Field Type!");
            return "Invalid";
        }

        inline ScriptFieldType ScriptFieldTypeFromString(std::string_view type)
        {
            if (type == "Invalid") return ScriptFieldType::Invalid;
            if (type == "Float")   return ScriptFieldType::Float;
            if (type == "Double")  return ScriptFieldType::Double;
            if (type == "Boolean") return ScriptFieldType::Bool;
            if (type == "Char")    return ScriptFieldType::Char;
            if (type == "Byte")    return ScriptFieldType::Byte;
            if (type == "Short")   return ScriptFieldType::Short;
            if (type == "Int")     return ScriptFieldType::Int;
            if (type == "Long")    return ScriptFieldType::Long;
            if (type == "UByte")   return ScriptFieldType::UByte;
            if (type == "UShort")  return ScriptFieldType::UShort;
            if (type == "UInt")    return ScriptFieldType::UInt;
            if (type == "ULong")   return ScriptFieldType::ULong;
            if (type == "Vec2")    return ScriptFieldType::Vector2;
            if (type == "Vec3")    return ScriptFieldType::Vector3;
            if (type == "Vec4")    return ScriptFieldType::Vector4;
            if (type == "Entity")  return ScriptFieldType::Entity;

            return ScriptFieldType::Invalid;
        }
    }
}

#endif