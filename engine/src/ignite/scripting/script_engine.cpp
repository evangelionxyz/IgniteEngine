// Copyright (c) 2026 Evangelion Manuhutu

#include "script_engine.hpp"
#include "script_glue.hpp"
#include "script_class.hpp"
#include "script_host.hpp"

#include "ignite/scene/component.hpp"
#include "ignite/project/project.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/string_utils.hpp"
#include "ignite/core/platform_utils.hpp"
#include "ignite/core/profiler/profiler.hpp"

#include <cstdlib>
#include <format>
#include <fstream>
#include <thread>
#include <chrono>

namespace ignite
{
    namespace
    {
        constexpr const char *kSerializeFieldAttributeTypeName = "Ignite.SerializeFieldAttribute";
        constexpr const char *kEntityTypeName = "Ignite.Entity";
    }

    namespace
    {
        static bool TryGetAssemblyWriteTime(const std::filesystem::path &filepath, std::filesystem::file_time_type &outTime)
        {
            std::error_code ec;
            if (!std::filesystem::exists(filepath, ec) || ec)
            {
                return false;
            }

            outTime = std::filesystem::last_write_time(filepath, ec);
            return !ec;
        }

        static bool WaitForAssemblyFileReady(const std::filesystem::path &filepath)
        {
            using namespace std::chrono_literals;

            uintmax_t lastSize = 0;
            bool hasLastSize = false;
            std::filesystem::file_time_type lastWriteTime{};
            bool hasLastWrite = false;
            int stableCount = 0;

            for (int i = 0; i < 80; i++)
            {
                std::error_code ec;
                if (!std::filesystem::exists(filepath, ec) || ec)
                {
                    std::this_thread::sleep_for(25ms);
                    continue;
                }

                const auto writeTime = std::filesystem::last_write_time(filepath, ec);
                if (ec)
                {
                    std::this_thread::sleep_for(25ms);
                    continue;
                }

                const auto fileSize = std::filesystem::file_size(filepath, ec);
                if (ec)
                {
                    std::this_thread::sleep_for(25ms);
                    continue;
                }

                std::ifstream stream(filepath, std::ios::binary);
                if (!stream.good())
                {
                    std::this_thread::sleep_for(25ms);
                    continue;
                }

                const bool sameWrite = hasLastWrite && writeTime == lastWriteTime;
                const bool sameSize = hasLastSize && fileSize == lastSize;

                if (sameWrite && sameSize)
                {
                    stableCount++;
                    if (stableCount >= 3)
                    {
                        return true;
                    }
                }
                else
                {
                    stableCount = 0;
                }

                hasLastWrite = true;
                hasLastSize = true;
                lastWriteTime = writeTime;
                lastSize = fileSize;

                std::this_thread::sleep_for(25ms);
            }

            return false;
        }

        static bool WaitForAssemblyNewerThan(const std::filesystem::path &filepath, const std::filesystem::file_time_type &previousWriteTime)
        {
            using namespace std::chrono_literals;

            for (int i = 0; i < 120; i++)
            {
                std::filesystem::file_time_type currentWriteTime{};
                if (TryGetAssemblyWriteTime(filepath, currentWriteTime) && currentWriteTime > previousWriteTime)
                {
                    return true;
                }

                std::this_thread::sleep_for(25ms);
            }

            return false;
        }
    }

    static std::unordered_map<std::string, ScriptFieldType> s_ScriptFieldTypeMap =
    {
        {"System.String",  ScriptFieldType::String},
        {"System.Boolean", ScriptFieldType::Bool},
        {"System.Single",  ScriptFieldType::Float},
        {"System.Char",    ScriptFieldType::Char},
        {"System.Byte",    ScriptFieldType::Byte},
        {"System.Double",  ScriptFieldType::Double},
        {"System.Int16",   ScriptFieldType::Short},
        {"System.Int32",   ScriptFieldType::Int},
        {"System.Int64",   ScriptFieldType::Long},
        {"System.UInt16",  ScriptFieldType::UShort},
        {"System.UInt32",  ScriptFieldType::UInt},
        {"System.UInt64",  ScriptFieldType::ULong},
        {"System.UInt",    ScriptFieldType::UByte},
        {"Ignite.Vector2", ScriptFieldType::Vector2},
        {"Ignite.Vector3", ScriptFieldType::Vector3},
        {"Ignite.Vector4", ScriptFieldType::Vector4},
		{"Ignite.Entity",  ScriptFieldType::Entity},
    };

