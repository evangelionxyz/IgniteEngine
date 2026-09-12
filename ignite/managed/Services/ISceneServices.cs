using System;
using Ignite.Managed.Models;
using System.Collections.Generic;
using System.Numerics;

namespace Ignite.Managed.Services;

public interface ISceneService
{
    IReadOnlyList<EntityModel> GetRootEntities();
    EntityModel? GetEntity(Guid id);
    EntityModel CreateEntity(string name, Guid? parentId = null);
    void DeleteEntity(Guid id);
    void ReparentEntity(Guid entityId, Guid? newParentId);
    void RenameEntity(Guid id, string newName);
    EntityModel DuplicateEntity(Guid id);
    bool NewScene();
    void LoadActiveScene();
    bool LoadScene(string filepath);
    bool SaveActiveScene();
    void SetEntityTransform(Guid entityId, Vector3 position, Vector3 rotation, Vector3 scale);
    void SetEntitySpriteColor(Guid entityId, Vector4 color);
    void AddComponent(Guid entityId, ComponentType type);
    void RemoveComponent(Guid entityId, ComponentType type);
    void SetEntityActive(Guid entityId, bool active);

    // Scene Lifecycle
    void PlayScene();
    void StopScene();
    void SimulateScene();
    void PauseScene();
    void StepScene(int frames = 1);
    int GetSceneState();

    // Component Setters
    void SetEntityCamera(Guid entityId, bool isPerspective, float fov, float nearPlane, float farPlane, float orthoSize);
    void SetEntityDirectionalLight(Guid entityId, Vector4 color, float intensity, float shadowDistance, bool castShadows);
    void SetEntityPointLight(Guid entityId, Vector4 color, float intensity, float range, bool enabled);
    void SetEntitySpotLight(Guid entityId, Vector4 color, float intensity, float range, float innerCone, float outerCone, bool enabled);
    void SetEntityPointLight2D(Guid entityId, Vector4 color, float radius, float intensity, bool enabled);
    void SetEntitySprite2D(Guid entityId, Vector4 color, Vector2 tiling, bool flipX, bool flipY);
    void SetEntityCircle2D(Guid entityId, Vector4 color, float thickness, float fade);
    void SetEntityRigidbody(Guid entityId, int bodyType, float mass, float linearDamping, float angularDamping, float friction, float restitution, bool useGravity);
    void SetEntityRigidbody2D(Guid entityId, int bodyType, float gravityScale, float linearDamping, float angularDamping, bool fixedRotation, bool isAwake, bool isEnabled);
    void SetEntityBoxCollider(Guid entityId, Vector3 center, Vector3 size);
    void SetEntitySphereCollider(Guid entityId, Vector3 center, float radius);
    void SetEntityCapsuleCollider(Guid entityId, Vector3 center, float radius, float height);
    void SetEntityBoxCollider2D(Guid entityId, Vector2 offset, Vector2 size, float density, float friction, float restitution, bool isSensor);
    void SetEntityCircleCollider2D(Guid entityId, Vector2 center, float radius, float density, float friction, float restitution, bool isSensor);
    void SetEntityCharacterController(Guid entityId, float radius, float height, float stepHeight, float slopeAngle, float mass, float friction);
    void SetEntityAudioSource(Guid entityId, float volume, float pitch, float pan, bool playOnStart, bool loop);
    void SetEntityText(Guid entityId, string text, Vector4 color, float kerning, float lineSpacing, bool screenSpace);
    void SetEntityWorldEnvironment(Guid entityId, float exposure, float gamma, float ambient, float fogDensity, Vector4 fogColor, float fogStart, float fogEnd);
}
