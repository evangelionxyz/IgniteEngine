// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "engine_api.h"
#include "ignite/core/application.hpp"
#include "ignite/core/layer.hpp"
#include "ignite/graphics/window.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_camera.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/scene/editor_camera.hpp"
#include "ignite/core/input/key_event.hpp"
#include "ignite/core/input/mouse_event.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/serializer/scene_serializer.hpp"

#include "native_embedded_app.h"
#include "avalonia_layer.h"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

static ignite::NativeEmbeddedApp *s_EmbeddedApp = nullptr;

// Static string buffers for returning strings to C#
static std::string s_ProjectNameBuffer;
static std::string s_ProjectDirBuffer;
static std::string s_ProjectFilePathBuffer;
static std::string s_ProjectAssetDirBuffer;
static std::string s_SceneHierarchyBuffer;

extern "C" {

IGN_API bool Ignite_Init(const IgniteAppConfig *config)
{
    if (s_EmbeddedApp)
    {
        return true;
    }

    ignite::Logger::Init();

    ignite::ApplicationCreateInfo createInfo{};
    createInfo.name = "Ignite Viewport";
    createInfo.width = config ? config->width : 1280;
    createInfo.height = config ? config->height : 720;
    createInfo.offscreen = config ? config->offscreen : false;
    createInfo.nativeWindowHandle = config ? config->nativeWindowHandle : nullptr;
    createInfo.useGui = true; // Avalonia manages UI

    if (config)
    {
        createInfo.graphicsApi = (config->graphicsApi == 1) ? nvrhi::GraphicsAPI::D3D12 : nvrhi::GraphicsAPI::VULKAN;
    }
    else
    {
        createInfo.graphicsApi = nvrhi::GraphicsAPI::VULKAN;
    }

    s_EmbeddedApp = new ignite::NativeEmbeddedApp(createInfo);
    return s_EmbeddedApp != nullptr;
}

IGN_API void Ignite_Tick(float deltaTime)
{
    if (s_EmbeddedApp)
    {
        s_EmbeddedApp->Tick(deltaTime);
    }
}

IGN_API void Ignite_Resize(uint32_t width, uint32_t height)
{
    if (s_EmbeddedApp)
    {
        s_EmbeddedApp->ResizeEmbedded(width, height);
    }
}

IGN_API void Ignite_Shutdown()
{
    if (s_EmbeddedApp)
    {
        delete s_EmbeddedApp;
        s_EmbeddedApp = nullptr;
        ignite::Logger::Shutdown();
    }
}

IGN_API void *Ignite_GetNativeWindow()
{
    if (s_EmbeddedApp && s_EmbeddedApp->GetWindow())
    {
        return (void*)s_EmbeddedApp->GetWindow()->GetNativeWindow();
    }
    return nullptr;
}

IGN_API void *Ignite_GetSharedTextureHandle()
{
    ignite::DeviceManager *dm = ignite::DeviceManager::GetInstance();
    if (dm)
    {
        return dm->GetSharedBackBufferHandle();
    }
    return nullptr;
}

IGN_API bool Ignite_ReadbackViewportPixels(void *outBuffer, uint32_t bufferSize)
{
    ignite::DeviceManager *dm = ignite::DeviceManager::GetInstance();
    if (dm)
    {
        return dm->ReadbackBackBuffer(outBuffer, bufferSize);
    }
    return false;
}

IGN_API void Ignite_SetLogCallback(IgniteLogCallback callback)
{
    ignite::Logger::SetLogCallback(callback);
}

// ======================================
// Project API
// ======================================
IGN_API bool Ignite_Project_New(const char *name, const char *parentDirectory)
{
    if (!name || !parentDirectory)
        return false;

    bool success = false;
    std::string nameStr = name;
    std::string dirStr = parentDirectory;

    std::thread t([&]() {
        Ref<ignite::Project> proj = ignite::Project::New(nameStr, dirStr);
        success = (proj != nullptr);
    });

    if (t.joinable())
        t.join();
    return success;
}

IGN_API bool Ignite_Project_Open(const char *filepath)
{
    if (!filepath)
        return false;

    bool success = false;
    std::string pathStr = filepath;

    std::thread t([&]() {
        Ref<ignite::Project> proj = ignite::Project::Open(pathStr);
        success = (proj != nullptr);
    });

    if (t.joinable())
        t.join();
    return success;
}

IGN_API bool Ignite_Project_Save()
{
    return ignite::Project::SaveActive();
}

IGN_API void Ignite_Project_Close()
{
    ignite::Project::CloseActive();
}

IGN_API bool Ignite_Project_IsOpen()
{
    return ignite::Project::GetActive() != nullptr;
}

IGN_API const char *Ignite_Project_GetName()
{
    if (Ref<ignite::Project> active = ignite::Project::GetActive())
    {
        s_ProjectNameBuffer = active->GetInfo().name;
        return s_ProjectNameBuffer.c_str();
    }
    return "";
}

IGN_API const char *Ignite_Project_GetDirectory()
{
    if (Ref<ignite::Project> active = ignite::Project::GetActive())
    {
        s_ProjectDirBuffer = active->GetDirectory().generic_string();
        return s_ProjectDirBuffer.c_str();
    }
    return "";
}

IGN_API const char *Ignite_Project_GetFilePath()
{
    if (Ref<ignite::Project> active = ignite::Project::GetActive())
    {
        s_ProjectFilePathBuffer = active->GetFilepath().generic_string();
        return s_ProjectFilePathBuffer.c_str();
    }
    return "";
}

IGN_API const char *Ignite_Project_GetAssetDirectory()
{
    if (Ref<ignite::Project> active = ignite::Project::GetActive())
    {
        s_ProjectAssetDirBuffer = active->GetAssetDirectory().generic_string();
        return s_ProjectAssetDirBuffer.c_str();
    }
    return "";
}

// Asset Manager API
IGN_API uint32_t Ignite_AssetManager_GetAssetCount()
{
    if (auto am = ignite::AssetManager::GetInstance())
    {
        return static_cast<uint32_t>(am->GetAssetAssetRegistry().size());
    }
    return 0;
}

IGN_API void Ignite_AssetManager_Refresh()
{
    if (auto am = ignite::AssetManager::GetInstance())
    {
        am->SyncFromRust();
    }
}

IGN_API void Ignite_AssetManager_SyncFromRust()
{
    if (auto am = ignite::AssetManager::GetInstance())
    {
        am->SyncFromRust();
    }
}

// ======================================
// Editor Camera API
// ======================================

IGN_API void Ignite_Camera_SetNavigationMode(int mode)
{
    if (s_EmbeddedApp)
        s_EmbeddedApp->SetCameraNavigationMode(mode);
}

IGN_API int Ignite_Camera_GetNavigationMode()
{
    if (s_EmbeddedApp)
        return s_EmbeddedApp->GetCameraNavigationMode();
    return 0;
}

// ======================================
// Viewport & Picking API
// ======================================

IGN_API uint64_t Ignite_Viewport_PickEntity(float mouseX, float mouseY, uint32_t viewportWidth, uint32_t viewportHeight, bool isDoubleClick, bool isShiftDown)
{
    if (s_EmbeddedApp)
    {
        const auto uuid = static_cast<uint64_t>(s_EmbeddedApp->PickEntity(mouseX, mouseY,
            viewportWidth, viewportHeight, isDoubleClick, isShiftDown));
        return uuid;
    }
    return 0;
}

IGN_API void Ignite_Viewport_SetSelectedEntity(uint64_t uuid)
{
    if (s_EmbeddedApp)
        s_EmbeddedApp->SetSelectedEntity(uuid);
}

IGN_API void Ignite_Viewport_SelectEntity(uint64_t uuid, bool multiSelect)
{
    if (s_EmbeddedApp)
        s_EmbeddedApp->SelectEntity(uuid, multiSelect);
}

IGN_API void Ignite_Viewport_DeselectEntity(uint64_t uuid)
{
    if (s_EmbeddedApp)
        s_EmbeddedApp->DeselectEntity(uuid);
}

IGN_API bool Ignite_Viewport_IsEntitySelected(uint64_t uuid)
{
    if (s_EmbeddedApp)
        return s_EmbeddedApp->IsEntitySelected(uuid);
    return false;
}

IGN_API void Ignite_Viewport_SetSelectedEntities(const uint64_t *uuids, uint32_t count)
{
    if (s_EmbeddedApp)
        s_EmbeddedApp->SetSelectedEntities(uuids, count);
}

IGN_API uint32_t Ignite_Viewport_GetSelectedEntities(uint64_t *outUuids, uint32_t maxCount)
{
    if (s_EmbeddedApp)
        return s_EmbeddedApp->GetSelectedEntities(outUuids, maxCount);
    return 0;
}

IGN_API uint32_t Ignite_Viewport_GetSelectedEntityCount()
{
    if (s_EmbeddedApp)
        return s_EmbeddedApp->GetSelectedEntityCount();
    return 0;
}

IGN_API void Ignite_Viewport_ClearSelectedEntities()
{
    if (s_EmbeddedApp)
        s_EmbeddedApp->ClearSelectedEntities();
}

IGN_API uint64_t Ignite_Viewport_GetSelectedEntity()
{
    if (s_EmbeddedApp)
        return s_EmbeddedApp->GetSelectedEntity();
    return 0;
}

IGN_API void Ignite_Viewport_SetEntitySelectedCallback(IgniteEntitySelectedCallback callback)
{
    if (s_EmbeddedApp)
        s_EmbeddedApp->SetEntitySelectedCallback(callback);
}

// ======================================
// Scene API
// ======================================

IGN_API const char *Ignite_Scene_DeserializeHierarchyJson(const char *filepath)
{
    if (!filepath)
        return "[]";

    s_SceneHierarchyBuffer = ignite::SceneSerializer::DeserializeHierarchyJson(filepath);
    return s_SceneHierarchyBuffer.c_str();
}

IGN_API const char *Ignite_Scene_GetActiveHierarchyJson()
{
    if (Ref<ignite::Project> active = ignite::Project::GetActive())
    {
        Ref<ignite::Scene> scene = active->LockActiveScene();
        if (scene)
        {
            s_SceneHierarchyBuffer = ignite::SceneSerializer::GetSceneHierarchyJson(scene.get());
            return s_SceneHierarchyBuffer.c_str();
        }
    }
    return "[]";
}

IGN_API bool Ignite_Scene_New()
{
    Ref<ignite::Project> active = ignite::Project::GetActive();
    if (!active)
        return false;

    Ref<ignite::Scene> newScene = ignite::Scene::Create(active.get());
    if (!newScene)
        return false;

    active->SetActiveScene(newScene);
    return true;
}

IGN_API bool Ignite_Scene_Open(const char *filepath)
{
    if (!filepath)
        return false;
    
    Ref<ignite::Project> active = ignite::Project::GetActive();
    if (!active)
        return false;
    
    Ref<ignite::Scene> loadedScene = ignite::SceneSerializer::Deserialize(filepath, active.get());
    if (!loadedScene)
        return false;
    
    active->SetActiveScene(loadedScene);
    return true;
}

IGN_API bool Ignite_Scene_Save()
{
    Ref<ignite::Project> active = ignite::Project::GetActive();
    if (!active)
        return false;

    Ref<ignite::Scene> scene = active->LockActiveScene();
    if (!scene)
        return false;

    if (ignite::AssetManager *am = ignite::AssetManager::GetInstance())
    {
        const auto &meta = am->GetMetaData(scene->handle);
        if (!meta.filepath.empty())
        {
            std::filesystem::path scenePath = active->GetProjectFilepath(meta.filepath);
            ignite::SceneSerializer serializer(scene, active.get());
            return serializer.Serialize(scenePath);
        }
    }
    return false;
}

// ======================================
// Entity & Component API
// ======================================

IGN_API uint64_t Ignite_Entity_Create(const char *name, uint64_t parentUuid)
{
    Ref<ignite::Project> active = ignite::Project::GetActive();
    if (!active)
        return 0;

    Ref<ignite::Scene> scene = active->LockActiveScene();
    if (!scene)
        return 0;

    std::string entityName = (name && strlen(name) > 0) ? name : "Empty Entity";
    ignite::Entity newEntity = ignite::SceneManager::CreateEmptyEntity(scene.get(), entityName);
    if (!newEntity)
        return 0;

    if (parentUuid != 0)
    {
        ignite::Entity parent = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(parentUuid));
        if (parent)
        {
            ignite::SceneManager::AddChild(scene.get(), parent, newEntity);
        }
    }

    scene->SetDirtyFlag(true);
    return static_cast<uint64_t>(newEntity.GetUUID());
}

