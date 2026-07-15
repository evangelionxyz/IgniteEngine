// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "script_engine.hpp"
#include "glue/component_script_glue.hpp"
#include "glue/core_script_glue.hpp"
#include "script_class.hpp"
#include "script_host.hpp"


#include "ignite/scene/component.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/project/project.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/string_utils.hpp"
#include "ignite/core/platform_utils.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/core/signals/signals.hpp"

namespace ignite
{
    namespace
    {
        // Constants
        constexpr const char *kIgniteObjectName = "Ignite.IgniteObject";
        constexpr const char *kSerializeFieldTypeName = "Ignite.SerializeField";
        constexpr const char *kScriptableObjectTypeName = "Ignite.ScriptableObject";
        constexpr const char *kEntityTypeName = "Ignite.Entity";
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

        // Normalized List<T> keys emitted by ScriptReflectionBridge
        {"List<System.Boolean>",          ScriptFieldType::List_Bool},
        {"List<System.Char>",             ScriptFieldType::List_Char},
        {"List<System.String>",           ScriptFieldType::List_String},
        {"List<System.Byte>",             ScriptFieldType::List_Byte},
        {"List<System.SByte>",            ScriptFieldType::List_SByte},
        {"List<System.Int16>",            ScriptFieldType::List_Short},
        {"List<System.UInt16>",           ScriptFieldType::List_UShort},
        {"List<System.Int32>",            ScriptFieldType::List_Int},
        {"List<System.UInt32>",           ScriptFieldType::List_UInt},
        {"List<System.Int64>",            ScriptFieldType::List_Long},
        {"List<System.UInt64>",           ScriptFieldType::List_ULong},
        {"List<System.Single>",           ScriptFieldType::List_Float},
        {"List<System.Double>",           ScriptFieldType::List_Double},
        {"List<Ignite.Mathf+Vector2>",    ScriptFieldType::List_Vector2},
        {"List<Ignite.Mathf+Vector3>",    ScriptFieldType::List_Vector3},
        {"List<Ignite.Mathf+Vector4>",    ScriptFieldType::List_Vector4},
        {"List<Ignite.Mathf+Quaternion>", ScriptFieldType::List_Quat},
        {"List<Ignite.Mathf+Color>",      ScriptFieldType::List_Color},
        {"List<Ignite.Entity>",           ScriptFieldType::List_Entity},
        {"List<Ignite.ScriptableObject>", ScriptFieldType::List_Asset},
        {"List<Ignite.AssetHandle>",      ScriptFieldType::List_Asset},
        {"List<Ignite.Asset>",            ScriptFieldType::List_Asset},
    };

    struct ScriptEngineData
    {
        Scope<ScriptHost> scriptHost;

        ignite::Path mochiSharpAssemblyFilepath;
        ignite::Path coreAssemblyFilepath;

        SignalToken solutionBuildToken;

        Scope<filewatch::FileWatch<std::string>> appAssemblyFileWatcher;
        std::chrono::time_point<std::chrono::file_clock> appAssemblyLastWriteTime{};
        ProjectConfiguration currentProjectConfig;
        bool isReady = false;
        bool assemblyReloadingPending = false;
        bool assemblyReloadDeferred = false;
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

    FileStatus ScriptEngine::EnsureAppAssembly()
    {
        LOG_ASSERT(!m_Project->GetScriptModulePath().empty(), "[Script Engine] App Assembly should not empty!");

        // Load immediately if the App Assembly (.dll) exists.
        // This ensures the editor loads the default scene successfully on startup
        // if a valid DLL is already present, without waiting for/blocking on a rebuild
 
        auto modulePath = m_Project->GetScriptModulePath();
        if (ignite::Path::exists(modulePath))
        {
            // Clean up any leftover slow-path subscription from a previous call
            SignalBus::Unsubscribe<SuccessResultSignal>(scriptEngineData->solutionBuildToken);
            scriptEngineData->solutionBuildToken = kInvalidSignalToken;

            // Load App Assembly immediately (we may be on a worker thread)
            scriptEngineData->isReady = LoadAppAssembly(modulePath);
            const bool ready = scriptEngineData->isReady;
            Application::SubmitToMainThread([ready]()
                {
                    SignalBus::Emit(SuccessResultSignal{ ready, SignalType::Project });
                });
            return FileStatus::Success;
        }

        // Slow path: DLL does not exist yet. Deregister any leftover subscription.
        SignalBus::Unsubscribe<SuccessResultSignal>(scriptEngineData->solutionBuildToken);
        scriptEngineData->solutionBuildToken = kInvalidSignalToken;

        // Register Build Solution callback — one-shot, only fires on ScriptEngine signal
        scriptEngineData->solutionBuildToken = SignalBus::Subscribe<SuccessResultSignal>([this](const SuccessResultSignal &signal)
        {
            // Guard: only handle the "build finished" notification, not any re-emitted Project signals
            if (signal.type != SignalType::ScriptEngine)
                return;

            // One-shot: unsubscribe immediately so cascading Project emits don't re-trigger this
            SignalBus::Unsubscribe<SuccessResultSignal>(scriptEngineData->solutionBuildToken);
            scriptEngineData->solutionBuildToken = kInvalidSignalToken;

            LOG_ASSERT(signal.isSuccess, "[Script Engine] Failed to build solution!");
            if (signal.isSuccess)
            {
                scriptEngineData->isReady = LoadAppAssembly(m_Project->GetScriptModulePath());
            }

            // We are already on the main thread (called from project.cpp's SubmitToMainThread),
            // so emit Project signal directly — no need for another SubmitToMainThread.
            SignalBus::Emit(SuccessResultSignal{ signal.isSuccess && scriptEngineData->isReady, SignalType::Project });
        });
        
        // Run the build and load the App Assembly if success
        LOG_DEBUG("Building Visual Studio Solution...");
        m_Project->BuildSolution(true);
        return FileStatus::Pending;
    }