    struct ScriptEngineData
    {
        std::unique_ptr<ScriptHost> scriptHost;

        std::vector<std::string> entityScriptStorage;

        std::filesystem::path mochiSharpAssemblyFilepath;
        std::filesystem::path coreAssemblyFilepath;
        std::filesystem::path appAssemblyFilepath;

        std::unique_ptr<filewatch::FileWatch<std::string>> appAssemblyFileWatcher;
        bool assemblyReloadingPending = false;
        bool assemblyReloadDeferred = false;
        std::filesystem::file_time_type appAssemblyLastWriteTime{};
        bool hasAppAssemblyLastWriteTime = false;

        std::unordered_map<std::string, Ref<ScriptClass>> entityClasses;
        std::unordered_map<ScriptInstanceID, Ref<ScriptInstance>> entityScriptInstances;
    };

    ScriptEngineData *scriptEngineData = nullptr;
    ScriptEngine *scriptEngine = nullptr;

    void ScriptEngine::InitHostFxr()
    {
        if (!scriptEngineData->scriptHost)
        {
            scriptEngineData->scriptHost = std::make_unique<ScriptHost>();
        }

        // Find the runtimeconfig.json for MochiSharp.Managed
        const std::filesystem::path configPath = m_Project->GetDirectory() / "Bin/MochiSharp.Managed.runtimeconfig.json";
        
        if (!std::filesystem::exists(configPath))
        {
            LOG_ERROR("[Script Engine] HostFXR config not found: {}", configPath.generic_string());
            return;
        }

        if (!scriptEngineData->scriptHost->Init(configPath))
        {
            LOG_ERROR("[Script Engine] Failed to initialize HostFXR");
            return;
        }

        LOG_WARN("[Script Engine] HostFXR Initialized");
    }

    void ScriptEngine::ShutdownHostFxr()
    {
        if (!scriptEngineData)
        {
            return;
        }

        scriptEngineData->scriptHost.reset();

        LOG_WARN("[Script Engine] HostFXR Shutdown");
    }

    ScriptEngine::ScriptEngine(Project *project)
        : m_Project(project)
    {
        scriptEngine = this;

        const auto appAssemblyPath = m_Project->GetScriptModulePath();

        if (scriptEngineData)
        {
            scriptEngineData->appAssemblyFilepath = appAssemblyPath.generic_string();
            ReloadAssembly();
            return;
        }

        scriptEngineData = new ScriptEngineData();

        InitHostFxr();

        // Load MochiSharp.Managed core
        const std::filesystem::path mochiSharpPath = m_Project->GetScriptBinDirectory() / "MochiSharp.Managed.dll";
        if (!std::filesystem::exists(mochiSharpPath))
        {
            LOG_ERROR("[Script Engine] MochiSharp.Managed.dll not found!");
            return;
        }

        scriptEngineData->mochiSharpAssemblyFilepath = mochiSharpPath;
        if (!scriptEngineData->scriptHost->LoadAssembly(mochiSharpPath))
        {
            LOG_ERROR("[Script Engine] Failed to load MochiSharp.Managed.dll");
            return;
        }
        LOG_INFO("[Script Engine] Loaded MochiSharp.Managed.dll");

        // Script Core Assembly (IgniteScriptEngine.dll)
        const std::filesystem::path coreAssemblyPath = m_Project->GetScriptBinDirectory() / "IgniteScriptEngine.dll";
        LOG_ASSERT(std::filesystem::exists(coreAssemblyPath), "[Script Engine] Script core assembly not found!");
        LoadCoreAssembly(coreAssemblyPath);

        // Register method signatures AFTER assemblies are loaded
        scriptEngineData->scriptHost->RegisterSignatures();
        LOG_INFO("[Script Engine] Registered method signatures");

        LoadAppAssembly(appAssemblyPath);
        LoadAppAssemblyClasses();

        // storing classes name into storage
        for (auto &it : scriptEngineData->entityClasses)
        {
            LOG_INFO("Script '{}' loaded", it.first);
            scriptEngineData->entityScriptStorage.emplace_back(it.first);
        }

        LOG_WARN("[Script Engine] Initialized");
    }

