// Copyright (c) 2026 Evangelion Manuhutu

#include "script_engine.hpp"
#include "glue/component_script_glue.hpp"
#include "glue/core_script_glue.hpp"
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
#include <ranges>

namespace ignite
{
    namespace
    {
        // Constants
        constexpr const char *kSerializeFieldTypeName = "Ignite.SerializeField";
        constexpr const char *kScriptableObjectTypeName = "Ignite.ScriptableObject";

        constexpr const char *kEntityTypeName = "Ignite.Entity";

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
        {"System.String",        ScriptFieldType::String},
        {"System.Boolean",       ScriptFieldType::Bool},
        {"System.Single",        ScriptFieldType::Float},
        {"System.Char",          ScriptFieldType::Char},
        {"System.Byte",          ScriptFieldType::Byte},
        {"System.Double",        ScriptFieldType::Double},
        {"System.Int16",         ScriptFieldType::Short},
        {"System.Int32",         ScriptFieldType::Int},
        {"System.Int64",         ScriptFieldType::Long},
        {"System.UInt16",        ScriptFieldType::UShort},
        {"System.UInt32",        ScriptFieldType::UInt},
        {"System.UInt64",        ScriptFieldType::ULong},
        {"System.SByte",         ScriptFieldType::SByte},
        {"Ignite.Mathf+Vector2", ScriptFieldType::Vector2},
        {"Ignite.Mathf+Vector3", ScriptFieldType::Vector3},
        {"Ignite.Mathf+Vector4", ScriptFieldType::Vector4},
        {"Ignite.Mathf+Quaternion", ScriptFieldType::Quat},
        {"Ignite.Mathf+Color",   ScriptFieldType::Color},
        {"Ignite.Entity",        ScriptFieldType::Entity},
        
        {"Ignite.ScriptableObject", ScriptFieldType::Asset},
        {"Ignite.AssetHandle", ScriptFieldType::Asset},
        {"Ignite.Asset", ScriptFieldType::Asset},
    };

    struct ScriptEngineData
    {
        std::unique_ptr<ScriptHost> scriptHost;

        std::filesystem::path mochiSharpAssemblyFilepath;
        std::filesystem::path coreAssemblyFilepath;
        std::filesystem::path appAssemblyFilepath;

        std::unique_ptr<filewatch::FileWatch<std::string>> appAssemblyFileWatcher;
        bool assemblyReloadingPending = false;
        bool assemblyReloadDeferred = false;
        std::filesystem::file_time_type appAssemblyLastWriteTime{};
        bool hasAppAssemblyLastWriteTime = false;

        // Entity script
        ScriptClassMap entityClasses;
        std::unordered_map<ScriptInstanceID, Ref<ScriptInstance>> entityScriptInstances;
        std::vector<std::string> entityScriptClassStorage;

        // Scriptable object script
        ScriptClassMap scriptableObjectClasses;
        std::vector<std::string> scriptableObjecClassStorage;
        std::vector<ScriptableObjectMenuEntry> scriptableObjectMenuEntries;
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
        ComponentScriptGlue::RegisterFunctions();
        ComponentScriptGlue::RegisterComponents();
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

        // Configure field serialization
        if (!scriptEngineData->scriptHost->ConfigureSerialization(kSerializeFieldTypeName, kEntityTypeName))
        {
            LOG_ERROR("[Script Engine] Failed to configure Entity script serialization type names");
            return false;
        }

        if (!scriptEngineData->scriptHost->ConfigureSerialization(kSerializeFieldTypeName, kScriptableObjectTypeName))
        {
            LOG_ERROR("[Script Engine] Failed to configure Scriptable Object serialization type names");
            return false;
        }

        // Initialize CORE & COMPONENT Internal Calls
        if (!scriptEngineData->scriptHost->InitializeCoreInternalCalls())
        {
            LOG_ERROR("[Script Engine] Failed to initialize CORE internal calls bridge");
            return false;
        }

        if (!scriptEngineData->scriptHost->InitializeComponentInternalCalls())
        {
            LOG_ERROR("[Script Engine] Failed to initialize COMPONENT internal calls bridge");
            return false;
        }

