using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using Ignite.Managed.Models;
using Ignite.Managed.Services;

namespace IgniteEditor.Services;

public class EngineSceneService : ISceneService
{
    private readonly List<EntityModel> _rootEntities = new();
    private readonly Dictionary<Guid, EntityModel> _entitiesById = new();
    private readonly object _lock = new();

    public EngineSceneService()
    {
        LoadActiveScene();
    }

    public void LoadActiveScene()
    {
        lock (_lock)
        {
            _rootEntities.Clear();
            _entitiesById.Clear();

            string json = NativeEngineBridge.GetActiveSceneHierarchyJson();
            var entities = SceneHierarchyDeserializer.Deserialize(json);

            foreach (var entity in entities)
            {
                _rootEntities.Add(entity);
                RegisterEntityRecursive(entity);
            }
        }
    }

    public bool NewScene()
    {
        return NativeEngineBridge.Ignite_Scene_New();
    }

    public bool LoadScene(string filepath)
    {
        if (string.IsNullOrWhiteSpace(filepath))
            return false;

        bool opened = NativeEngineBridge.Ignite_Scene_Open(filepath);
        if (opened)
        {
            LoadActiveScene();
            return true;
        }
        return false;
    }

    public bool SaveActiveScene()
    {
        return NativeEngineBridge.Ignite_Scene_Save();
    }

    private void RegisterEntityRecursive(EntityModel entity)
    {
        _entitiesById[entity.Id] = entity;
        foreach (var child in entity.Children)
        {
            RegisterEntityRecursive(child);
        }
    }

    public IReadOnlyList<EntityModel> GetRootEntities()
    {
        lock (_lock)
        {
            return _rootEntities.ToList();
        }
    }

    public EntityModel? GetEntity(Guid id)
    {
        lock (_lock)
        {
            return _entitiesById.TryGetValue(id, out var entity) ? entity : null;
        }
    }

    public EntityModel CreateEntity(string name, Guid? parentId = null)
    {
        lock (_lock)
        {
            ulong parentUuid = 0;
            if (parentId.HasValue && _entitiesById.TryGetValue(parentId.Value, out var parentEntity))
            {
                parentUuid = parentEntity.Uuid;
            }

            ulong newUuid = NativeEngineBridge.Ignite_Entity_Create(name, parentUuid);
            Guid entityId = newUuid != 0 ? EntityModel.GuidFromUInt64(newUuid) : Guid.NewGuid();

            var entity = new EntityModel
            {
                Id = entityId,
                Uuid = newUuid,
                Name = name,
                ParentId = parentId,
                IsActive = true
            };

            entity.Components.Add(new ComponentModel
            {
                Type = ComponentType.Transform,
                DisplayName = "Transform",
                IsEnabled = true,
                Data = new TransformData()
            });

            _entitiesById[entity.Id] = entity;

            if (parentId.HasValue && _entitiesById.TryGetValue(parentId.Value, out var parent))
            {
                parent.Children.Add(entity);
            }
            else
            {
                _rootEntities.Add(entity);
            }

            return entity;
        }
    }

    public void DeleteEntity(Guid id)
    {
        lock (_lock)
        {
            if (!_entitiesById.TryGetValue(id, out var entity))
                return;

            if (entity.Uuid != 0)
            {
                NativeEngineBridge.Ignite_Entity_Delete(entity.Uuid);
            }

            UnregisterEntityRecursive(entity);

            if (entity.ParentId.HasValue && _entitiesById.TryGetValue(entity.ParentId.Value, out var parent))
            {
                parent.Children.Remove(entity);
            }
            else
            {
                _rootEntities.Remove(entity);
            }
        }
    }

    private void UnregisterEntityRecursive(EntityModel entity)
    {
        _entitiesById.Remove(entity.Id);
        foreach (var child in entity.Children)
        {
            UnregisterEntityRecursive(child);
        }
    }