    ScriptEngine::~ScriptEngine()
    {
        ShutdownHostFxr();

        if (!scriptEngineData)
        {
            return;
        }

        scriptEngineData->entityClasses.clear();
        scriptEngineData->entityScriptInstances.clear();

        delete scriptEngineData;
        scriptEngineData = nullptr;

        LOG_WARN("[Script Engine] Shutdown");
    }

    void ScriptEngine::RegisterCoreClassesAndFunctions()
    {
        // Register glue functions and components via HostFXR
        ScriptGlue::RegisterFunctions();
        ScriptGlue::RegisterComponents();
    }

    bool ScriptEngine::LoadCoreAssembly(const std::filesystem::path &filepath)
    {
        scriptEngineData->coreAssemblyFilepath = filepath;

        if (!scriptEngineData->scriptHost->LoadAssembly(filepath))
        {
            LOG_ERROR("[Script Engine] Failed to load core assembly: {}", filepath.generic_string());
            return false;
        }

        // Register glue functions and components
        RegisterCoreClassesAndFunctions();

        LOG_INFO("[Script Engine] Core assembly loaded: {}", filepath.generic_string());
        return true;
    }

    void ScriptEngine::OnAppAssemblyFileSystemEvent(const std::string &path, const filewatch::Event eventType)
    {
        if (!scriptEngineData->assemblyReloadingPending && eventType == filewatch::Event::modified)
        {
            scriptEngineData->assemblyReloadingPending = true;

            Application::SubmitToMainThread([&]()
            {
                if (scriptEngine->m_Scene && scriptEngine->m_Scene->IsRunning())
                {
                    scriptEngineData->assemblyReloadDeferred = true;
                    scriptEngineData->assemblyReloadingPending = false;
                    LOG_INFO("[Script Engine] App assembly change detected during play. Reload deferred until scene stops.");
                    return;
                }
                
                scriptEngineData->appAssemblyFileWatcher.reset();
                scriptEngine->ReloadAssembly();
                scriptEngineData->assemblyReloadingPending = false;
            });
        }
    }

    bool ScriptEngine::LoadAppAssembly(const std::filesystem::path &filepath)
    {
        if (!exists(filepath))
        {
            if (!m_Project->BuildSolution())
            {
                return false;
            }
        }

        scriptEngineData->appAssemblyFilepath = filepath;

        if (scriptEngineData->hasAppAssemblyLastWriteTime)
        {
            if (!WaitForAssemblyNewerThan(filepath, scriptEngineData->appAssemblyLastWriteTime))
            {
                LOG_WARN("[Script Engine] App assembly timestamp did not advance before reload: {}", filepath.generic_string());
            }
        }

        if (!WaitForAssemblyFileReady(filepath))
        {
            LOG_WARN("[Script Engine] App assembly may still be updating: {}", filepath.generic_string());
        }
        
        if (!scriptEngineData->scriptHost->LoadAssembly(filepath))
        {
            LOG_ERROR("[Script Engine] Failed to load app assembly: {}", filepath.generic_string());
            return false;
        }

        if (!scriptEngineData->scriptHost->ConfigureSerialization(kSerializeFieldAttributeTypeName, kEntityTypeName))
        {
            LOG_ERROR("[Script Engine] Failed to configure script serialization type names");
            return false;
        }

        if (!scriptEngineData->scriptHost->InitializeInternalCalls())
        {
            LOG_ERROR("[Script Engine] Failed to initialize internal calls bridge");
            return false;
        }

        scriptEngineData->appAssemblyFileWatcher = CreateScope<filewatch::FileWatch<std::string>>(filepath.string(), ScriptEngine::OnAppAssemblyFileSystemEvent);
        scriptEngineData->assemblyReloadingPending = false;

        std::filesystem::file_time_type currentWriteTime{};
        if (TryGetAssemblyWriteTime(filepath, currentWriteTime))
        {
            scriptEngineData->appAssemblyLastWriteTime = currentWriteTime;
            scriptEngineData->hasAppAssemblyLastWriteTime = true;
        }

        LOG_INFO("[Script Engine] App assembly loaded: {}", filepath.generic_string());
        return true;
    }