        scriptEngineData->appAssemblyFileWatcher = CreateScope<filewatch::FileWatch<std::string>>(filepath.string(), ScriptEngine::OnAppAssemblyFileSystemEvent);
        scriptEngineData->assemblyReloadingPending = false;

        std::filesystem::file_time_type currentWriteTime {};
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

    Ref<ScriptClass> ScriptEngine::GetEntityClassByName(const std::string &name)
    {
        if (!scriptEngineData->entityClasses.contains(name))
            return nullptr;

        return scriptEngineData->entityClasses.at(name);
    }

    const ScriptClassMap &ScriptEngine::GetEntityClasses()
    {
        return scriptEngineData->entityClasses;
    }

    const std::vector<std::string> &ScriptEngine::GetEntityScriptClassStorage()
    {
        return scriptEngineData->entityScriptClassStorage;
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

    bool ScriptEngine::IsScriptableObjectClassExists(const std::string &fullClassName)
    {
        return scriptEngineData->scriptableObjectClasses.contains(fullClassName);
    }

    Ref<ScriptClass> ScriptEngine::GetScriptableObjectClassByName(const std::string &name)
    {
        if (!scriptEngineData->scriptableObjectClasses.contains(name))
            return nullptr;
        return scriptEngineData->scriptableObjectClasses.at(name);
    }

    const ScriptClassMap &ScriptEngine::GetScriptableObjectClasses()
    {
        return scriptEngineData->scriptableObjectClasses;
    }

    const std::vector<std::string> &ScriptEngine::GetScriptableObjectClassStorage()
    {
        return scriptEngineData->scriptableObjecClassStorage;
    }

    const std::vector<ScriptableObjectMenuEntry> &ScriptEngine::GetScriptableObjectMenuEntries()
    {
        return scriptEngineData->scriptableObjectMenuEntries;
    }

    void ScriptEngine::RefreshScriptableObjectMenuEntries()
    {
        scriptEngineData->scriptableObjectMenuEntries.clear();
        if (!scriptEngineData || !scriptEngineData->scriptHost)
            return;

        const std::string rawData = scriptEngineData->scriptHost->GetCreateAssetMenuData(
            scriptEngineData->appAssemblyFilepath,
            kScriptableObjectTypeName);

        if (rawData.empty())
            return;

        // Format: "FullClassName~FileName~MenuName|..."
        size_t start = 0;
        while (start <= rawData.size())
        {
            const size_t end = rawData.find('|', start);
            const std::string entry = (end == std::string::npos)
                ? rawData.substr(start)
                : rawData.substr(start, end - start);

            if (!entry.empty())
            {
                const size_t sep1 = entry.find('~');
                const size_t sep2 = (sep1 != std::string::npos) ? entry.find('~', sep1 + 1) : std::string::npos;

                if (sep1 != std::string::npos && sep2 != std::string::npos)
                {
                    ScriptableObjectMenuEntry menuEntry;
                    menuEntry.className = entry.substr(0, sep1);
                    menuEntry.fileName  = entry.substr(sep1 + 1, sep2 - sep1 - 1);
                    menuEntry.menuName  = entry.substr(sep2 + 1);
                    scriptEngineData->scriptableObjectMenuEntries.push_back(menuEntry);
                    LOG_TRACE("[Script Engine] CreateAssetMenu: class='{}' file='{}' menu='{}'",
                              menuEntry.className, menuEntry.fileName, menuEntry.menuName);
                }
            }

            if (end == std::string::npos) break;
            start = end + 1;
        }

        LOG_INFO("[Script Engine] Found {} CreateAssetMenu entries", scriptEngineData->scriptableObjectMenuEntries.size());
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
        // Load ScriptableObject classes FIRST so entity field parsing can reference them
        LoadAppClasses(kScriptableObjectTypeName, scriptEngineData->scriptableObjectClasses);
        LoadAppClasses(kEntityTypeName, scriptEngineData->entityClasses);

        // Store class name to class storage
        // Entity class storage
        scriptEngineData->entityScriptClassStorage.clear();
        for (const auto &className : scriptEngineData->entityClasses | std::views::keys)
        {
            LOG_TRACE("Entity script '{}' loaded", className);
            scriptEngineData->entityScriptClassStorage.push_back(className);
        }

        // Scriptable object
        scriptEngineData->scriptableObjecClassStorage.clear();
        for (const auto &className : scriptEngineData->scriptableObjectClasses | std::views::keys)
        {
            LOG_TRACE("Scriptable object '{}' loaded", className);
            scriptEngineData->scriptableObjecClassStorage.push_back(className);
        }

        // Refresh CreateAssetMenu entries (used by content browser context menu)
        RefreshScriptableObjectMenuEntries();
    }

    void ScriptEngine::LoadAppClasses(const std::string &classFullName, ScriptClassMap &outClasses)
    {
        // Copy the classes first to get previous field data
        ScriptClassMap prevClasses = outClasses;

        // Clear
        outClasses.clear();

        const std::string appAssemblyName = scriptEngineData->appAssemblyFilepath.stem().string();
        std::string derivedTypes = scriptEngineData->scriptHost->GetDerivedTypes(scriptEngineData->appAssemblyFilepath, classFullName);

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

                // Create the script class
                const Ref<ScriptClass> scriptClass = CreateRef<ScriptClass>(classNamespace, className, appAssemblyName);

                // Get field typename
                const std::string fieldMetadata = scriptEngineData->scriptHost->GetTypeFields(fullName);
                size_t fieldStart = 0;
                auto fieldEntries = stringutils::SplitString(fieldMetadata, '|');
                for (const auto &fieldEntry : fieldEntries)
                {
                    if (fieldEntry.empty()) continue;

                    auto parts = stringutils::SplitString(fieldEntry, '~');
                    if (parts.size() >= 4)
                    {
                        const std::string &fieldName = parts[0];
                        const std::string &managedTypeName = parts[1];
                        bool isPublic = parts[2] == "1";
                        bool hasSerializeField = parts[3] == "1";

                        ScriptField field;
                        field.Name = fieldName;
                        field.ManagedTypeName = managedTypeName;
                        field.IsPublic = isPublic;
                        field.HasSerializeFieldAttribute = hasSerializeField;

                        if (parts.size() >= 5 && parts[4] == "1")
                        {
                            field.IsEnum = true;
                            field.Type = ScriptFieldType::Enum;
                            if (parts.size() >= 7)
                            {
                                field.EnumNames = stringutils::SplitString(parts[5], ',');
                                auto valStrings = stringutils::SplitString(parts[6], ',');
                                for (const auto &vs : valStrings)
                                {
                                    try
                                    {
                                        field.EnumValues.push_back(std::stoi(vs));
                                    }
                                    catch (...) { }
                                }
                            }
                        }
                        else
                        {
                            const auto fieldTypeIt = s_ScriptFieldTypeMap.find(managedTypeName);
                            if (fieldTypeIt != s_ScriptFieldTypeMap.end())
                            {
                                field.Type = fieldTypeIt->second;
                            }
                            else if (scriptEngineData->scriptableObjectClasses.count(managedTypeName))
                            {
                                // Field is a specific ScriptableObject subclass (same assembly)
                                field.Type = ScriptFieldType::Asset;
                            }
                        }

                        if (field.Type != ScriptFieldType::Invalid)
                        {
                            scriptClass->InsertField(fieldName, field);
                        }
                        else
                        {
                            LOG_ERROR("[Script Engine] Unsupported script field type '{}.{}' ({})", fullName, fieldName, managedTypeName);
                        }
                    }
                }

                // Iterate previous class and get the field data
                auto previousClassIt = prevClasses.find(fullName);
                if (previousClassIt != prevClasses.end() && previousClassIt->second)
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

                // Store the class
                outClasses[fullName] = scriptClass;
            }

            if (end == std::string::npos)
            {
                break;
            }
            start = end + 1;
        }

        LOG_INFO("[Script Engine] Loaded {} script classes", outClasses.size());
    }
}
