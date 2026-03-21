/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "script_engine.hpp"
#include "script_glue.hpp"
#include "script_class.hpp"
#include "script_host.hpp"

#include "ignite/scene/component.hpp"
#include "ignite/project/project.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/string_utils.hpp"
#include "ignite/core/platform_utils.hpp"

#include <cstdlib>
#include <format>
#include <fstream>
#include <thread>
#include <chrono>

namespace ignite
{
    namespace
    {
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
    }

    static std::unordered_map<std::string, ScriptFieldType> s_ScriptFieldTypeMap =
    {
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
        {"IgniteScriptEngine.Vector2", ScriptFieldType::Vector2},
        {"IgniteScriptEngine.Vector3", ScriptFieldType::Vector3},
        {"IgniteScriptEngine.Vector4", ScriptFieldType::Vector4},
        {"IgniteScriptEngine.Entity",  ScriptFieldType::Entity},
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

        std::unordered_map<std::string, Ref<ScriptClass>> entityClasses;
        std::unordered_map<UUID, Ref<ScriptInstance>> entityInstances;
        std::unordered_map<UUID, ScriptFieldMap> entityScriptFields;
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
        scriptEngineData->entityInstances.clear();

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
                    return;
                
                scriptEngineData->appAssemblyFileWatcher.reset();
                scriptEngine->ReloadAssembly();
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

        if (!WaitForAssemblyFileReady(filepath))
        {
            LOG_WARN("[Script Engine] App assembly may still be updating: {}", filepath.generic_string());
        }
        
        if (!scriptEngineData->scriptHost->LoadAssembly(filepath))
        {
            LOG_ERROR("[Script Engine] Failed to load app assembly: {}", filepath.generic_string());
            return false;
        }

        if (!scriptEngineData->scriptHost->InitializeInternalCalls())
        {
            LOG_ERROR("[Script Engine] Failed to initialize internal calls bridge");
            return false;
        }

        scriptEngineData->appAssemblyFileWatcher = CreateScope<filewatch::FileWatch<std::string>>(filepath.string(), ScriptEngine::OnAppAssemblyFileSystemEvent);
        scriptEngineData->assemblyReloadingPending = false;

        LOG_INFO("[Script Engine] App assembly loaded: {}", filepath.generic_string());
        return true;
    }

    void ScriptEngine::ReloadAssembly()
    {
        // Clear existing instances
        scriptEngineData->entityInstances.clear();

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
        m_Scene = nullptr;
        scriptEngineData->entityInstances.clear();
    }

    bool ScriptEngine::EntityClassExists(const std::string &fullClassName)
    {
        if (scriptEngineData)
            return scriptEngineData->entityClasses.contains(fullClassName);
        return false;
    }

    void ScriptEngine::OnCreateEntity(Entity entity)
    {
        if (const auto &sc = entity.GetComponent<ScriptComponent>(); EntityClassExists(sc.className))
        {
            const UUID uuid = entity.GetUUID();

            const auto instance = std::make_shared<ScriptInstance>(scriptEngineData->entityClasses[sc.className], entity);
            scriptEngineData->entityInstances[uuid] = instance;

            // TODO: Field value copying for HostFXR
            // For now, we'll handle basic fields in ScriptInstance
            // Complex types (Entity references) need C# bridge methods

            // C# On Create Function
            instance->InvokeOnCreate();
        }
    }

    void ScriptEngine::OnUpdateEntity(Entity entity, float time)
    {
        const UUID uuid = entity.GetUUID();
        if (const auto &it = scriptEngineData->entityInstances.find(uuid); it == scriptEngineData->entityInstances.end())
        {
            LOG_ERROR("[Script Engine] Entity script instance is not attached! {} {}", entity.GetName(), static_cast<uint64_t>(uuid));
            return;
        }

        const auto instance = scriptEngineData->entityInstances.at(uuid);
        instance->InvokeOnUpdate(time);
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

    ScriptFieldMap &ScriptEngine::GetScriptFieldMap(Entity entity)
    {
        LOG_ASSERT(entity.IsValid(), "[Script Engine] Failed to get entity");

        const UUID uuid = entity.GetUUID();
        return scriptEngineData->entityScriptFields[uuid];
    }

    std::vector<std::string> ScriptEngine::GetScriptClassStorage()
    {
        return scriptEngineData->entityScriptStorage;
    }

    Ref<ScriptInstance> ScriptEngine::GetEntityScriptInstance(UUID uuid)
    {
        const auto &it = scriptEngineData->entityInstances.find(uuid);
        if (it == scriptEngineData->entityInstances.end())
        {
            LOG_ERROR("[Script Engine] Failed to find {}", static_cast<uint64_t>(uuid));
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
        scriptEngineData->entityClasses.clear();

        const std::string appAssemblyName = scriptEngineData->appAssemblyFilepath.stem().string();
        std::string derivedTypes = scriptEngineData->scriptHost->GetDerivedTypes(scriptEngineData->appAssemblyFilepath, "IgniteScriptEngine.Entity");
        
        if (derivedTypes.empty())
        {
            LOG_WARN("[Script Engine] No derived script classes found in {}", scriptEngineData->appAssemblyFilepath.generic_string());
            return;
        }

        size_t start = 0;
        while (start <= derivedTypes.size())
        {
            const size_t end = derivedTypes.find('|', start);
            const std::string fullName = (end == std::string::npos)
                ? derivedTypes.substr(start)
                : derivedTypes.substr(start, end - start);

            if (!fullName.empty())
            {
                const size_t lastDot = fullName.find_last_of('.');
                const std::string classNamespace = (lastDot == std::string::npos) ? "" : fullName.substr(0, lastDot);
                const std::string className = (lastDot == std::string::npos) ? fullName : fullName.substr(lastDot + 1);

                scriptEngineData->entityClasses[fullName] = CreateRef<ScriptClass>(classNamespace, className, appAssemblyName);
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