IGN_API bool Ignite_Entity_Delete(uint64_t uuid)
{
    Ref<ignite::Project> active = ignite::Project::GetActive();
    if (!active)
        return false;

    Ref<ignite::Scene> scene = active->LockActiveScene();
    if (!scene)
        return false;

    ignite::SceneManager::DestroyEntity(scene.get(), ignite::UUID(uuid));
    scene->UpdateTransforms(0.0f);
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_Rename(uint64_t uuid, const char *newName)
{
    if (!newName)
        return false;

    Ref<ignite::Project> active = ignite::Project::GetActive();
    if (!active)
        return false;

    Ref<ignite::Scene> scene = active->LockActiveScene();
    if (!scene)
        return false;

    ignite::Entity entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity)
        return false;

    ignite::SceneManager::RenameEntity(scene.get(), entity, newName);
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_Reparent(uint64_t entityUuid, uint64_t newParentUuid)
{
    Ref<ignite::Project> active = ignite::Project::GetActive();
    if (!active)
        return false;

    Ref<ignite::Scene> scene = active->LockActiveScene();
    if (!scene)
        return false;

    ignite::Entity entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(entityUuid));
    if (!entity)
        return false;

    if (newParentUuid == 0)
    {
        auto &idComp = entity.GetComponent<ignite::IDComponent>();
        if (idComp.parent != ignite::UUID(0))
        {
            ignite::Entity oldParent = ignite::SceneManager::GetEntity(scene.get(), idComp.parent);
            if (oldParent)
            {
                oldParent.GetComponent<ignite::IDComponent>().RemoveChild(idComp.uuid);
            }
            idComp.parent = ignite::UUID(0);
        }
        scene->UpdateTransforms(0.0f);
        scene->SetDirtyFlag(true);
        return true;
    }
    else
    {
        ignite::Entity newParent = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(newParentUuid));
        if (!newParent)
            return false;

        bool success = ignite::SceneManager::AddChild(scene.get(), newParent, entity);
        if (success)
        {
            scene->UpdateTransforms(0.0f);
            scene->SetDirtyFlag(true);
        }
        return success;
    }
}

