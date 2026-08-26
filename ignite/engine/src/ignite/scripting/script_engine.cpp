// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "script_engine.hpp"
#include "glue/component_script_glue.hpp"
#include "script_class.hpp"
#include "script_host.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/project/project.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/string_utils.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/core/signals/signals.hpp"

#include <utility>

namespace ignite
{
    namespace
    {
        // Constants
        constexpr const char *kIgniteObjectName = "Ignite.IgniteObject";
        constexpr const char *kSerializeFieldTypeName = "Ignite.SerializeField";
        constexpr const char *kScriptableObjectTypeName = "Ignite.ScriptableObject";
        constexpr const char *kUISliderTypeName = "Ignite.UISlider";
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

        std::filesystem::path mochiSharpAssemblyFilepath;
        std::filesystem::path coreAssemblyFilepath;

        SignalToken solutionBuildToken;

        Scope<filewatch::FileWatch<std::string>> appAssemblyFileWatcher;
        std::chrono::time_point<std::chrono::file_clock> appAssemblyLastWriteTime{};
        ProjectConfiguration currentProjectConfig;
        bool assemblyReloadingPending = false;
        bool assemblyReloadDeferred = false;
        bool hotReloadPending = false;
        bool hasAppAssemblyLastWriteTime = false;

        bool coreAssemblyLoaded = false;
        bool appAssemblyLoaded = false;

        // Entity script
        ScriptClassMap entityClasses;
        std::unordered_map<ScriptInstanceID, Ref<ScriptInstance>> entityScriptInstances;
        std::vector<std::string> entityScriptClassStorage;

        // Scriptable object script
        ScriptClassMap scriptableObjectClasses;
        std::vector<std::string> scriptableObjecClassStorage;
        std::vector<ScriptableObjectMenuEntry> scriptableObjectMenuEntries;

        // Hot-reload: captured List<Entity> / List<Asset> field IDs
        // Key: instanceId -> (fieldName -> pipe-separated entity IDs string)
        std::unordered_map<ScriptInstanceID, std::unordered_map<std::string, std::string>> capturedEntityListFields;

        Project *project = nullptr;
        Scene *scene = nullptr;
    };

    static ScriptEngineData *scriptEngineData = nullptr;
    static ScriptEngine *scriptEngine = nullptr;

    ScriptEngine::ScriptEngine(Project *project)
    {
        scriptEngine = this;

        if (scriptEngineData)
        {
            scriptEngineData->project = project;
            scriptEngineData->scene = nullptr;

            ReloadAssembly();
            return;
        }

        scriptEngineData = new ScriptEngineData();

        scriptEngineData->project = project;
        scriptEngineData->scene = nullptr;

        InitHostFxr();

        scriptEngineData->mochiSharpAssemblyFilepath = scriptEngineData->project->GetScriptBinDirectory() / "MochiSharp.Managed.dll";
        scriptEngineData->currentProjectConfig = scriptEngineData->project->GetConfiguration();

        // Script Core Assembly (Ignite.ScriptEngine.dll)
        scriptEngineData->coreAssemblyFilepath = scriptEngineData->project->GetScriptBinDirectory() / "Ignite.ScriptEngine.dll";
        LOG_ASSERT(std::filesystem::exists(scriptEngineData->coreAssemblyFilepath), "[Script Engine] Script core assembly not found!");

        // Register method signatures AFTER Core Assembly is loaded
        scriptEngineData->scriptHost->RegisterSignatures();
        LOG_INFO("[Script Engine] Registered method signatures");

        // Build solution if the App Assembly is not available yet
        EnsureAppAssembly(true);
    }

