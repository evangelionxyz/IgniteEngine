using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;

using Ignite.Managed.Services;
using Ignite.Managed.Models;

namespace IgniteEditor.Services;

public class MockSceneService : ISceneService
{
    private readonly List<EntityModel> _entities = new();
    private readonly object _lock = new();

    public MockSceneService()
    {
        InitializeSampleScene();
    }

    private void InitializeSampleScene()
    {
        // 1. Main Camera (Camera + Transform)
        CreateEntityInternal("Main Camera", null,
            ComponentType.Transform,
            ComponentType.Camera);

        // 2. Directional Light (DirectionalLight + Transform)
        CreateEntityInternal("Directional Light", null,
            ComponentType.Transform,
            ComponentType.DirectionalLight);

        // 3. Player (StaticMesh + Transform + Rigidbody + BoxCollider + Script)
        var player = CreateEntityInternal("Player", null,
            ComponentType.Transform,
            ComponentType.StaticMesh,
            ComponentType.Rigidbody,
            ComponentType.BoxCollider,
            ComponentType.Script);

        // 3a. PlayerModel child (SkeletalMesh + Transform)
        CreateEntityInternal("PlayerModel", player.Id,
            ComponentType.Transform,
            ComponentType.SkeletalMesh);

        // 3b. Weapon child (StaticMesh + Transform)
        CreateEntityInternal("Weapon", player.Id,
            ComponentType.Transform,
            ComponentType.StaticMesh);

        // 4. Ground (StaticMesh + Transform + BoxCollider)
        CreateEntityInternal("Ground", null,
            ComponentType.Transform,
            ComponentType.StaticMesh,
            ComponentType.BoxCollider);

        // 5. Environment folder (empty entity with children: Tree1, Tree2, Rock1 - each StaticMesh + Transform)
        var environment = CreateEntityInternal("Environment", null,
            ComponentType.Transform);

        // 5a. Tree1 (StaticMesh + Transform)
        CreateEntityInternal("Tree1", environment.Id,
            ComponentType.Transform,
            ComponentType.StaticMesh);

        // 5b. Tree2 (StaticMesh + Transform)
        CreateEntityInternal("Tree2", environment.Id,
            ComponentType.Transform,
            ComponentType.StaticMesh);

        // 5c. Rock1 (StaticMesh + Transform)
        CreateEntityInternal("Rock1", environment.Id,
            ComponentType.Transform,
            ComponentType.StaticMesh);

        RebuildHierarchy();
    }

    private EntityModel CreateEntityInternal(string name, Guid? parentId, params ComponentType[] componentTypes)
    {
        var entity = new EntityModel
        {
            Id = Guid.NewGuid(),
            Name = name,
            ParentId = parentId,
            IsActive = true
        };

        var types = componentTypes.ToList();
        if (!types.Contains(ComponentType.Transform))
        {
            types.Insert(0, ComponentType.Transform);
        }
        else
        {
            types.Remove(ComponentType.Transform);
            types.Insert(0, ComponentType.Transform);
        }

        foreach (var type in types)
        {
            entity.Components.Add(new ComponentModel
            {
                Type = type,
                DisplayName = type.ToString(),
                IsEnabled = true
            });
        }

        _entities.Add(entity);
        return entity;
    }

    private void RebuildHierarchy()
    {
        var dict = _entities.ToDictionary(e => e.Id);

        foreach (var entity in _entities)
        {
            entity.Children.Clear();
        }

        foreach (var entity in _entities)
        {
            if (entity.ParentId.HasValue && dict.TryGetValue(entity.ParentId.Value, out var parent))
            {
                parent.Children.Add(entity);
            }
        }
    }

    public IReadOnlyList<EntityModel> GetRootEntities()
    {
        lock (_lock)
        {
            return _entities.Where(e => e.ParentId == null).ToList();
        }
    }

    public EntityModel? GetEntity(Guid id)
    {
        lock (_lock)
        {
            return _entities.FirstOrDefault(e => e.Id == id);
        }
    }

    public EntityModel CreateEntity(string name, Guid? parentId = null)
    {
        lock (_lock)
        {
            var entity = CreateEntityInternal(name, parentId, ComponentType.Transform);
            RebuildHierarchy();
            return entity;
        }
    }

    public void DeleteEntity(Guid id)
    {
        lock (_lock)
        {
            var entity = _entities.FirstOrDefault(e => e.Id == id);
            if (entity == null) return;

            var toRemove = new HashSet<Guid>();
            CollectDescendantIds(entity, toRemove);
            toRemove.Add(id);

            _entities.RemoveAll(e => toRemove.Contains(e.Id));
            RebuildHierarchy();
        }
    }

    private static void CollectDescendantIds(EntityModel parent, HashSet<Guid> ids)
    {
        foreach (var child in parent.Children)
        {
            ids.Add(child.Id);
            CollectDescendantIds(child, ids);
        }
    }