    public void ReparentEntity(Guid entityId, Guid? newParentId)
    {
        lock (_lock)
        {
            if (!_entitiesById.TryGetValue(entityId, out var entity))
                return;

            if (newParentId.HasValue)
            {
                if (newParentId.Value == entityId) return;

                // Cycle prevention
                var descendants = new HashSet<Guid>();
                CollectDescendants(entity, descendants);
                if (descendants.Contains(newParentId.Value)) return;

                if (!_entitiesById.TryGetValue(newParentId.Value, out var newParent))
                    return;

                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_Reparent(entity.Uuid, newParent.Uuid);
                }

                // Remove from old parent / root
                if (entity.ParentId.HasValue && _entitiesById.TryGetValue(entity.ParentId.Value, out var oldParent))
                    oldParent.Children.Remove(entity);
                else
                    _rootEntities.Remove(entity);

                entity.ParentId = newParentId;
                newParent.Children.Add(entity);
            }
            else
            {
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_Reparent(entity.Uuid, 0);
                }

                // Move to root
                if (entity.ParentId.HasValue && _entitiesById.TryGetValue(entity.ParentId.Value, out var oldParent))
                    oldParent.Children.Remove(entity);

                entity.ParentId = null;
                if (!_rootEntities.Contains(entity))
                    _rootEntities.Add(entity);
            }
        }
    }

    public void RenameEntity(Guid id, string newName)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(id, out var entity))
            {
                entity.Name = newName;
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_Rename(entity.Uuid, newName);
                }
            }
        }
    }

    public EntityModel DuplicateEntity(Guid id)
    {
        lock (_lock)
        {
            if (!_entitiesById.TryGetValue(id, out var original))
                throw new ArgumentException($"Entity with ID {id} not found.", nameof(id));

            if (original.Uuid != 0)
            {
                ulong dupUuid = NativeEngineBridge.Ignite_Entity_Duplicate(original.Uuid);
                if (dupUuid != 0)
                {
                    LoadActiveScene();
                    var found = GetEntity(EntityModel.GuidFromUInt64(dupUuid));
                    if (found != null) return found;
                }
            }

            var copy = DuplicateRecursive(original, original.ParentId, isRoot: true);

            if (copy.ParentId.HasValue && _entitiesById.TryGetValue(copy.ParentId.Value, out var parent))
            {
                parent.Children.Add(copy);
            }
            else
            {
                _rootEntities.Add(copy);
            }

            return copy;
        }
    }

    public void SetEntityTransform(Guid entityId, Vector3 position, Vector3 rotation, Vector3 scale)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var tc = entity.Components.FirstOrDefault(c => c.Type == ComponentType.Transform);
                if (tc?.Data is TransformData td)
                {
                    td.Position = position;
                    td.Rotation = rotation;
                    td.Scale = scale;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetTransform(entity.Uuid,
                        position.X, position.Y, position.Z,
                        rotation.X, rotation.Y, rotation.Z,
                        scale.X, scale.Y, scale.Z);
                }
            }
        }
    }

    public void SetEntitySpriteColor(Guid entityId, Vector4 color)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var sc = entity.Components.FirstOrDefault(c => c.Type == ComponentType.Sprite2D || c.Type == ComponentType.Circle2D);
                if (sc?.Data is SpriteData sd)
                {
                    sd.Color = color;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetSpriteColor(entity.Uuid, color.X, color.Y, color.Z, color.W);
                }
            }
        }
    }

    public void SetEntityActive(Guid entityId, bool active)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                entity.IsActive = active;
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetActive(entity.Uuid, active);
                }
            }
        }
    }

    public void AddComponent(Guid entityId, ComponentType type)
    {
        lock (_lock)
        {
            if (!_entitiesById.TryGetValue(entityId, out var entity))
                return;

            if (entity.Components.Any(c => c.Type == type))
                return;

            int nativeType = MapToNativeCompType(type);
            if (entity.Uuid != 0 && nativeType != 0)
            {
                NativeEngineBridge.Ignite_Entity_AddComponent(entity.Uuid, nativeType);
            }

            var comp = CreateDefaultComponentModel(type);
            entity.Components.Add(comp);
        }
    }

    public void RemoveComponent(Guid entityId, ComponentType type)
    {
        lock (_lock)
        {
            if (!_entitiesById.TryGetValue(entityId, out var entity))
                return;

            if (type == ComponentType.Transform)
                return;

            int nativeType = MapToNativeCompType(type);
            if (entity.Uuid != 0 && nativeType != 0)
            {
                NativeEngineBridge.Ignite_Entity_RemoveComponent(entity.Uuid, nativeType);
            }

            var comp = entity.Components.FirstOrDefault(c => c.Type == type);
            if (comp != null)
            {
                entity.Components.Remove(comp);
            }
        }
    }

    public static int MapToNativeCompType(ComponentType type) => type switch
    {
        ComponentType.Transform => 4,
        ComponentType.Camera => 5,
        ComponentType.Widget => 6,
        ComponentType.Sprite2D => 7,
        ComponentType.Circle2D => 8,
        ComponentType.PointLight2D => 9,
        ComponentType.Text => 10,
        ComponentType.SkeletalMesh => 11,
        ComponentType.StaticMesh => 12,
        ComponentType.DirectionalLight => 13,
        ComponentType.BoxCollider2D => 14,
        ComponentType.CircleCollider2D => 15,
        ComponentType.Rigidbody2D => 16,
        ComponentType.Rigidbody => 17,
        ComponentType.BoxCollider => 19,
        ComponentType.SphereCollider => 20,
        ComponentType.CapsuleCollider => 21,
        ComponentType.MeshCollider => 22,
        ComponentType.AudioSource => 23,
        ComponentType.Script => 24,
        ComponentType.WorldEnvironment => 25,
        ComponentType.Animator2D => 26,
        ComponentType.PointLight => 27,
        ComponentType.SpotLight => 28,
        ComponentType.CharacterController => 29,
        ComponentType.Terrain => 30,
        ComponentType.HeightFieldCollider => 31,
        ComponentType.Prefab => 32,
        _ => 0
    };

    // Scene Lifecycle
    public void PlayScene()
    {
        NativeEngineBridge.Ignite_Scene_Play();
    }

    public void StopScene()
    {
        NativeEngineBridge.Ignite_Scene_Stop();
        LoadActiveScene();
    }

    public void SimulateScene()
    {
        NativeEngineBridge.Ignite_Scene_Simulate();
    }

    public void PauseScene()
    {
        NativeEngineBridge.Ignite_Scene_Pause();
    }

    public void StepScene(int frames = 1)
    {
        NativeEngineBridge.Ignite_Scene_StepFrame(frames);
    }

    public int GetSceneState()
    {
        return NativeEngineBridge.Ignite_Scene_GetState();
    }

    // Component Setters
    public void SetEntityCamera(Guid entityId, bool isPerspective, float fov, float nearPlane, float farPlane, float orthoSize)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.Camera);
                if (comp?.Data is CameraData cd)
                {
                    cd.IsPerspective = isPerspective;
                    cd.FieldOfView = fov;
                    cd.NearClip = nearPlane;
                    cd.FarClip = farPlane;
                    cd.OrthographicSize = orthoSize;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetCamera(entity.Uuid, isPerspective, fov, nearPlane, farPlane, orthoSize);
                }
            }
        }
    }

    public void SetEntityDirectionalLight(Guid entityId, Vector4 color, float intensity, float shadowDistance, bool castShadows)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.DirectionalLight);
                if (comp?.Data is DirectionalLightData dld)
                {
                    dld.Color = color;
                    dld.Intensity = intensity;
                    dld.ShadowDistance = shadowDistance;
                    dld.CastShadows = castShadows;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetDirectionalLight(entity.Uuid, color.X, color.Y, color.Z, color.W, intensity, shadowDistance, castShadows);
                }
            }
        }
    }

    public void SetEntityPointLight(Guid entityId, Vector4 color, float intensity, float range, bool enabled)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.PointLight);
                if (comp?.Data is PointLightData pld)
                {
                    pld.Color = color;
                    pld.Intensity = intensity;
                    pld.Range = range;
                    pld.Enabled = enabled;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetPointLight(entity.Uuid, color.X, color.Y, color.Z, color.W, intensity, range, enabled);
                }
            }
        }
    }

    public void SetEntitySpotLight(Guid entityId, Vector4 color, float intensity, float range, float innerCone, float outerCone, bool enabled)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.SpotLight);
                if (comp?.Data is SpotLightData sld)
                {
                    sld.Color = color;
                    sld.Intensity = intensity;
                    sld.Range = range;
                    sld.InnerCone = innerCone;
                    sld.OuterCone = outerCone;
                    sld.Enabled = enabled;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetSpotLight(entity.Uuid, color.X, color.Y, color.Z, color.W, intensity, range, innerCone, outerCone, enabled);
                }
            }
        }
    }

    public void SetEntityPointLight2D(Guid entityId, Vector4 color, float radius, float intensity, bool enabled)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.PointLight2D);
                if (comp?.Data is PointLight2DData pld)
                {
                    pld.Color = color;
                    pld.Radius = radius;
                    pld.Intensity = intensity;
                    pld.Enabled = enabled;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetPointLight2D(entity.Uuid, color.X, color.Y, color.Z, color.W, radius, intensity, enabled);
                }
            }
        }
    }

    public void SetEntitySprite2D(Guid entityId, Vector4 color, Vector2 tiling, bool flipX, bool flipY)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.Sprite2D);
                if (comp?.Data is SpriteData sd)
                {
                    sd.Color = color;
                    sd.Tiling = tiling;
                    sd.FlipX = flipX;
                    sd.FlipY = flipY;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetSprite2D(entity.Uuid, color.X, color.Y, color.Z, color.W, tiling.X, tiling.Y, flipX, flipY);
                }
            }
        }
    }

    public void SetEntityCircle2D(Guid entityId, Vector4 color, float thickness, float fade)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.Circle2D);
                if (comp?.Data is Circle2DData cd)
                {
                    cd.Color = color;
                    cd.Thickness = thickness;
                    cd.Fade = fade;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetCircle2D(entity.Uuid, color.X, color.Y, color.Z, color.W, thickness, fade);
                }
            }
        }
    }

    public void SetEntityRigidbody(Guid entityId, int bodyType, float mass, float linearDamping, float angularDamping, float friction, float restitution, bool useGravity)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.Rigidbody);
                if (comp?.Data is RigidbodyData rd)
                {
                    rd.BodyType = bodyType;
                    rd.Mass = mass;
                    rd.LinearDamping = linearDamping;
                    rd.AngularDamping = angularDamping;
                    rd.Friction = friction;
                    rd.Restitution = restitution;
                    rd.UseGravity = useGravity;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetRigidbody(entity.Uuid, bodyType, mass, linearDamping, angularDamping, friction, restitution, useGravity);
                }
            }
        }
    }

    public void SetEntityRigidbody2D(Guid entityId, int bodyType, float gravityScale, float linearDamping, float angularDamping, bool fixedRotation, bool isAwake, bool isEnabled)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.Rigidbody2D);
                if (comp?.Data is Rigidbody2DData rd)
                {
                    rd.BodyType = bodyType;
                    rd.GravityScale = gravityScale;
                    rd.LinearDamping = linearDamping;
                    rd.AngularDamping = angularDamping;
                    rd.FixedRotation = fixedRotation;
                    rd.IsAwake = isAwake;
                    rd.IsEnabled = isEnabled;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetRigidbody2D(entity.Uuid, bodyType, gravityScale, linearDamping, angularDamping, fixedRotation, isAwake, isEnabled);
                }
            }
        }
    }

    public void SetEntityBoxCollider(Guid entityId, Vector3 center, Vector3 size)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.BoxCollider);
                if (comp?.Data is BoxColliderData bcd)
                {
                    bcd.Center = center;
                    bcd.Size = size;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetBoxCollider(entity.Uuid, center.X, center.Y, center.Z, size.X, size.Y, size.Z);
                }
            }
        }
    }

    public void SetEntitySphereCollider(Guid entityId, Vector3 center, float radius)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.SphereCollider);
                if (comp?.Data is SphereColliderData scd)
                {
                    scd.Center = center;
                    scd.Radius = radius;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetSphereCollider(entity.Uuid, center.X, center.Y, center.Z, radius);
                }
            }
        }
    }

    public void SetEntityCapsuleCollider(Guid entityId, Vector3 center, float radius, float height)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.CapsuleCollider);
                if (comp?.Data is CapsuleColliderData ccd)
                {
                    ccd.Center = center;
                    ccd.Radius = radius;
                    ccd.Height = height;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetCapsuleCollider(entity.Uuid, center.X, center.Y, center.Z, radius, height);
                }
            }
        }
    }

    public void SetEntityBoxCollider2D(Guid entityId, Vector2 offset, Vector2 size, float density, float friction, float restitution, bool isSensor)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.BoxCollider2D);
                if (comp?.Data is BoxCollider2DData bcd)
                {
                    bcd.Offset = offset;
                    bcd.Size = size;
                    bcd.Density = density;
                    bcd.Friction = friction;
                    bcd.Restitution = restitution;
                    bcd.IsSensor = isSensor;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetBoxCollider2D(entity.Uuid, offset.X, offset.Y, size.X, size.Y, density, friction, restitution, isSensor);
                }
            }
        }
    }

    public void SetEntityCircleCollider2D(Guid entityId, Vector2 center, float radius, float density, float friction, float restitution, bool isSensor)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.CircleCollider2D);
                if (comp?.Data is CircleCollider2DData ccd)
                {
                    ccd.Center = center;
                    ccd.Radius = radius;
                    ccd.Density = density;
                    ccd.Friction = friction;
                    ccd.Restitution = restitution;
                    ccd.IsSensor = isSensor;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetCircleCollider2D(entity.Uuid, center.X, center.Y, radius, density, friction, restitution, isSensor);
                }
            }
        }
    }

    public void SetEntityCharacterController(Guid entityId, float radius, float height, float stepHeight, float slopeAngle, float mass, float friction)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.CharacterController);
                if (comp?.Data is CharacterControllerData ccd)
                {
                    ccd.Radius = radius;
                    ccd.Height = height;
                    ccd.MaxStepHeight = stepHeight;
                    ccd.MaxSlopeAngle = slopeAngle;
                    ccd.Mass = mass;
                    ccd.Friction = friction;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetCharacterController(entity.Uuid, radius, height, stepHeight, slopeAngle, mass, friction);
                }
            }
        }
    }

    public void SetEntityAudioSource(Guid entityId, float volume, float pitch, float pan, bool playOnStart, bool loop)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.AudioSource);
                if (comp?.Data is AudioSourceData asd)
                {
                    asd.Volume = volume;
                    asd.Pitch = pitch;
                    asd.Pan = pan;
                    asd.PlayOnStart = playOnStart;
                    asd.Loop = loop;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetAudioSource(entity.Uuid, volume, pitch, pan, playOnStart, loop);
                }
            }
        }
    }

    public void SetEntityText(Guid entityId, string text, Vector4 color, float kerning, float lineSpacing, bool screenSpace)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.Text);
                if (comp?.Data is TextData td)
                {
                    td.Text = text;
                    td.Color = color;
                    td.Kerning = kerning;
                    td.LineSpacing = lineSpacing;
                    td.ScreenSpace = screenSpace;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetText(entity.Uuid, text, color.X, color.Y, color.Z, color.W, kerning, lineSpacing, screenSpace);
                }
            }
        }
    }

    public void SetEntityWorldEnvironment(Guid entityId, float exposure, float gamma, float ambient, float fogDensity, Vector4 fogColor, float fogStart, float fogEnd)
    {
        lock (_lock)
        {
            if (_entitiesById.TryGetValue(entityId, out var entity))
            {
                var comp = entity.Components.FirstOrDefault(c => c.Type == ComponentType.WorldEnvironment);
                if (comp?.Data is WorldEnvironmentData wed)
                {
                    wed.Exposure = exposure;
                    wed.Gamma = gamma;
                    wed.Ambient = ambient;
                    wed.FogDensity = fogDensity;
                    wed.FogColor = fogColor;
                    wed.FogStart = fogStart;
                    wed.FogEnd = fogEnd;
                }
                if (entity.Uuid != 0)
                {
                    NativeEngineBridge.Ignite_Entity_SetWorldEnvironment(entity.Uuid, exposure, gamma, ambient, fogDensity, fogColor.X, fogColor.Y, fogColor.Z, fogColor.W, fogStart, fogEnd);
                }
            }
        }
    }

    public static ComponentModel CreateDefaultComponentModel(ComponentType type)
    {
        return type switch
        {
            ComponentType.Transform => new ComponentModel { Type = type, DisplayName = "Transform", Data = new TransformData() },
            ComponentType.Camera => new ComponentModel { Type = type, DisplayName = "Camera", Data = new CameraData() },
            ComponentType.Sprite2D => new ComponentModel { Type = type, DisplayName = "Sprite 2D", Data = new SpriteData { Color = new Vector4(1, 1, 1, 1), Tiling = new Vector2(1, 1) } },
            ComponentType.Circle2D => new ComponentModel { Type = type, DisplayName = "Circle 2D", Data = new Circle2DData { Color = new Vector4(1, 1, 1, 1), Thickness = 1.0f, Fade = 0.005f } },
            ComponentType.DirectionalLight => new ComponentModel { Type = type, DisplayName = "Directional Light", Data = new DirectionalLightData() },
            ComponentType.PointLight => new ComponentModel { Type = type, DisplayName = "Point Light", Data = new PointLightData() },
            ComponentType.SpotLight => new ComponentModel { Type = type, DisplayName = "Spot Light", Data = new SpotLightData() },
            ComponentType.PointLight2D => new ComponentModel { Type = type, DisplayName = "Point Light 2D", Data = new PointLight2DData() },
            ComponentType.StaticMesh => new ComponentModel { Type = type, DisplayName = "Static Mesh" },
            ComponentType.SkeletalMesh => new ComponentModel { Type = type, DisplayName = "Skeletal Mesh" },
            ComponentType.Rigidbody => new ComponentModel { Type = type, DisplayName = "Rigid Body", Data = new RigidbodyData() },
            ComponentType.Rigidbody2D => new ComponentModel { Type = type, DisplayName = "Rigid Body 2D", Data = new Rigidbody2DData() },
            ComponentType.BoxCollider => new ComponentModel { Type = type, DisplayName = "Box Collider", Data = new BoxColliderData() },
            ComponentType.BoxCollider2D => new ComponentModel { Type = type, DisplayName = "Box Collider 2D", Data = new BoxCollider2DData() },
            ComponentType.SphereCollider => new ComponentModel { Type = type, DisplayName = "Sphere Collider", Data = new SphereColliderData() },
            ComponentType.CapsuleCollider => new ComponentModel { Type = type, DisplayName = "Capsule Collider", Data = new CapsuleColliderData() },
            ComponentType.CircleCollider2D => new ComponentModel { Type = type, DisplayName = "Circle Collider 2D", Data = new CircleCollider2DData() },
            ComponentType.AudioSource => new ComponentModel { Type = type, DisplayName = "Audio Source", Data = new AudioSourceData() },
            ComponentType.WorldEnvironment => new ComponentModel { Type = type, DisplayName = "World Environment", Data = new WorldEnvironmentData() },
            ComponentType.CharacterController => new ComponentModel { Type = type, DisplayName = "Character Controller", Data = new CharacterControllerData() },
            ComponentType.Text => new ComponentModel { Type = type, DisplayName = "Text", Data = new TextData() },
            ComponentType.Script => new ComponentModel { Type = type, DisplayName = "Script", Data = new ScriptData() },
            _ => new ComponentModel { Type = type, DisplayName = type.ToString() }
        };
    }

    private EntityModel DuplicateRecursive(EntityModel source, Guid? newParentId, bool isRoot)
    {
        var copy = new EntityModel
        {
            Id = Guid.NewGuid(),
            Name = isRoot ? $"{source.Name} (Copy)" : source.Name,
            ParentId = newParentId,
            IsActive = source.IsActive
        };

        foreach (var comp in source.Components)
        {
            copy.Components.Add(new ComponentModel
            {
                Type = comp.Type,
                DisplayName = comp.DisplayName,
                IsEnabled = comp.IsEnabled,
                Data = comp.Data
            });
        }

        _entitiesById[copy.Id] = copy;

        foreach (var child in source.Children)
        {
            var childCopy = DuplicateRecursive(child, copy.Id, isRoot: false);
            copy.Children.Add(childCopy);
        }

        return copy;
    }

    private static void CollectDescendants(EntityModel parent, HashSet<Guid> ids)
    {
        foreach (var child in parent.Children)
        {
            ids.Add(child.Id);
            CollectDescendants(child, ids);
        }
    }
}