    ScriptEngine::~ScriptEngine()
    {
        scriptEngine = nullptr;
        ShutdownHostFxr();

        if (!scriptEngineData)
        {
            return;
        }

        if (scriptEngineData->solutionBuildToken != kInvalidSignalToken)
        {
            SignalBus::Unsubscribe<SuccessResultSignal>(scriptEngineData->solutionBuildToken);
            scriptEngineData->solutionBuildToken = kInvalidSignalToken;
        }

        scriptEngineData->entityClasses.clear();
        scriptEngineData->entityScriptInstances.clear();

        delete scriptEngineData;
        scriptEngineData = nullptr;

        LOG_WARN("[Script Engine] Shutdown");
    }

    bool ScriptEngine::LoadCoreAssembly(const std::filesystem::path &filepath)
    {
        LOG_ASSERT(!filepath.empty(), "[Script Engine] Core Assembly should not empty!");

        // Skip reload
        if (scriptEngineData->coreAssemblyLoaded)
            return true;

        scriptEngineData->coreAssemblyLoaded = scriptEngineData->scriptHost->LoadAssembly(filepath);

        if (scriptEngineData->coreAssemblyLoaded)
        {
            // Register glue functions and components via HostFXR
            ComponentScriptGlue::RegisterFunctions();
            ComponentScriptGlue::RegisterComponents();
            LOG_WARN("[Script Engine] Core assembly loaded: {}", filepath.generic_string());
        }

        LOG_ASSERT(scriptEngineData->coreAssemblyLoaded, "[Script Engine] Failed to load Core Assembly \"Ignite.ScriptEngine.dll\" on path: \"{}\"", filepath.string());
        return scriptEngineData->coreAssemblyLoaded;
    }

    bool ScriptEngine::LoadAppAssembly(const std::filesystem::path &filepath)
    {
        // NOTE: Always makesure Core assembly is loaded
        LoadCoreAssembly(scriptEngineData->coreAssemblyFilepath);

        LOG_ASSERT(!filepath.empty(), "[Script Engine] App Assembly should not empty!");

        if (scriptEngineData->hasAppAssemblyLastWriteTime && scriptEngineData->currentProjectConfig == scriptEngineData->project->GetConfiguration())
        {
            if (!vfs::WaitForFileNewerThan(filepath, scriptEngineData->appAssemblyLastWriteTime))
            {
                // The DLL timestamp never advanced past the previously-loaded version.
                // Loading now would give us the same (or a partial) binary — bail out.
                LOG_WARN("[Script Engine] App assembly timestamp did not advance; skipping reload to avoid loading a stale binary: {}", filepath.generic_string());
                return false;
            }
        }

        if (!vfs::WaitForFileReady(filepath))
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
        scriptEngineData->appAssemblyFileWatcher = vfs::WatchFile(filepath.string(), ScriptEngine::OnAppAssemblyFileSystemEvent);
        scriptEngineData->assemblyReloadingPending = false;

        std::chrono::time_point<std::chrono::file_clock> currentWriteTime{};
        if (vfs::TryGetFileWriteTime(filepath, currentWriteTime))
        {
            scriptEngineData->appAssemblyLastWriteTime = currentWriteTime;
            scriptEngineData->hasAppAssemblyLastWriteTime = true;
        }

        LOG_INFO("[Script Engine] App assembly loaded: {}", filepath.generic_string());

        // Load the classes
        LoadAppAssemblyClasses();

        // Refresh any already-loaded ScriptableObject managed instances in the new ALC
        RefreshScriptableObjectInstances();

        return true;
    }

