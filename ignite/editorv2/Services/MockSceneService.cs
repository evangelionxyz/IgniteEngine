using System;
using System.Collections.Generic;
using System.Linq;
using IgniteEditor.Models;

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
}