    public void ReparentEntity(Guid entityId, Guid? newParentId)
    {
        lock (_lock)
        {
            var entity = _entities.FirstOrDefault(e => e.Id == entityId);
            if (entity == null) return;

            if (newParentId.HasValue)
            {
                if (newParentId.Value == entityId) return;

                // Prevent cycles: new parent cannot be a descendant of entity
                var descendants = new HashSet<Guid>();
                CollectDescendantIds(entity, descendants);
                if (descendants.Contains(newParentId.Value)) return;

                var newParent = _entities.FirstOrDefault(e => e.Id == newParentId.Value);
                if (newParent == null) return;
            }

            entity.ParentId = newParentId;
            RebuildHierarchy();
        }
    }

    public void RenameEntity(Guid id, string newName)
    {
        lock (_lock)
        {
            var entity = _entities.FirstOrDefault(e => e.Id == id);
            if (entity != null)
            {
                entity.Name = newName;
            }
        }
    }

    public EntityModel DuplicateEntity(Guid id)
    {
        lock (_lock)
        {
            var original = _entities.FirstOrDefault(e => e.Id == id);
            if (original == null)
            {
                throw new ArgumentException($"Entity with ID {id} not found.", nameof(id));
            }

            var duplicate = DuplicateEntityRecursive(original, original.ParentId, isRoot: true);
            RebuildHierarchy();
            return duplicate;
        }
    }