    void ScriptEngine::InitHostFxr()
    {
        if (!scriptEngineData->scriptHost)
        {
            scriptEngineData->scriptHost = CreateScope<ScriptHost>();
        }

        // Find the runtimeconfig.json for MochiSharp.Managed
        const ignite::Path configPath = m_Project->GetDirectory() / "Bin/MochiSharp.Managed.runtimeconfig.json";

        if (!std::filesystem::exists(configPath.string()))
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
        : m_Project(project), m_Scene(nullptr)
    {
        scriptEngine = this;


        if (scriptEngineData)
        {
            ReloadAssembly();
            return;
        }

        scriptEngineData = new ScriptEngineData();

        InitHostFxr();

        scriptEngineData->mochiSharpAssemblyFilepath = m_Project->GetScriptBinDirectory() / "MochiSharp.Managed.dll";

        // Script Core Assembly (Ignite.ScriptEngine.dll)
        scriptEngineData->coreAssemblyFilepath = m_Project->GetScriptBinDirectory() / "Ignite.ScriptEngine.dll";
        LOG_ASSERT(ignite::Path::exists(scriptEngineData->coreAssemblyFilepath), "[Script Engine] Script core assembly not found!");
        if (!LoadCoreAssembly(scriptEngineData->coreAssemblyFilepath))
        {
            LOG_ASSERT(false, "[Script Engine] Failed to reload core assembly '{}'", scriptEngineData->coreAssemblyFilepath.generic_string());
            return;
        }

        // Register method signatures AFTER Core Assembly is loaded
        scriptEngineData->scriptHost->RegisterSignatures();
        LOG_INFO("[Script Engine] Registered method signatures");

        // Build solution if the App Assembly is not available yet
        EnsureAppAssembly();
		scriptEngineData->currentProjectConfig = m_Project->GetConfiguration();
    }

    ScriptEngine::~ScriptEngine()
    {
        scriptEngine = nullptr;
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

    bool ScriptEngine::LoadCoreAssembly(const ignite::Path &filepath)
    {
        LOG_ASSERT(!filepath.empty(), "[Script Engine] Core Assembly should not empty!");

        if (!scriptEngineData->scriptHost->LoadAssembly(filepath))
            return false;

        // Register glue functions and components via HostFXR
        ComponentScriptGlue::RegisterFunctions();
        ComponentScriptGlue::RegisterComponents();

        LOG_WARN("[Script Engine] Core assembly loaded: {}", filepath.generic_string());
        return true;
    }

    void ScriptEngine::OnAppAssemblyFileSystemEvent(const std::string &path, const filewatch::Event eventType)
    {
        if (!scriptEngineData->assemblyReloadingPending && eventType == filewatch::Event::modified)
        {
            scriptEngineData->assemblyReloadingPending = true;

            Application::SubmitToMainThread([]()
            {
                if (scriptEngine->m_Scene && scriptEngine->m_Scene->IsRunning())
                {
                    scriptEngineData->assemblyReloadDeferred = true;
                    scriptEngineData->assemblyReloadingPending = false;
                    LOG_INFO("[Script Engine] App assembly change detected during play. Reload deferred until scene stops.");
                    return;
                }

                // Reset the last-write-time guard BEFORE destroying the watcher.
                // This ensures that LoadAppAssembly's WaitForFileNewerThan check uses
                // a zeroed baseline and waits for the truly new timestamp rather than
                // accepting a partial/in-progress write that already bumped the timestamp.
                scriptEngineData->hasAppAssemblyLastWriteTime = false;
                scriptEngineData->appAssemblyLastWriteTime = {};

                scriptEngineData->appAssemblyFileWatcher.reset();
                scriptEngine->ReloadAssembly();
                scriptEngineData->assemblyReloadingPending = false;
            });
        }
    }

    bool ScriptEngine::LoadAppAssembly(const ignite::Path &filepath)
    {
        LOG_ASSERT(!filepath.empty(), "[Script Engine] App Assembly should not empty!");

        if (scriptEngineData->hasAppAssemblyLastWriteTime && scriptEngineData->currentProjectConfig == m_Project->GetConfiguration())
        {
            if (!ignite::Path::WaitForFileNewerThan(filepath, scriptEngineData->appAssemblyLastWriteTime))
            {
                // The DLL timestamp never advanced past the previously-loaded version.
                // Loading now would give us the same (or a partial) binary — bail out.
                LOG_WARN("[Script Engine] App assembly timestamp did not advance; skipping reload to avoid loading a stale binary: {}", filepath.generic_string());
                return false;
            }
        }

        if (!ignite::Path::WaitForFileReady(filepath))
        {
            LOG_WARN("[Script Engine] App assembly may still be updating: {}", filepath.generic_string());
            return false;
        }

        if (!scriptEngineData->scriptHost->LoadAssembly(filepath))
        {
            LOG_ASSERT(false, "[Script Engine] Failed to load App Assembly: {}", filepath.generic_string());
            return false;
        }

        // Configure field serialization
        if (!scriptEngineData->scriptHost->ConfigureSerialization(kSerializeFieldTypeName, kEntityTypeName))
        {
            LOG_ASSERT(false, "[Script Engine] Failed to configure Entity script serialization type names");
            return false;
        }

        if (!scriptEngineData->scriptHost->ConfigureSerialization(kSerializeFieldTypeName, kScriptableObjectTypeName))
        {
            LOG_ASSERT(false, "[Script Engine] Failed to configure Scriptable Object serialization type names");
            return false;
        }

        // Initialize CORE & COMPONENT Internal Calls
        if (!scriptEngineData->scriptHost->InitializeCoreInternalCalls())
        {
            LOG_ASSERT(false, "[Script Engine] Failed to initialize CORE internal calls bridge");
            return false;
        }

        if (!scriptEngineData->scriptHost->InitializeComponentInternalCalls())
        {
            LOG_ASSERT(false, "[Script Engine] Failed to initialize COMPONENT internal calls bridge");
            return false;
        }

        // Create App Assembly File-watcher
        LOG_WARN("[Script Engine] Watching App Assembly '{}'", filepath.string());
        scriptEngineData->appAssemblyFileWatcher = ignite::Path::WatchFile(filepath.string(), ScriptEngine::OnAppAssemblyFileSystemEvent);
        scriptEngineData->assemblyReloadingPending = false;

        std::chrono::time_point<std::chrono::file_clock> currentWriteTime {};
        if (ignite::Path::TryGetFileWriteTime(filepath, currentWriteTime))
        {
            scriptEngineData->appAssemblyLastWriteTime = currentWriteTime;
            scriptEngineData->hasAppAssemblyLastWriteTime = true;
        }

        LOG_INFO("[Script Engine] App assembly loaded: {}", filepath.generic_string());

        // Load the classes
        LoadAppAssemblyClasses();

        return true;
    }

    bool ScriptEngine::ReloadAssembly()
    {
        scriptEngineData->isReady = false;

        // CRITICAL: Destroy all script instances BEFORE reloading the assembly
        // This prevents TargetException due to managed objects holding old type references
        if (!scriptEngineData->entityScriptInstances.empty())
        {
            // Destroy all instances first (calls OnDestroy on each)
            for (auto &[instanceID, instance] : scriptEngineData->entityScriptInstances)
            {
                if (scriptEngineData->scriptHost)
                {
                    scriptEngineData->scriptHost->DestroyInstance(instanceID);
                }
            }
            // Clear the instances map
            scriptEngineData->entityScriptInstances.clear();
        }

        if (!scriptEngineData->scriptHost || !scriptEngineData->scriptHost->ResetLoadContext())
        {
            LOG_ASSERT(false, "[Script Engine] Failed to reset script host load context during reload");
            return false;
        }

        if (!LoadCoreAssembly(scriptEngineData->coreAssemblyFilepath))
        {
            LOG_ASSERT(false, "[Script Engine] Failed to reload core assembly '{}'", scriptEngineData->coreAssemblyFilepath.generic_string());
            return false;
        }

        // Register method signatures AFTER Core Assembly is loaded
        scriptEngineData->scriptHost->RegisterSignatures();
        LOG_TRACE("[Script Engine] Registered method signatures");

        // Re-initialize the native bridge into the freshly loaded core assembly.
        // The new ALC gives CoreInternalCalls/ComponentInternalCalls clean static
        // fields, so they must be re-populated before any managed code runs.
        if (!scriptEngineData->scriptHost->InitializeCoreInternalCalls())
        {
            LOG_ASSERT(false, "[Script Engine] Failed to re-initialize CORE internal calls bridge after reload");
            return false;
        }

        if (!scriptEngineData->scriptHost->InitializeComponentInternalCalls())
        {
            LOG_ASSERT(false, "[Script Engine] Failed to re-initialize COMPONENT internal calls bridge after reload");
            return false;
        }

        // Reload app assembly (MochiSharp handles unloading through collectible context)
        EnsureAppAssembly();
		scriptEngineData->currentProjectConfig = m_Project->GetConfiguration();

        return true;
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
            scriptEngineData->scriptHost->DestroyInstance(instance.second->GetInstanceID());
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

        if (scriptEngineData->scriptHost)
        {
            scriptEngineData->scriptHost->DestroyInstance(instanceID);
        }

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

        const std::string rawData = scriptEngineData->scriptHost->GetCreateAssetMenuData(m_Project->GetScriptModulePath(), kScriptableObjectTypeName);

        if (rawData.empty())
            return;

        // Format: "FullClassName~FileName~MenuName|..."
        size_t start = 0;
        while (start <= rawData.size())
        {
            const size_t end = rawData.find('|', start);
            const std::string entry = (end == std::string::npos) ? rawData.substr(start) : rawData.substr(start, end - start);

            if (!entry.empty())
            {
                const size_t sep1 = entry.find('~');
                const size_t sep2 = (sep1 != std::string::npos) ? entry.find('~', sep1 + 1) : std::string::npos;

                if (sep1 != std::string::npos && sep2 != std::string::npos)
                {
                    ScriptableObjectMenuEntry menuEntry;
                    menuEntry.className = entry.substr(0, sep1);
                    menuEntry.fileName = entry.substr(sep1 + 1, sep2 - sep1 - 1);
                    menuEntry.menuName = entry.substr(sep2 + 1);
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

    bool ScriptEngine::IsReady() const
    {
        if (!scriptEngineData)
            return false;

        return scriptEngineData->isReady;
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

        const std::string appAssemblyName = m_Project->GetScriptModulePath().stem().string();
        std::string derivedTypes = scriptEngineData->scriptHost->GetDerivedTypes(m_Project->GetScriptModulePath(), classFullName);

        if (derivedTypes.empty())
        {
            LOG_WARN("[Script Engine] No derived script classes found in {} for '{}'", m_Project->GetScriptModulePath().generic_string(), classFullName);
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
                            else if (managedTypeName.rfind("List<", 0) == 0 &&
                                     managedTypeName.size() > 5 &&
                                     managedTypeName.back() == '>')
                            {
                                // Normalized "List<ElementTypeName>" emitted by ScriptReflectionBridge
                                const std::string elementType = managedTypeName.substr(5, managedTypeName.size() - 6);
                                field.ListElementTypeName = elementType;

                                // Try direct map lookup (covers all primitive + engine types)
                                const auto listTypeIt = s_ScriptFieldTypeMap.find(managedTypeName);
                                if (listTypeIt != s_ScriptFieldTypeMap.end())
                                {
                                    field.Type = listTypeIt->second;
                                }
                                else if (scriptEngineData->scriptableObjectClasses.count(elementType))
                                {
                                    // List of a specific ScriptableObject subclass
                                    field.Type = ScriptFieldType::List_Asset;
                                }
                                else if (scriptEngineData->entityClasses.count(elementType))
                                {
                                    // List of a custom entity script class (treat as List<Entity>)
                                    field.Type = ScriptFieldType::List_Entity;
                                }
                            }
                        }

                        // Only register the field if it resolved to a known type.
                        // Unknown types (e.g. engine component wrappers not in the map) are silently
                        // skipped — the C# side already filtered out private/non-serializable fields.
                        if (field.Type != ScriptFieldType::Invalid)
                        {
                            scriptClass->InsertField(fieldName, field);
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