IGN_API uint64_t Ignite_Entity_Duplicate(uint64_t uuid)
{
    Ref<ignite::Project> active = ignite::Project::GetActive();
    if (!active)
        return 0;

    Ref<ignite::Scene> scene = active->LockActiveScene();
    if (!scene)
        return 0;

    ignite::Entity entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity)
        return 0;

    ignite::Entity duplicated = ignite::SceneManager::DuplicateEntity(scene.get(), entity, true);
    if (duplicated)
    {
        scene->UpdateTransforms(0.0f);
        scene->SetDirtyFlag(true);
        return static_cast<uint64_t>(duplicated.GetUUID());
    }
    return 0;
}

IGN_API bool Ignite_Entity_SetActive(uint64_t uuid, bool active)
{
    Ref<ignite::Project> activeProject = ignite::Project::GetActive();
    if (!activeProject)
        return false;

    Ref<ignite::Scene> scene = activeProject->LockActiveScene();
    if (!scene)
        return false;

    ignite::Entity entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity)
        return false;

    if (entity.HasComponent<ignite::RenderingComponent>())
    {
        entity.GetComponent<ignite::RenderingComponent>().visible = active;
    }
    else
    {
        auto &rc = entity.AddComponent<ignite::RenderingComponent>();
        rc.visible = active;
    }

    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetTransform(uint64_t uuid, float px, float py, float pz, float rx, float ry, float rz, float sx, float sy, float sz)
{
    Ref<ignite::Project> active = ignite::Project::GetActive();
    if (!active)
        return false;

    Ref<ignite::Scene> scene = active->LockActiveScene();
    if (!scene)
        return false;

    ignite::Entity entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::TransformComponent>())
        return false;

    auto &tc = entity.GetComponent<ignite::TransformComponent>();
    tc.local.translation = glm::vec3(px, py, pz);
    tc.local.rotation = glm::quat(glm::radians(glm::vec3(rx, ry, rz)));
    tc.local.scale = glm::vec3(sx, sy, sz);

    scene->UpdateTransforms(0.0f);
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetSpriteColor(uint64_t uuid, float r, float g, float b, float a)
{
    Ref<ignite::Project> active = ignite::Project::GetActive();
    if (!active)
        return false;

    Ref<ignite::Scene> scene = active->LockActiveScene();
    if (!scene)
        return false;

    ignite::Entity entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity)
        return false;

    if (entity.HasComponent<ignite::Sprite2DComponent>())
    {
        auto &sprite = entity.GetComponent<ignite::Sprite2DComponent>();
        sprite.color = glm::vec4(r, g, b, a);
        scene->SetDirtyFlag(true);
        return true;
    }
    if (entity.HasComponent<ignite::Circle2DComponent>())
    {
        auto &circle = entity.GetComponent<ignite::Circle2DComponent>();
        circle.color = glm::vec4(r, g, b, a);
        scene->SetDirtyFlag(true);
        return true;
    }

    return false;
}

