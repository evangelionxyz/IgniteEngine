// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ENGINE_API_H
#define IGN_ENGINE_API_H

#include "ignite/core/base.hpp"
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

struct IgniteAppConfig
{
    void *nativeWindowHandle;   // HWND on Windows (e.g. Avalonia window or child control)
    uint32_t width;
    uint32_t height;
    bool offscreen;
    int graphicsApi;            // 0 = VULKAN, 1 = D3D12
    bool enableDebug;
};

IGN_API bool Ignite_Init(const IgniteAppConfig *config);
IGN_API void Ignite_Tick(float deltaTime);
IGN_API void Ignite_Resize(uint32_t width, uint32_t height);
IGN_API void Ignite_Shutdown();
IGN_API void *Ignite_GetNativeWindow();
IGN_API void *Ignite_GetSharedTextureHandle();
IGN_API bool Ignite_ReadbackViewportPixels(void *outBuffer, uint32_t bufferSize);

typedef void (*IgniteLogCallback)(int level, const char *message);
IGN_API void Ignite_SetLogCallback(IgniteLogCallback callback);

// ======================================
// Project API
// ======================================
IGN_API bool Ignite_Project_New(const char *name, const char *parentDirectory);
IGN_API bool Ignite_Project_Open(const char *filepath);
IGN_API bool Ignite_Project_Save();
IGN_API void Ignite_Project_Close();
IGN_API bool Ignite_Project_IsOpen();
IGN_API const char *Ignite_Project_GetName();
IGN_API const char *Ignite_Project_GetDirectory();
IGN_API const char *Ignite_Project_GetFilePath();
IGN_API const char *Ignite_Project_GetAssetDirectory();

// ======================================
// Asset Manager API
// ======================================
IGN_API uint32_t Ignite_AssetManager_GetAssetCount();
IGN_API void Ignite_AssetManager_Refresh();
IGN_API void Ignite_AssetManager_SyncFromRust();

// ======================================
// Editor Camera API
// ======================================
// Push the editor camera matrices from C# into the native engine each tick.
// The matrices are column-major float[16] (glm/HLSL convention).
IGN_API void Ignite_Camera_SetNavigationMode(int mode); // 0 = Orbit, 1 = Fly, 2 = Mode2D
IGN_API int  Ignite_Camera_GetNavigationMode();

// ======================================
// Scene API
// ======================================
IGN_API const char *Ignite_Scene_DeserializeHierarchyJson(const char *filepath);
IGN_API const char *Ignite_Scene_GetActiveHierarchyJson();
IGN_API bool        Ignite_Scene_Open(const char *filepath);
IGN_API bool        Ignite_Scene_New();
IGN_API bool        Ignite_Scene_Save();
IGN_API void        Ignite_Scene_Play();
IGN_API void        Ignite_Scene_Stop();
IGN_API void        Ignite_Scene_Simulate();
IGN_API void        Ignite_Scene_Pause();
IGN_API void        Ignite_Scene_StepFrame(int frames);
IGN_API int         Ignite_Scene_GetState();

// ======================================
// Entity & Component API
// ======================================
IGN_API uint64_t Ignite_Entity_Create(const char *name, uint64_t parentUuid);
IGN_API bool     Ignite_Entity_Delete(uint64_t uuid);
IGN_API bool     Ignite_Entity_Rename(uint64_t uuid, const char *newName);
IGN_API bool     Ignite_Entity_Reparent(uint64_t entityUuid, uint64_t newParentUuid);
IGN_API uint64_t Ignite_Entity_Duplicate(uint64_t uuid);
IGN_API bool     Ignite_Entity_SetActive(uint64_t uuid, bool active);
IGN_API bool     Ignite_Entity_SetTransform(uint64_t uuid, float px, float py, float pz, float rx, float ry, float rz, float sx, float sy, float sz);
IGN_API bool     Ignite_Entity_SetSpriteColor(uint64_t uuid, float r, float g, float b, float a);
IGN_API bool     Ignite_Entity_AddComponent(uint64_t uuid, int componentType);
IGN_API bool     Ignite_Entity_RemoveComponent(uint64_t uuid, int componentType);

IGN_API bool     Ignite_Entity_SetCamera(uint64_t uuid, bool isPerspective, float fov, float nearPlane, float farPlane, float orthoSize);
IGN_API bool     Ignite_Entity_SetDirectionalLight(uint64_t uuid, float r, float g, float b, float a, float intensity, float shadowDistance, bool castShadows);
IGN_API bool     Ignite_Entity_SetPointLight(uint64_t uuid, float r, float g, float b, float a, float intensity, float range, bool enabled);
IGN_API bool     Ignite_Entity_SetSpotLight(uint64_t uuid, float r, float g, float b, float a, float intensity, float range, float innerCone, float outerCone, bool enabled);
IGN_API bool     Ignite_Entity_SetPointLight2D(uint64_t uuid, float r, float g, float b, float a, float radius, float intensity, bool enabled);
IGN_API bool     Ignite_Entity_SetSprite2D(uint64_t uuid, float r, float g, float b, float a, float tilingX, float tilingY, bool flipX, bool flipY);
IGN_API bool     Ignite_Entity_SetCircle2D(uint64_t uuid, float r, float g, float b, float a, float thickness, float fade);
IGN_API bool     Ignite_Entity_SetRigidbody(uint64_t uuid, int bodyType, float mass, float linearDamping, float angularDamping, float friction, float restitution, bool useGravity);
IGN_API bool     Ignite_Entity_SetRigidbody2D(uint64_t uuid, int bodyType, float gravityScale, float linearDamping, float angularDamping, bool fixedRotation, bool isAwake, bool isEnabled);
IGN_API bool     Ignite_Entity_SetBoxCollider(uint64_t uuid, float cx, float cy, float cz, float sx, float sy, float sz);
IGN_API bool     Ignite_Entity_SetSphereCollider(uint64_t uuid, float cx, float cy, float cz, float radius);
IGN_API bool     Ignite_Entity_SetCapsuleCollider(uint64_t uuid, float cx, float cy, float cz, float radius, float height);
IGN_API bool     Ignite_Entity_SetBoxCollider2D(uint64_t uuid, float ox, float oy, float sx, float sy, float density, float friction, float restitution, bool isSensor);
IGN_API bool     Ignite_Entity_SetCircleCollider2D(uint64_t uuid, float cx, float cy, float radius, float density, float friction, float restitution, bool isSensor);
IGN_API bool     Ignite_Entity_SetCharacterController(uint64_t uuid, float radius, float height, float stepHeight, float slopeAngle, float mass, float friction);
IGN_API bool     Ignite_Entity_SetAudioSource(uint64_t uuid, float volume, float pitch, float pan, bool playOnStart, bool loop);
IGN_API bool     Ignite_Entity_SetText(uint64_t uuid, const char *text, float r, float g, float b, float a, float kerning, float lineSpacing, bool screenSpace);
IGN_API bool     Ignite_Entity_SetWorldEnvironment(uint64_t uuid, float exposure, float gamma, float ambient, float fogDensity, float fr, float fg, float fb, float fa, float fogStart, float fogEnd);


#ifdef __cplusplus
}
#endif

#endif // IGN_ENGINE_API_H