    private EntityModel DuplicateEntityRecursive(EntityModel source, Guid? newParentId, bool isRoot)
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
                IsEnabled = comp.IsEnabled
            });
        }

        _entities.Add(copy);

        foreach (var child in source.Children)
        {
            DuplicateEntityRecursive(child, copy.Id, isRoot: false);
        }

        return copy;
    }

    public bool NewScene()
    {
        return false;
    }

    public void LoadActiveScene()
    {
        lock (_lock)
        {
            _entities.Clear();
            InitializeSampleScene();
        }
    }

    public bool LoadScene(string filepath)
    {
        LoadActiveScene();
        return true;
    }

    public bool SaveActiveScene()
    {
        return true;
    }

    public void SetEntityTransform(Guid entityId, Vector3 position, Vector3 rotation, Vector3 scale)
    {
        var entity = GetEntity(entityId);
        var tc = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.Transform);
        if (tc?.Data is TransformData td)
        {
            td.Position = position;
            td.Rotation = rotation;
            td.Scale = scale;
        }
    }

    public void SetEntitySpriteColor(Guid entityId, Vector4 color)
    {
        var entity = GetEntity(entityId);
        var sc = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.Sprite2D || c.Type == ComponentType.Circle2D);
        if (sc?.Data is SpriteData sd)
        {
            sd.Color = color;
        }
    }

    public void SetEntityActive(Guid entityId, bool active)
    {
        var entity = GetEntity(entityId);
        if (entity != null)
        {
            entity.IsActive = active;
        }
    }

    public void AddComponent(Guid entityId, ComponentType type)
    {
        var entity = GetEntity(entityId);
        if (entity != null && !entity.Components.Any(c => c.Type == type))
        {
            entity.Components.Add(EngineSceneService.CreateDefaultComponentModel(type));
        }
    }

    public void RemoveComponent(Guid entityId, ComponentType type)
    {
        var entity = GetEntity(entityId);
        if (entity != null && type != ComponentType.Transform)
        {
            var comp = entity.Components.FirstOrDefault(c => c.Type == type);
            if (comp != null) entity.Components.Remove(comp);
        }
    }

    private int _mockSceneState = 0; // 0=Stopped, 1=Play, 2=Simulate, 3=Paused

    public void PlayScene()
    {
        _mockSceneState = 1;
    }

    public void StopScene()
    {
        _mockSceneState = 0;
    }

    public void SimulateScene()
    {
        _mockSceneState = 2;
    }

    public void PauseScene()
    {
        if (_mockSceneState == 1 || _mockSceneState == 2)
            _mockSceneState = 3;
        else if (_mockSceneState == 3)
            _mockSceneState = 1;
    }

    public void StepScene(int frames = 1)
    {
    }

    public int GetSceneState()
    {
        return _mockSceneState;
    }

    public void SetEntityCamera(Guid entityId, bool isPerspective, float fov, float nearPlane, float farPlane, float orthoSize)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.Camera);
        if (comp?.Data is CameraData cd)
        {
            cd.IsPerspective = isPerspective;
            cd.FieldOfView = fov;
            cd.NearClip = nearPlane;
            cd.FarClip = farPlane;
            cd.OrthographicSize = orthoSize;
        }
    }

    public void SetEntityDirectionalLight(Guid entityId, Vector4 color, float intensity, float shadowDistance, bool castShadows)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.DirectionalLight);
        if (comp?.Data is DirectionalLightData dld)
        {
            dld.Color = color;
            dld.Intensity = intensity;
            dld.ShadowDistance = shadowDistance;
            dld.CastShadows = castShadows;
        }
    }

    public void SetEntityPointLight(Guid entityId, Vector4 color, float intensity, float range, bool enabled)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.PointLight);
        if (comp?.Data is PointLightData pld)
        {
            pld.Color = color;
            pld.Intensity = intensity;
            pld.Range = range;
            pld.Enabled = enabled;
        }
    }

    public void SetEntitySpotLight(Guid entityId, Vector4 color, float intensity, float range, float innerCone, float outerCone, bool enabled)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.SpotLight);
        if (comp?.Data is SpotLightData sld)
        {
            sld.Color = color;
            sld.Intensity = intensity;
            sld.Range = range;
            sld.InnerCone = innerCone;
            sld.OuterCone = outerCone;
            sld.Enabled = enabled;
        }
    }

    public void SetEntityPointLight2D(Guid entityId, Vector4 color, float radius, float intensity, bool enabled)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.PointLight2D);
        if (comp?.Data is PointLight2DData pld)
        {
            pld.Color = color;
            pld.Radius = radius;
            pld.Intensity = intensity;
            pld.Enabled = enabled;
        }
    }

    public void SetEntitySprite2D(Guid entityId, Vector4 color, Vector2 tiling, bool flipX, bool flipY)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.Sprite2D);
        if (comp?.Data is SpriteData sd)
        {
            sd.Color = color;
            sd.Tiling = tiling;
            sd.FlipX = flipX;
            sd.FlipY = flipY;
        }
    }

    public void SetEntityCircle2D(Guid entityId, Vector4 color, float thickness, float fade)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.Circle2D);
        if (comp?.Data is Circle2DData cd)
        {
            cd.Color = color;
            cd.Thickness = thickness;
            cd.Fade = fade;
        }
    }

    public void SetEntityRigidbody(Guid entityId, int bodyType, float mass, float linearDamping, float angularDamping, float friction, float restitution, bool useGravity)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.Rigidbody);
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
    }

    public void SetEntityRigidbody2D(Guid entityId, int bodyType, float gravityScale, float linearDamping, float angularDamping, bool fixedRotation, bool isAwake, bool isEnabled)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.Rigidbody2D);
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
    }

    public void SetEntityBoxCollider(Guid entityId, Vector3 center, Vector3 size)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.BoxCollider);
        if (comp?.Data is BoxColliderData bcd)
        {
            bcd.Center = center;
            bcd.Size = size;
        }
    }

    public void SetEntitySphereCollider(Guid entityId, Vector3 center, float radius)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.SphereCollider);
        if (comp?.Data is SphereColliderData scd)
        {
            scd.Center = center;
            scd.Radius = radius;
        }
    }

    public void SetEntityCapsuleCollider(Guid entityId, Vector3 center, float radius, float height)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.CapsuleCollider);
        if (comp?.Data is CapsuleColliderData ccd)
        {
            ccd.Center = center;
            ccd.Radius = radius;
            ccd.Height = height;
        }
    }

    public void SetEntityBoxCollider2D(Guid entityId, Vector2 offset, Vector2 size, float density, float friction, float restitution, bool isSensor)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.BoxCollider2D);
        if (comp?.Data is BoxCollider2DData bcd)
        {
            bcd.Offset = offset;
            bcd.Size = size;
            bcd.Density = density;
            bcd.Friction = friction;
            bcd.Restitution = restitution;
            bcd.IsSensor = isSensor;
        }
    }

    public void SetEntityCircleCollider2D(Guid entityId, Vector2 center, float radius, float density, float friction, float restitution, bool isSensor)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.CircleCollider2D);
        if (comp?.Data is CircleCollider2DData ccd)
        {
            ccd.Center = center;
            ccd.Radius = radius;
            ccd.Density = density;
            ccd.Friction = friction;
            ccd.Restitution = restitution;
            ccd.IsSensor = isSensor;
        }
    }

    public void SetEntityCharacterController(Guid entityId, float radius, float height, float stepHeight, float slopeAngle, float mass, float friction)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.CharacterController);
        if (comp?.Data is CharacterControllerData ccd)
        {
            ccd.Radius = radius;
            ccd.Height = height;
            ccd.MaxStepHeight = stepHeight;
            ccd.MaxSlopeAngle = slopeAngle;
            ccd.Mass = mass;
            ccd.Friction = friction;
        }
    }

    public void SetEntityAudioSource(Guid entityId, float volume, float pitch, float pan, bool playOnStart, bool loop)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.AudioSource);
        if (comp?.Data is AudioSourceData asd)
        {
            asd.Volume = volume;
            asd.Pitch = pitch;
            asd.Pan = pan;
            asd.PlayOnStart = playOnStart;
            asd.Loop = loop;
        }
    }

    public void SetEntityText(Guid entityId, string text, Vector4 color, float kerning, float lineSpacing, bool screenSpace)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.Text);
        if (comp?.Data is TextData td)
        {
            td.Text = text;
            td.Color = color;
            td.Kerning = kerning;
            td.LineSpacing = lineSpacing;
            td.ScreenSpace = screenSpace;
        }
    }

    public void SetEntityWorldEnvironment(Guid entityId, float exposure, float gamma, float ambient, float fogDensity, Vector4 fogColor, float fogStart, float fogEnd)
    {
        var entity = GetEntity(entityId);
        var comp = entity?.Components.FirstOrDefault(c => c.Type == ComponentType.WorldEnvironment);
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
    }
}