IGN_API bool Ignite_Entity_AddComponent(uint64_t uuid, int componentType)
{
    Ref<ignite::Project> active = ignite::Project::GetActive();
    if (!active)
        return false;

    Ref<ignite::Scene> scene = active->LockActiveScene();
    if (!scene)
        return false;

    ignite::Entity entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity)
        return false;

    switch (componentType)
    {
    case ignite::CompType_Camera:
        if (!entity.HasComponent<ignite::CameraComponent>())
            entity.AddComponent<ignite::CameraComponent>();
        break;
    case ignite::CompType_Sprite2D:
        if (!entity.HasComponent<ignite::Sprite2DComponent>())
            entity.AddComponent<ignite::Sprite2DComponent>();
        break;
    case ignite::CompType_Circle2D:
        if (!entity.HasComponent<ignite::Circle2DComponent>())
            entity.AddComponent<ignite::Circle2DComponent>();
        break;
    case ignite::CompType_PointLight2D:
        if (!entity.HasComponent<ignite::PointLight2DComponent>())
            entity.AddComponent<ignite::PointLight2DComponent>();
        break;
    case ignite::CompType_DirectionalLight:
        if (!entity.HasComponent<ignite::DirectionalLightComponent>())
            entity.AddComponent<ignite::DirectionalLightComponent>();
        break;
    case ignite::CompType_PointLight:
        if (!entity.HasComponent<ignite::PointLightComponent>())
            entity.AddComponent<ignite::PointLightComponent>();
        break;
    case ignite::CompType_SpotLight:
        if (!entity.HasComponent<ignite::SpotLightComponent>())
            entity.AddComponent<ignite::SpotLightComponent>();
        break;
    case ignite::CompType_StaticMesh:
        if (!entity.HasComponent<ignite::StaticMeshComponent>())
            entity.AddComponent<ignite::StaticMeshComponent>();
        break;
    case ignite::CompType_SkeletalMesh:
        if (!entity.HasComponent<ignite::SkeletalMeshComponent>())
            entity.AddComponent<ignite::SkeletalMeshComponent>();
        break;
    case ignite::CompType_Rigidbody:
        if (!entity.HasComponent<ignite::RigidbodyComponent>())
            entity.AddComponent<ignite::RigidbodyComponent>();
        break;
    case ignite::CompType_Rigidbody2D:
        if (!entity.HasComponent<ignite::Rigidbody2DComponent>())
            entity.AddComponent<ignite::Rigidbody2DComponent>();
        break;
    case ignite::CompType_BoxCollider:
        if (!entity.HasComponent<ignite::BoxColliderComponent>())
            entity.AddComponent<ignite::BoxColliderComponent>();
        break;
    case ignite::CompType_BoxCollider2D:
        if (!entity.HasComponent<ignite::BoxCollider2DComponent>())
            entity.AddComponent<ignite::BoxCollider2DComponent>();
        break;
    case ignite::CompType_SphereCollider:
        if (!entity.HasComponent<ignite::SphereColliderComponent>())
            entity.AddComponent<ignite::SphereColliderComponent>();
        break;
    case ignite::CompType_CapsuleCollider:
        if (!entity.HasComponent<ignite::CapsuleColliderComponent>())
            entity.AddComponent<ignite::CapsuleColliderComponent>();
        break;
    case ignite::CompType_CircleCollider2D:
        if (!entity.HasComponent<ignite::CircleCollider2DComponent>())
            entity.AddComponent<ignite::CircleCollider2DComponent>();
        break;
    case ignite::CompType_MeshCollider:
        if (!entity.HasComponent<ignite::MeshColliderComponent>())
            entity.AddComponent<ignite::MeshColliderComponent>();
        break;
    case ignite::CompType_AudioSource:
        if (!entity.HasComponent<ignite::AudioSourceComponent>())
            entity.AddComponent<ignite::AudioSourceComponent>();
        break;
    case ignite::CompType_WorldEnvironment:
        if (!entity.HasComponent<ignite::WorldEnvironment>())
            entity.AddComponent<ignite::WorldEnvironment>();
        break;
    case ignite::CompType_CharacterController:
        if (!entity.HasComponent<ignite::CharacterControllerComponent>())
            entity.AddComponent<ignite::CharacterControllerComponent>();
        break;
    case ignite::CompType_Text:
        if (!entity.HasComponent<ignite::TextComponent>())
            entity.AddComponent<ignite::TextComponent>();
        break;
    default:
        return false;
    }

    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_RemoveComponent(uint64_t uuid, int componentType)
{
    Ref<ignite::Project> active = ignite::Project::GetActive();
    if (!active)
        return false;

    Ref<ignite::Scene> scene = active->LockActiveScene();
    if (!scene)
        return false;

    ignite::Entity entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity)
        return false;

    switch (componentType)
    {
    case ignite::CompType_Camera:
        if (entity.HasComponent<ignite::CameraComponent>())
            entity.RemoveComponent<ignite::CameraComponent>();
        break;
    case ignite::CompType_Sprite2D:
        if (entity.HasComponent<ignite::Sprite2DComponent>())
            entity.RemoveComponent<ignite::Sprite2DComponent>();
        break;
    case ignite::CompType_Circle2D:
        if (entity.HasComponent<ignite::Circle2DComponent>())
            entity.RemoveComponent<ignite::Circle2DComponent>();
        break;
    case ignite::CompType_PointLight2D:
        if (entity.HasComponent<ignite::PointLight2DComponent>())
            entity.RemoveComponent<ignite::PointLight2DComponent>();
        break;
    case ignite::CompType_DirectionalLight:
        if (entity.HasComponent<ignite::DirectionalLightComponent>())
            entity.RemoveComponent<ignite::DirectionalLightComponent>();
        break;
    case ignite::CompType_PointLight:
        if (entity.HasComponent<ignite::PointLightComponent>())
            entity.RemoveComponent<ignite::PointLightComponent>();
        break;
    case ignite::CompType_SpotLight:
        if (entity.HasComponent<ignite::SpotLightComponent>())
            entity.RemoveComponent<ignite::SpotLightComponent>();
        break;
    case ignite::CompType_StaticMesh:
        if (entity.HasComponent<ignite::StaticMeshComponent>())
            entity.RemoveComponent<ignite::StaticMeshComponent>();
        break;
    case ignite::CompType_SkeletalMesh:
        if (entity.HasComponent<ignite::SkeletalMeshComponent>())
            entity.RemoveComponent<ignite::SkeletalMeshComponent>();
        break;
    case ignite::CompType_Rigidbody:
        if (entity.HasComponent<ignite::RigidbodyComponent>())
            entity.RemoveComponent<ignite::RigidbodyComponent>();
        break;
    case ignite::CompType_Rigidbody2D:
        if (entity.HasComponent<ignite::Rigidbody2DComponent>())
            entity.RemoveComponent<ignite::Rigidbody2DComponent>();
        break;
    case ignite::CompType_BoxCollider:
        if (entity.HasComponent<ignite::BoxColliderComponent>())
            entity.RemoveComponent<ignite::BoxColliderComponent>();
        break;
    case ignite::CompType_BoxCollider2D:
        if (entity.HasComponent<ignite::BoxCollider2DComponent>())
            entity.RemoveComponent<ignite::BoxCollider2DComponent>();
        break;
    case ignite::CompType_SphereCollider:
        if (entity.HasComponent<ignite::SphereColliderComponent>())
            entity.RemoveComponent<ignite::SphereColliderComponent>();
        break;
    case ignite::CompType_CapsuleCollider:
        if (entity.HasComponent<ignite::CapsuleColliderComponent>())
            entity.RemoveComponent<ignite::CapsuleColliderComponent>();
        break;
    case ignite::CompType_CircleCollider2D:
        if (entity.HasComponent<ignite::CircleCollider2DComponent>())
            entity.RemoveComponent<ignite::CircleCollider2DComponent>();
        break;
    case ignite::CompType_MeshCollider:
        if (entity.HasComponent<ignite::MeshColliderComponent>())
            entity.RemoveComponent<ignite::MeshColliderComponent>();
        break;
    case ignite::CompType_AudioSource:
        if (entity.HasComponent<ignite::AudioSourceComponent>())
            entity.RemoveComponent<ignite::AudioSourceComponent>();
        break;
    case ignite::CompType_WorldEnvironment:
        if (entity.HasComponent<ignite::WorldEnvironment>())
            entity.RemoveComponent<ignite::WorldEnvironment>();
        break;
    case ignite::CompType_CharacterController:
        if (entity.HasComponent<ignite::CharacterControllerComponent>())
            entity.RemoveComponent<ignite::CharacterControllerComponent>();
        break;
    case ignite::CompType_Text:
        if (entity.HasComponent<ignite::TextComponent>())
            entity.RemoveComponent<ignite::TextComponent>();
        break;
    default:
        return false;
    }

    scene->SetDirtyFlag(true);
    return true;
}