    bool ScriptEngine::ReloadAssembly()
    {
        scriptEngineData->appAssemblyLoaded = false;

        // CRITICAL: Destroy all script instances BEFORE reloading the assembly
        // This prevents TargetException due to managed objects holding old type references
        if (!scriptEngineData->entityScriptInstances.empty())
        {
            // Destroy all instances first (calls OnDestroy on each)
            for (const auto& instanceId : scriptEngineData->entityScriptInstances | std::views::keys)
            {
                if (scriptEngineData->scriptHost)
                {
                    scriptEngineData->scriptHost->DestroyInstance(instanceId);
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
        scriptEngineData->currentProjectConfig = scriptEngineData->project->GetConfiguration();

        return true;
    }

    bool ScriptEngine::IsHotReloadPending()
    {
        return scriptEngineData ? scriptEngineData->hotReloadPending : false;
    }

    void ScriptEngine::HotReloadAssembly()
    {
        if (!scriptEngineData || !scriptEngineData->scene)
            return;

        LOG_INFO("[Script Engine] Hot reloading app assembly in play mode...");

        // 1. Capture current C# managed field values to C++ ScriptInstanceFields
        CaptureAllInstanceFieldValues();

        // 2. Snapshot current active script components in the scene
        struct ScriptEntitySnapshot
        {
            ScriptInstanceID instanceId;
            entt::entity entity;
            std::string className;
        };

        std::vector<ScriptEntitySnapshot> scriptSnapshots;

        if (scriptEngineData->scene && scriptEngineData->scene->registry)
        {
            scriptEngineData->scene->registry->view<ScriptComponent>().each([&scriptSnapshots](entt::entity enttEntity, ScriptComponent &script)
            {
                if (script.runtimeScriptInstance)
                {
                    Entity entity{ enttEntity, scriptEngineData->scene };
                    scriptSnapshots.push_back({ entity.GetUUID(), enttEntity, script.className });
                    script.runtimeScriptInstance = nullptr;
                }
            });
        }

        // 3. Clear entity script instances map before reloading assembly
        scriptEngineData->entityScriptInstances.clear();
        // (capturedEntityListFields is preserved across the reload and consumed in step 5)

        // 4. Reset filewatcher write time guard and reload assembly
        scriptEngineData->hasAppAssemblyLastWriteTime = false;
        scriptEngineData->appAssemblyLastWriteTime = {};
        scriptEngineData->appAssemblyFileWatcher.reset();

        if (!ReloadAssembly())
        {
            LOG_ERROR("[Script Engine] Hot reload failed during assembly reload!");
            scriptEngineData->hotReloadPending = false;
            return;
        }

        // 5. Re-create script instances for entities without calling OnCreate
        for (const auto & [instanceId, entity, className] : scriptSnapshots)
        {
            if (scriptEngineData->scene->registry->valid(entity))
            {
                auto &script = scriptEngineData->scene->registry->get<ScriptComponent>(entity);
                script.runtimeScriptInstance = OnCreateEntityInstance(instanceId, className, /*invokeOnCreate=*/false);

                if (script.runtimeScriptInstance)
                {
                    auto scriptClass = script.runtimeScriptInstance->GetScriptClass();
                    if (auto *instanceFields = scriptClass->GetInstanceFieldsById(instanceId))
                    {
                        for (auto &[fieldName, instanceField] : *instanceFields)
                        {
                            // Restore primitive / value-type [SerializeField] fields into
                            // the freshly-created managed instance.  Entity and Asset fields
                            // hold GC handles that were invalidated when the old ALC was
                            // unloaded, so we skip them here — the script must re-acquire
                            // those references in its OnHotReload() override.
                            switch (instanceField.field.Type)
                            {
                                case ScriptFieldType::Bool:    { auto v = instanceField.GetValue<bool>();     scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::Byte:    { auto v = instanceField.GetValue<uint8_t>();  scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::SByte:   { auto v = instanceField.GetValue<int8_t>();   scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::Char:    { auto v = instanceField.GetValue<char16_t>(); scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::Short:   { auto v = instanceField.GetValue<int16_t>();  scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::UShort:  { auto v = instanceField.GetValue<uint16_t>(); scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::Int:     { auto v = instanceField.GetValue<int32_t>();  scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::UInt:    { auto v = instanceField.GetValue<uint32_t>(); scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::Long:    { auto v = instanceField.GetValue<int64_t>();  scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::ULong:   { auto v = instanceField.GetValue<uint64_t>(); scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::Float:   { auto v = instanceField.GetValue<float>();    scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::Double:  { auto v = instanceField.GetValue<double>();   scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::Vector2: { auto v = instanceField.GetValue<glm::vec2>();scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::Vector3: { auto v = instanceField.GetValue<glm::vec3>();scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::Vector4: { auto v = instanceField.GetValue<glm::vec4>();scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::Color:   { auto v = instanceField.GetValue<glm::vec4>();scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::Quat:    { auto v = instanceField.GetValue<glm::quat>();scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::Enum:    { auto v = instanceField.GetValue<int32_t>();  scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
                                case ScriptFieldType::String:
                                {
                                    auto str = instanceField.GetValue<std::string>();
                                    scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, str.data(), static_cast<int>(str.size()));
                                    break;
                                }
                                // Entity and Asset fields are stored as uint64_t IDs in the C++ buffer
                                // (entity UUID or asset handle) — not raw GC handles — so they are safe
                                // to restore across a hot reload via the ID-based SetInstanceFieldValue path.
                                case ScriptFieldType::Entity:
                                {
                                    auto id = instanceField.GetValue<uint64_t>();
                                    if (id != 0)
                                        scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &id, sizeof(id));
                                    break;
                                }
                                case ScriptFieldType::Asset:
                                {
                                    auto id = instanceField.GetValue<uint64_t>();
                                    if (id != 0)
                                    {
                                        // Re-create the managed ScriptableObject sub-instance in the new ALC
                                        // so MochiSharp can resolve the reference via its _instances lookup.
                                        Scene *scene = scriptEngineData->scene;
                                        if (AssetManager *am = scene ? scene->GetAssetManager() : nullptr; am && am->IsAssetHandleValid(AssetHandle(id)))
                                        {
                                            auto so = am->GetAssetImmediate<ScriptableObject>(AssetHandle(id));
                                            if (so)
                                            {
                                                if (!scriptEngineData->scriptHost->CreateInstance(id, so->GetClassName()))
                                                {
                                                    LOG_ERROR("[Script Engine] HotReload: failed to recreate SO managed instance '{}' (handle={})", so->GetClassName(), id);
                                                }
                                                else
                                                {
                                                    ScriptInstance::PopulateSOFields(scriptEngineData->scriptHost.get(), id, *so);
                                                }
                                            }
                                        }
                                        scriptEngineData->scriptHost->SetInstanceFieldValue(instanceId, fieldName, &id, sizeof(id));
                                    }
                                    break;
                                }
                                default:
                                    std::unreachable();
                            }
                        }
                    }

                    // 6. Restore List<Entity> / List<Asset> fields captured before the ALC unload.
                    // The managed bridge reconstructs each list from the pipe-separated entity IDs.
                    auto capturedListIt = scriptEngineData->capturedEntityListFields.find(instanceId);
                    if (capturedListIt != scriptEngineData->capturedEntityListFields.end())
                    {
                        for (auto &[fieldName, ids] : capturedListIt->second)
                        {
                            if (!ids.empty())
                                scriptEngineData->scriptHost->SetEntityListField(instanceId, fieldName, ids);
                        }
                    }

                    // 7. Invoke OnHotReload() callback
                    script.runtimeScriptInstance->InvokeOnHotReload();
                }
            }
        }

        scriptEngineData->capturedEntityListFields.clear();
        scriptEngineData->hotReloadPending = false;
        LOG_INFO("[Script Engine] App assembly hot reload complete! {} script instances updated.", scriptSnapshots.size());
    }

    void ScriptEngine::SetSceneContext(Scene *scene)
    {
        scriptEngineData->scene = scene;
    }

    void ScriptEngine::ClearSceneContext()
    {
        if (!scriptEngineData)
            return;

        for (const auto& val : scriptEngineData->entityScriptInstances | std::views::values)
        {
            scriptEngineData->scriptHost->DestroyInstance(val->GetInstanceId());
        }

        scriptEngineData->entityScriptInstances.clear();

        if (scriptEngineData->assemblyReloadDeferred)
        {
            scriptEngineData->assemblyReloadDeferred = false;
            scriptEngineData->appAssemblyFileWatcher.reset();
            ReloadAssembly();
        }

        scriptEngineData->scene = nullptr;
    }

    bool ScriptEngine::IsEntityClassExists(const std::string &fullClassName)
    {
        if (scriptEngineData)
            return scriptEngineData->entityClasses.contains(fullClassName);
        return false;
    }

    Ref<ScriptInstance> ScriptEngine::OnCreateEntityInstance(ScriptInstanceID instanceId, const std::string &className, bool invokeOnCreate)
    {
        IGN_PROFILE_FUNCTION();

        if (IsEntityClassExists(className))
        {
            auto scriptInstance = CreateRef<ScriptInstance>(scriptEngineData->entityClasses[className], instanceId);
            scriptEngineData->entityScriptInstances[instanceId] = scriptInstance;

            if (invokeOnCreate)
                scriptInstance->InvokeOnCreate();
            return scriptInstance;
        }

        return nullptr;
    }

    void ScriptEngine::OnDestroyEntityInstance(const ScriptInstanceID instanceId)
    {
        IGN_PROFILE_FUNCTION();

        if (const auto &scriptInstance = scriptEngineData->entityScriptInstances[instanceId])
            scriptInstance->InvokeOnDestroy();

        if (scriptEngineData->scriptHost)
        {
            scriptEngineData->scriptHost->DestroyInstance(instanceId);
        }

        scriptEngineData->entityScriptInstances.erase(instanceId);
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

    Ref<ScriptInstance> ScriptEngine::GetEntityScriptInstance(ScriptInstanceID instanceId)
    {
        const auto &it = scriptEngineData->entityScriptInstances.find(instanceId);
        if (it == scriptEngineData->entityScriptInstances.end())
        {
            LOG_ERROR("[Script Engine] Failed to find {}", instanceId);
            return nullptr;
        }

        return it->second;
    }

    const std::vector<std::string> &ScriptEngine::GetEntityScriptClassStorage()
    {
        return scriptEngineData->entityScriptClassStorage;
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

        const std::string rawData = scriptEngineData->scriptHost->GetCreateAssetMenuData(scriptEngineData->project->GetScriptModulePath(), kScriptableObjectTypeName);

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

    bool ScriptEngine::IsReady()
    {
        if (!scriptEngineData)
            return false;

        return scriptEngineData->appAssemblyLoaded && scriptEngineData->coreAssemblyLoaded;
    }

    Scene *ScriptEngine::GetSceneContext()
    {
        return scriptEngineData->scene;
    }

    ScriptHost *ScriptEngine::GetScriptHost()
    {
        return scriptEngineData ? scriptEngineData->scriptHost.get() : nullptr;
    }

    ScriptEngine *ScriptEngine::GetInstance()
    {
        return scriptEngine;
    }

    FileStatus ScriptEngine::EnsureAppAssembly(bool waitForBuild)
    {
        LOG_ASSERT(!scriptEngineData->project->GetScriptModulePath().empty(), "[Script Engine] App Assembly should not empty!");

        // For initial project load/create, force asynchronous build first so app assembly
        // is loaded only after dependencies and build outputs are updated.
        if (!waitForBuild)
        {
            auto modulePath = scriptEngineData->project->GetScriptModulePath();
            if (std::filesystem::exists(modulePath))
            {
                // Clean up any leftover slow-path subscription from a previous call
                SignalBus::Unsubscribe<SuccessResultSignal>(scriptEngineData->solutionBuildToken);
                scriptEngineData->solutionBuildToken = kInvalidSignalToken;

                // Load App Assembly immediately (we may be on a worker thread)
                scriptEngineData->appAssemblyLoaded = LoadAppAssembly(modulePath);
                const bool ready = scriptEngineData->appAssemblyLoaded;
                Application::SubmitToMainThread([ready]()
                {
                    SignalBus::Emit(SuccessResultSignal{ ready, SignalType::Project });
                });
                return FileStatus::Success;
            }
        }

        // Build path (forced at project open/create, or when DLL does not exist):
        // Deregister any leftover subscription.
        SignalBus::Unsubscribe<SuccessResultSignal>(scriptEngineData->solutionBuildToken);
        scriptEngineData->solutionBuildToken = kInvalidSignalToken;

        // Register Build Solution callback — one-shot, only fires on ScriptEngine signal
        scriptEngineData->solutionBuildToken = SignalBus::Subscribe<SuccessResultSignal>([](const SuccessResultSignal &signal)
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
                scriptEngineData->appAssemblyLoaded = LoadAppAssembly(scriptEngineData->project->GetScriptModulePath());
            }

            // We are already on the main thread (called from project.cpp's SubmitToMainThread),
            // so emit Project signal directly — no need for another SubmitToMainThread.
            SignalBus::Emit(SuccessResultSignal{ signal.isSuccess && scriptEngineData->appAssemblyLoaded, SignalType::Project });
        });

        // Run the build and load the App Assembly if success
        LOG_DEBUG("Building Visual Studio Solution...");
        scriptEngineData->project->BuildSolution(true);
        return FileStatus::Pending;
    }

    void ScriptEngine::InitHostFxr()
    {
        if (!scriptEngineData->scriptHost)
        {
            scriptEngineData->scriptHost = CreateScope<ScriptHost>();
        }

        // Find the runtimeconfig.json for MochiSharp.Managed
        const std::filesystem::path configPath = scriptEngineData->project->GetDirectory() / "Bin/MochiSharp.Managed.runtimeconfig.json";

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

    void ScriptEngine::OnAppAssemblyFileSystemEvent(const std::string &path, const filewatch::Event eventType)
    {
        if (!scriptEngineData->assemblyReloadingPending && eventType == filewatch::Event::modified)
        {
            scriptEngineData->assemblyReloadingPending = true;

            Application::SubmitToMainThread([]()
            {
                if (scriptEngineData->scene && scriptEngineData->scene->IsRunning())
                {
                    scriptEngineData->hotReloadPending = true;
                    scriptEngineData->assemblyReloadingPending = false;
                    LOG_INFO("[Script Engine] App assembly change detected during play. Hot reload scheduled for frame end.");
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

        const std::string appAssemblyName = scriptEngineData->project->GetScriptModulePath().stem().string();
        std::string derivedTypes = scriptEngineData->scriptHost->GetDerivedTypes(scriptEngineData->project->GetScriptModulePath(), classFullName);
        if (derivedTypes.empty())
        {
            LOG_WARN("[Script Engine] No derived script classes found in {} for '{}'",
                scriptEngineData->project->GetScriptModulePath().generic_string(), classFullName);

            return;
        }

        // Get classes names
        size_t start = 0;
        while (start <= derivedTypes.size())
        {
            const size_t end = derivedTypes.find('|', start);
            if (const std::string fullName = (end == std::string::npos) ? derivedTypes.substr(start) : derivedTypes.substr(start, end - start); !fullName.empty())
            {
                const size_t lastDot = fullName.find_last_of('.');
                const std::string classNamespace = (lastDot == std::string::npos) ? "" : fullName.substr(0, lastDot);
                const std::string className = (lastDot == std::string::npos) ? fullName : fullName.substr(lastDot + 1);

                // Create the script class
                const Ref<ScriptClass> scriptClass = CreateRef<ScriptClass>(classNamespace, className, appAssemblyName);

                // UI attributes for this class
                std::unordered_map<std::string, std::pair<float, float>> uiSliderMap;
                std::string uiAttributes = scriptEngineData->scriptHost->GetFieldUIAttribute(fullName, kUISliderTypeName);
                if (!uiAttributes.empty())
                {
                    auto uiEntries = stringutils::SplitString(uiAttributes, '|');
                    for (const auto &uiEntry : uiEntries)
                    {
                        if (uiEntry.empty())
                            continue;
                        auto parts = stringutils::SplitString(uiEntry, '~');
                        if (parts.size() >= 6)
                        {
                            const std::string &fName = parts[0];
                            float minVal = std::stof(parts[4]);
                            float maxVal = std::stof(parts[5]);
                            uiSliderMap[fName] = { minVal, maxVal };
                        }
                    }
                }

                // Get field typename
                const std::string fieldMetadata = scriptEngineData->scriptHost->GetTypeFields(fullName);
                for (auto fieldEntries = stringutils::SplitString(fieldMetadata, '|'); const auto &fieldEntry : fieldEntries)
                {
                    if (fieldEntry.empty())
                        continue;

                    if (auto parts = stringutils::SplitString(fieldEntry, '~'); parts.size() >= 4)
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
                                for (auto valStrings = stringutils::SplitString(parts[6], ','); const auto &vs : valStrings)
                                {
                                    field.EnumValues.push_back(std::stoi(vs));
                                }
                            }
                        }
                        else
                        {
                            if (const auto fieldTypeIt = s_ScriptFieldTypeMap.find(managedTypeName); fieldTypeIt != s_ScriptFieldTypeMap.end())
                            {
                                field.Type = fieldTypeIt->second;
                            }
                            else if (scriptEngineData->scriptableObjectClasses.contains(managedTypeName))
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
                                if (const auto listTypeIt = s_ScriptFieldTypeMap.find(managedTypeName); listTypeIt != s_ScriptFieldTypeMap.end())
                                {
                                    field.Type = listTypeIt->second;
                                }
                                else if (scriptEngineData->scriptableObjectClasses.contains(elementType))
                                {
                                    // List of a specific ScriptableObject subclass
                                    field.Type = ScriptFieldType::List_Asset;
                                }
                                else if (scriptEngineData->entityClasses.contains(elementType))
                                {
                                    // List of a custom entity script class (treat as List<Entity>)
                                    field.Type = ScriptFieldType::List_Entity;
                                }
                            }
                        }

                        // Apply UI attributes strictly for single numeric field types
                        auto sliderIt = uiSliderMap.find(fieldName);
                        if (sliderIt != uiSliderMap.end())
                        {
                            if (IsSingleNumericFieldType(field.Type))
                            {
                                field.uiType = FieldUIType::Slider;
                                field.minValue = sliderIt->second.first;
                                field.maxValue = sliderIt->second.second;
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

                // Capture C# initial default values for all fields in scriptClass
                uint64_t tempId = 0xFFFFFFFFFFFFFFFFULL;
                if (scriptEngineData->scriptHost && scriptEngineData->scriptHost->CreateInstance(tempId, fullName))
                {
                    auto &defaultFields = scriptClass->GetDefaultFields();
                    for (const auto &[fieldName, fieldDef] : scriptClass->GetFields())
                    {
                        ScriptInstanceField defaultField;
                        defaultField.field = fieldDef;
                        char buffer[64] = { 0 };
                        if (scriptEngineData->scriptHost->GetInstanceFieldValue(tempId, fieldName, buffer, sizeof(buffer)))
                        {
                            defaultField.SetValueRaw(buffer, sizeof(buffer));
                        }
                        defaultFields[fieldName] = defaultField;
                    }
                    scriptEngineData->scriptHost->DestroyInstance(tempId);
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

    void ScriptEngine::RefreshScriptableObjectInstances()
    {
        if (!scriptEngineData || !scriptEngineData->scriptHost)
            return;

        AssetManager *am = AssetManager::GetInstance();
        if (!am)
            return;

        for (auto &[handle, asset] : am->GetAssetAssetRegistry())
        {
            if (asset.type != AssetType::ScriptableObject)
                continue;

            auto so = am->GetAssetImmediate<ScriptableObject>(handle);
            if (!so || !IsScriptableObjectClassExists(so->GetClassName()))
                continue;

            // Ensure a fresh managed instance exists for this SO in the new ALC
            const uint64_t id = static_cast<uint64_t>(handle);
            if (!scriptEngineData->scriptHost->CreateInstance(id, so->GetClassName()))
            {
                LOG_WARN("[Script Engine] Failed to recreate managed SO instance '{}' (handle={})",
                         so->GetClassName(), id);
                continue;
            }

            // Re-push the stored C++ fields into the new managed instance
            ScriptInstance::PopulateSOFields(scriptEngineData->scriptHost.get(), id, *so);

            // Synchronize any new script fields from C# into the C++ ScriptableObject
            Ref<ScriptClass> scriptClass = GetScriptableObjectClassByName(so->GetClassName());
            if (scriptClass)
            {
                auto &soFields = so->GetFields();
                for (const auto &[fieldName, fieldDef] : scriptClass->GetFields())
                {
                    if (!soFields.contains(fieldName))
                    {
                        ScriptInstanceField newField;
                        newField.field = fieldDef;
                        char buffer[64] = { 0 };
                        if (scriptEngineData->scriptHost->GetInstanceFieldValue(id, fieldName, buffer, sizeof(buffer)))
                        {
                            newField.SetValueRaw(buffer, sizeof(buffer));
                        }
                        soFields[fieldName] = newField;
                    }
                }
            }

            LOG_TRACE("[Script Engine] Refreshed ScriptableObject '{}' (handle={})", so->GetClassName(), id);
        }
    }

    void ScriptEngine::CaptureAllInstanceFieldValues()
    {
        if (!scriptEngineData || !scriptEngineData->scriptHost)
            return;

        char buffer[64];
        for (auto &[instanceId, scriptInstance] : scriptEngineData->entityScriptInstances)
        {
            if (!scriptInstance)
                continue;

            auto scriptClass = scriptInstance->GetScriptClass();
            if (!scriptClass)
                continue;

            auto *instanceFields = scriptClass->GetInstanceFieldsById(instanceId);
            if (!instanceFields)
                continue;

            for (auto &[fieldName, instanceField] : *instanceFields)
            {
                if (instanceField.field.Type == ScriptFieldType::Invalid)
                    continue;

                // Entity and Asset fields are reference types: GetInstanceFieldValue always
                // returns 0 for them (MochiSharp can't marshal GC references into a raw buffer).
                // Their uint64_t IDs are already correctly stored in the C++ buffer from when
                // the field was first assigned, so skip the capture to avoid overwriting them.
                if (instanceField.field.Type == ScriptFieldType::Entity ||
                    instanceField.field.Type == ScriptFieldType::Asset)
                    continue;

                // List<Entity> and List<Asset>: read the entity IDs via the managed reflection
                // bridge before the ALC is torn down, so we can reconstruct the list after reload.
                if (instanceField.field.Type == ScriptFieldType::List_Entity ||
                    instanceField.field.Type == ScriptFieldType::List_Asset)
                {
                    std::string ids = scriptEngineData->scriptHost->GetEntityListFieldIds(instanceId, fieldName);
                    LOG_ASSERT(!ids.empty(), "[Script Engine] IDs is empty! This can be a bug!");
                    scriptEngineData->capturedEntityListFields[instanceId][fieldName] = std::move(ids);
                    continue;
                }

                memset(buffer, 0, sizeof(buffer));
                if (scriptEngineData->scriptHost->GetInstanceFieldValue(instanceId, fieldName, buffer, sizeof(buffer)))
                {
                    instanceField.SetValueRaw(buffer, sizeof(buffer));
                }
            }
        }
    }
}