    void ScriptEngine::ReloadAssembly()
    {
        // Clear existing instances
        scriptEngineData->entityScriptInstances.clear();

        // Reload app assembly (MochiSharp handles unloading through collectible context)
        if (LoadAppAssembly(scriptEngineData->appAssemblyFilepath))
        {
            LoadAppAssemblyClasses();

            // storing classes name into storage
            scriptEngineData->entityScriptStorage.clear();

            for (auto &it : scriptEngineData->entityClasses)
            {
                scriptEngineData->entityScriptStorage.emplace_back(it.first);
            }

            LOG_INFO("[Script Engine] App Assembly Reloaded '{}'", scriptEngineData->appAssemblyFilepath.generic_string());
        }
    }

    void ScriptEngine::SetSceneContext(Scene *scene)
    {
        m_Scene = scene;
    }

    void ScriptEngine::ClearSceneContext()
    {
        if (!scriptEngineData)
            return;

        for (auto &instance : scriptEngineData->entityScriptInstances)
        {
			scriptEngine->GetScriptHost()->DestroyInstance(instance.second->GetInstanceID());
        }

        scriptEngineData->entityScriptInstances.clear();
        if (scriptEngineData->assemblyReloadDeferred)
        {
            scriptEngineData->assemblyReloadDeferred = false;
            scriptEngineData->appAssemblyFileWatcher.reset();
            ReloadAssembly();
        }

		m_Scene = nullptr;
    }

    bool ScriptEngine::IsEntityClassExists(const std::string &fullClassName)
    {
        if (scriptEngineData)
            return scriptEngineData->entityClasses.contains(fullClassName);
        return false;
    }

    Ref<ScriptInstance> ScriptEngine::OnCreateEntityInstance(ScriptInstanceID instanceID, const std::string &className)
    {
        IGN_PROFILE_FUNCTION();

        if (IsEntityClassExists(className))
        {
            auto scriptInstance = CreateRef<ScriptInstance>(scriptEngineData->entityClasses[className], instanceID);
            scriptEngineData->entityScriptInstances[instanceID] = scriptInstance;

            // C# On Create Function
            scriptInstance->InvokeOnCreate();
            return scriptInstance;
        }

        return nullptr;
    }

	void ScriptEngine::OnDestroyEntityInstance(ScriptInstanceID instanceID)
	{
        IGN_PROFILE_FUNCTION();

        auto &scriptInstance = scriptEngineData->entityScriptInstances[instanceID];
        if (scriptInstance)
            scriptInstance->InvokeOnDestroy();

        scriptEngineData->entityScriptInstances.erase(instanceID);
	}

    Ref<ScriptClass> ScriptEngine::GetEntityClassesByName(const std::string &name)
    {
        if (!scriptEngineData)
            return nullptr;

        if (!scriptEngineData->entityClasses.contains(name))
            return nullptr;

        return scriptEngineData->entityClasses.at(name);
    }

    std::unordered_map<std::string, Ref<ScriptClass>> ScriptEngine::GetEntityClasses()
    {
        return scriptEngineData->entityClasses;
    }

    std::vector<std::string> ScriptEngine::GetScriptClassStorage()
    {
        return scriptEngineData->entityScriptStorage;
    }

    Ref<ScriptInstance> ScriptEngine::GetEntityScriptInstance(ScriptInstanceID instanceID)
    {
        const auto &it = scriptEngineData->entityScriptInstances.find(instanceID);
        if (it == scriptEngineData->entityScriptInstances.end())
        {
            LOG_ERROR("[Script Engine] Failed to find {}", instanceID);
            return nullptr;
        }

        return it->second;
    }

    Scene *ScriptEngine::GetSceneContext()
    {
        return m_Scene;
    }

    ScriptHost *ScriptEngine::GetScriptHost()
    {
        return scriptEngineData ? scriptEngineData->scriptHost.get() : nullptr;
    }

    ScriptEngine *ScriptEngine::GetInstance()
    {
        return scriptEngine;
    }