static Ref<ignite::Scene> GetActiveSceneHelper()
{
    if (s_EmbeddedApp && s_EmbeddedApp->GetAvaloniaLayer())
    {
        Ref<ignite::Scene> sc = s_EmbeddedApp->GetAvaloniaLayer()->GetCurrentScene();
        if (sc) return sc;
    }
    if (Ref<ignite::Project> activeProject = ignite::Project::GetActive())
    {
        return activeProject->LockActiveScene();
    }
    return nullptr;
}

IGN_API void Ignite_Scene_Play()
{
    if (s_EmbeddedApp)
         s_EmbeddedApp->Play();
}

IGN_API void Ignite_Scene_Stop()
{
    if (s_EmbeddedApp)
        s_EmbeddedApp->Stop();
}

IGN_API void Ignite_Scene_Simulate()
{
    if (s_EmbeddedApp)
        s_EmbeddedApp->Simulate();
}

IGN_API void Ignite_Scene_Pause()
{
    if (s_EmbeddedApp)
        s_EmbeddedApp->Pause();
}

IGN_API void Ignite_Scene_StepFrame(int frames)
{
    if (s_EmbeddedApp)
        s_EmbeddedApp->StepFrame(frames);
}

IGN_API int Ignite_Scene_GetState()
{
    if (s_EmbeddedApp)
        return s_EmbeddedApp->GetSceneState();
    return 1;
}

IGN_API bool Ignite_Entity_SetCamera(uint64_t uuid, bool isPerspective, float fov, float nearPlane, float farPlane, float orthoSize)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::CameraComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::CameraComponent>();
    comp.camera.projectionType = isPerspective ? ignite::ProjectionType::Perspective : ignite::ProjectionType::Orthographic;
    comp.camera.fov = fov;
    comp.camera.nearPlane = nearPlane;
    comp.camera.farPlane = farPlane;
    comp.camera.orthoSize = orthoSize;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetDirectionalLight(uint64_t uuid, float r, float g, float b, float a, float intensity, float shadowDistance, bool castShadows)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::DirectionalLightComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::DirectionalLightComponent>();
    comp.color = glm::vec4(r, g, b, a);
    comp.intensity = intensity;
    comp.shadowDistance = shadowDistance;
    comp.cascadeShadow = castShadows;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetPointLight(uint64_t uuid, float r, float g, float b, float a, float intensity, float range, bool enabled)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::PointLightComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::PointLightComponent>();
    comp.color = glm::vec4(r, g, b, a);
    comp.intensity = intensity;
    comp.range = range;
    comp.enabled = enabled;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetSpotLight(uint64_t uuid, float r, float g, float b, float a, float intensity, float range, float innerCone, float outerCone, bool enabled)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::SpotLightComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::SpotLightComponent>();
    comp.color = glm::vec4(r, g, b, a);
    comp.intensity = intensity;
    comp.range = range;
    comp.innerConeAngle = innerCone;
    comp.outerConeAngle = outerCone;
    comp.enabled = enabled;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetPointLight2D(uint64_t uuid, float r, float g, float b, float a, float radius, float intensity, bool enabled)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::PointLight2DComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::PointLight2DComponent>();
    comp.color = glm::vec4(r, g, b, a);
    comp.radius = radius;
    comp.intensity = intensity;
    comp.enabled = enabled;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetSprite2D(uint64_t uuid, float r, float g, float b, float a, float tilingX, float tilingY, bool flipX, bool flipY)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::Sprite2DComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::Sprite2DComponent>();
    comp.color = glm::vec4(r, g, b, a);
    comp.tilingFactor = glm::vec2(tilingX, tilingY);
    comp.flipX = flipX;
    comp.flipY = flipY;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetCircle2D(uint64_t uuid, float r, float g, float b, float a, float thickness, float fade)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::Circle2DComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::Circle2DComponent>();
    comp.color = glm::vec4(r, g, b, a);
    comp.thickness = thickness;
    comp.fade = fade;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetRigidbody(uint64_t uuid, int bodyType, float mass, float linearDamping, float angularDamping, float friction, float restitution, bool useGravity)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::RigidbodyComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::RigidbodyComponent>();
    comp.bodyType = static_cast<ignite::physics::BodyType>(bodyType);
    comp.mass = mass;
    comp.linearDamping = linearDamping;
    comp.angularDamping = angularDamping;
    comp.friction = friction;
    comp.restitution = restitution;
    comp.useGravity = useGravity;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetRigidbody2D(uint64_t uuid, int bodyType, float gravityScale, float linearDamping, float angularDamping, bool fixedRotation, bool isAwake, bool isEnabled)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::Rigidbody2DComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::Rigidbody2DComponent>();
    comp.bodyType = static_cast<ignite::physics::BodyType>(bodyType);
    comp.gravityScale = gravityScale;
    comp.linearDamping = linearDamping;
    comp.angularDamping = angularDamping;
    comp.fixedRotation = fixedRotation;
    comp.isAwake = isAwake;
    comp.isEnabled = isEnabled;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetBoxCollider(uint64_t uuid, float cx, float cy, float cz, float sx, float sy, float sz)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::BoxColliderComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::BoxColliderComponent>();
    comp.center = glm::vec3(cx, cy, cz);
    comp.scale = glm::vec3(sx, sy, sz);
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetSphereCollider(uint64_t uuid, float cx, float cy, float cz, float radius)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::SphereColliderComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::SphereColliderComponent>();
    comp.center = glm::vec3(cx, cy, cz);
    comp.radius = radius;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetCapsuleCollider(uint64_t uuid, float cx, float cy, float cz, float radius, float height)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::CapsuleColliderComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::CapsuleColliderComponent>();
    comp.center = glm::vec3(cx, cy, cz);
    comp.radius = radius;
    comp.height = height;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetBoxCollider2D(uint64_t uuid, float ox, float oy, float sx, float sy, float density, float friction, float restitution, bool isSensor)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::BoxCollider2DComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::BoxCollider2DComponent>();
    comp.offset = glm::vec2(ox, oy);
    comp.size = glm::vec2(sx, sy);
    comp.density = density;
    comp.friction = friction;
    comp.restitution = restitution;
    comp.isSensor = isSensor;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetCircleCollider2D(uint64_t uuid, float cx, float cy, float radius, float density, float friction, float restitution, bool isSensor)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::CircleCollider2DComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::CircleCollider2DComponent>();
    comp.center = glm::vec2(cx, cy);
    comp.radius = radius;
    comp.density = density;
    comp.friction = friction;
    comp.restitution = restitution;
    comp.isSensor = isSensor;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetCharacterController(uint64_t uuid, float radius, float height, float stepHeight, float slopeAngle, float mass, float friction)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::CharacterControllerComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::CharacterControllerComponent>();
    comp.radius = radius;
    comp.height = height;
    comp.maxStepHeight = stepHeight;
    comp.maxSlopeAngle = slopeAngle;
    comp.mass = mass;
    comp.friction = friction;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetAudioSource(uint64_t uuid, float volume, float pitch, float pan, bool playOnStart, bool loop)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::AudioSourceComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::AudioSourceComponent>();
    comp.volume = volume;
    comp.pitch = pitch;
    comp.pan = pan;
    comp.playOnStart = playOnStart;
    comp.loop = loop;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetText(uint64_t uuid, const char *text, float r, float g, float b, float a, float kerning, float lineSpacing, bool screenSpace)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::TextComponent>()) return false;
    auto &comp = entity.GetComponent<ignite::TextComponent>();
    if (text) comp.text = text;
    comp.color = glm::vec4(r, g, b, a);
    comp.kerning = kerning;
    comp.lineSpacing = lineSpacing;
    comp.screenSpace = screenSpace;
    scene->SetDirtyFlag(true);
    return true;
}