    void ScriptEngine::LoadAppAssemblyClasses()
    {
        auto previousEntityClasses = std::move(scriptEngineData->entityClasses);
        scriptEngineData->entityClasses.clear();

        const std::string appAssemblyName = scriptEngineData->appAssemblyFilepath.stem().string();
        std::string derivedTypes = scriptEngineData->scriptHost->GetDerivedTypes(scriptEngineData->appAssemblyFilepath, "Ignite.Entity");
        
        if (derivedTypes.empty())
        {
            LOG_WARN("[Script Engine] No derived script classes found in {}", scriptEngineData->appAssemblyFilepath.generic_string());
            return;
        }

        // Get classes names
        size_t start = 0;
        while (start <= derivedTypes.size())
        {
            const size_t end = derivedTypes.find('|', start);
            const std::string fullName = (end == std::string::npos) ? derivedTypes.substr(start) : derivedTypes.substr(start, end - start);
            if (!fullName.empty())
            {
                const size_t lastDot = fullName.find_last_of('.');
                const std::string classNamespace = (lastDot == std::string::npos) ? "" : fullName.substr(0, lastDot);
                const std::string className = (lastDot == std::string::npos) ? fullName : fullName.substr(lastDot + 1);

                const auto scriptClass = CreateRef<ScriptClass>(classNamespace, className, appAssemblyName);

                const std::string fieldMetadata = scriptEngineData->scriptHost->GetTypeFields(fullName);
                size_t fieldStart = 0;
                while (fieldStart <= fieldMetadata.size())
                {
                    const size_t fieldEnd = fieldMetadata.find('|', fieldStart);
                    const std::string fieldEntry = (fieldEnd == std::string::npos) ? fieldMetadata.substr(fieldStart) : fieldMetadata.substr(fieldStart, fieldEnd - fieldStart);

                    if (!fieldEntry.empty())
                    {
                        const size_t sep0 = fieldEntry.find('~');
                        const size_t sep1 = (sep0 == std::string::npos) ? std::string::npos : fieldEntry.find('~', sep0 + 1);
                        const size_t sep2 = (sep1 == std::string::npos) ? std::string::npos : fieldEntry.find('~', sep1 + 1);

                        if (sep0 != std::string::npos && sep1 != std::string::npos && sep2 != std::string::npos)
                        {
                            const std::string fieldName = fieldEntry.substr(0, sep0);
                            const std::string managedTypeName = fieldEntry.substr(sep0 + 1, sep1 - sep0 - 1);
                            const bool isPublic = fieldEntry.substr(sep1 + 1, sep2 - sep1 - 1) == "1";
                            const bool hasSerializeField = fieldEntry.substr(sep2 + 1) == "1";

                            ScriptField field;
                            field.Name = fieldName;
                            field.ManagedTypeName = managedTypeName;
                            field.IsPublic = isPublic;
                            field.HasSerializeFieldAttribute = hasSerializeField;

                            const auto fieldTypeIt = s_ScriptFieldTypeMap.find(managedTypeName);
                            if (fieldTypeIt != s_ScriptFieldTypeMap.end())
                            {
                                field.Type = fieldTypeIt->second;
                                scriptClass->InsertField(fieldName, field);
                            }
                            else
                            {
                                LOG_WARN("[Script Engine] Unsupported script field type '{}.{}' ({})", fullName, fieldName, managedTypeName);
                            }
                        }
                    }

                    if (fieldEnd == std::string::npos)
                    {
                        break;
                    }

                    fieldStart = fieldEnd + 1;
                }

                auto previousClassIt = previousEntityClasses.find(fullName);
                if (previousClassIt != previousEntityClasses.end() && previousClassIt->second)
                {
                    auto &previousInstances = previousClassIt->second->GetInstancesFields();
                    auto &currentInstances = scriptClass->GetInstancesFields();

                    for (auto &[instanceId, previousFields] : previousInstances)
                    {
                        auto &currentFields = currentInstances[instanceId];
                        for (auto &[fieldName, fieldDef] : scriptClass->GetFields())
                        {
                            auto previousFieldIt = previousFields.find(fieldName);
                            if (previousFieldIt != previousFields.end() && previousFieldIt->second.field.Type == fieldDef.Type)
                            {
                                ScriptInstanceField instanceField = previousFieldIt->second;
                                instanceField.field = fieldDef;
                                currentFields[fieldName] = instanceField;
                            }
                        }
                    }
                }

                scriptEngineData->entityClasses[fullName] = scriptClass;
            }

            if (end == std::string::npos)
            {
                break;
            }
            start = end + 1;
        }

        LOG_INFO("[Script Engine] Loaded {} script classes", scriptEngineData->entityClasses.size());
    }
}