IGN_API bool Ignite_Entity_SetWorldEnvironment(uint64_t uuid, float exposure, float gamma, float ambient, float fogDensity, float fr, float fg, float fb, float fa, float fogStart, float fogEnd)
{
    auto scene = GetActiveSceneHelper();
    if (!scene) return false;
    auto entity = ignite::SceneManager::GetEntity(scene.get(), ignite::UUID(uuid));
    if (!entity || !entity.HasComponent<ignite::WorldEnvironment>()) return false;
    auto &comp = entity.GetComponent<ignite::WorldEnvironment>();
    comp.exposure = exposure;
    comp.gamma = gamma;
    comp.ambient = ambient;
    comp.fogDensity = fogDensity;
    comp.fogColor = glm::vec4(fr, fg, fb, fa);
    comp.fogStart = fogStart;
    comp.fogEnd = fogEnd;
    comp.dirtyEnvironment = true;
    scene->SetDirtyFlag(true);
    return true;
}

// ======================================
// Window & Event System API
// ======================================
IGN_API void Ignite_Window_RegisterExternalWindowID(uint32_t sdlWindowId)
{
    if (s_EmbeddedApp && s_EmbeddedApp->GetWindow())
    {
        s_EmbeddedApp->GetWindow()->RegisterExternalWindowID(static_cast<SDL_WindowID>(sdlWindowId));
    }
}

IGN_API void Ignite_Window_UnregisterExternalWindowID(uint32_t sdlWindowId)
{
    if (s_EmbeddedApp && s_EmbeddedApp->GetWindow())
    {
        s_EmbeddedApp->GetWindow()->UnregisterExternalWindowID(static_cast<SDL_WindowID>(sdlWindowId));
    }
}

IGN_API void Ignite_Input_InjectKeyEvent(int sdlKeycode, bool isDown, uint16_t mod, bool repeat)
{
    if (auto *activeInput = ignite::InputSystem::GetActiveSystem())
    {
        activeInput->SetModifier(ignite::KeyMod::Shift, (mod & SDL_KMOD_SHIFT) != 0);
        activeInput->SetModifier(ignite::KeyMod::Control, (mod & SDL_KMOD_CTRL) != 0);
        activeInput->SetModifier(ignite::KeyMod::LeftAlt, (mod & SDL_KMOD_LALT) != 0);
        activeInput->SetModifier(ignite::KeyMod::RightAlt, (mod & SDL_KMOD_RALT) != 0);
        activeInput->SetModifier(ignite::KeyMod::LeftShift, (mod & SDL_KMOD_LSHIFT) != 0);
        activeInput->SetModifier(ignite::KeyMod::RightShift, (mod & SDL_KMOD_RSHIFT) != 0);
        activeInput->SetModifier(ignite::KeyMod::LeftControl, (mod & SDL_KMOD_LCTRL) != 0);
        activeInput->SetModifier(ignite::KeyMod::RightControl, (mod & SDL_KMOD_RCTRL) != 0);
        activeInput->SetKey(static_cast<SDL_Keycode>(sdlKeycode), isDown);
    }

    if (auto *app = ignite::Application::GetInstance())
    {
        if (isDown)
        {
            ignite::KeyPressedEvent e(static_cast<KeyCode>(sdlKeycode), repeat ? 1 : 0);
            e.SetSource(ignite::EventSource::Editor);
            app->OnEvent(e);
        }
        else
        {
            ignite::KeyReleasedEvent e(static_cast<KeyCode>(sdlKeycode));
            e.SetSource(ignite::EventSource::Editor);
            app->OnEvent(e);
        }
    }
}

IGN_API void Ignite_Input_InjectMouseButton(int button, bool isDown, float x, float y)
{
    if (auto *activeInput = ignite::InputSystem::GetActiveSystem())
    {
        activeInput->SetMousePosition(static_cast<int>(x), static_cast<int>(y));
        activeInput->SetMouseButton(static_cast<MouseCode>(button), isDown);
    }

    if (auto *app = ignite::Application::GetInstance())
    {
        if (isDown)
        {
            ignite::MouseButtonPressedEvent e(static_cast<MouseCode>(button));
            e.SetSource(ignite::EventSource::Editor);
            app->OnEvent(e);
        }
        else
        {
            ignite::MouseButtonReleasedEvent e(static_cast<MouseCode>(button));
            e.SetSource(ignite::EventSource::Editor);
            app->OnEvent(e);
        }
    }
}

IGN_API void Ignite_Input_InjectMouseMove(float x, float y)
{
    if (auto *activeInput = ignite::InputSystem::GetActiveSystem())
    {
        activeInput->SetMousePosition(static_cast<int>(x), static_cast<int>(y));
    }

    if (auto *app = ignite::Application::GetInstance())
    {
        ignite::MouseMovedEvent e(static_cast<int>(x), static_cast<int>(y));
        e.SetSource(ignite::EventSource::Editor);
        app->OnEvent(e);
    }
}

IGN_API void Ignite_Input_InjectMouseScroll(float xOffset, float yOffset)
{
    if (auto *app = ignite::Application::GetInstance())
    {
        ignite::MouseScrolledEvent e(xOffset, yOffset);
        e.SetSource(ignite::EventSource::Editor);
        app->OnEvent(e);
    }
}

}

