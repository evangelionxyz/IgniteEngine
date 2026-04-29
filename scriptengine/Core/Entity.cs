// Copyright (c) 2026 Evangelion Manuhutu

using System;

using Ignite.Core;
namespace Ignite;

public class Entity : IEquatable<Entity>
{
    public ulong ID { get; internal set; }
    
    internal Entity(ulong id) { ID = id; }
    protected Entity() { ID = 0; }

    public virtual void OnCreate() { }
    public virtual void OnUpdate(float deltaTime) { }
    public virtual void OnDestroy() { }

    internal void SetID(ulong id) => ID = id;

    public bool Equals(Entity other)
    {
        if (ReferenceEquals(other, null))
            return false;

        if (ReferenceEquals(this, other))
            return true;

        return ID == other.ID;
    }
    public override bool Equals(object obj) => obj is Entity entity && Equals(entity);
    public override int GetHashCode() => ID.GetHashCode();
    public override string ToString() => $"{typeof(Entity)} {GetName()} {ID}";

    public static bool operator == (Entity left, Entity right)
    {
        if (ReferenceEquals(left, right))
            return true;

        if (ReferenceEquals(left, null) || ReferenceEquals(right, null))
            return false;

        return left.ID == right.ID;
    }

    public static bool operator != (Entity left, Entity right) => !(left == right);

    public static implicit operator ulong(Entity entity) => entity?.ID ?? 0UL;

    public Vector3 Translation
    {
        get
        {
            InternalCalls.TransformComponent_GetTranslation(ID, out Vector3 translation);
            return translation;
        }
        set => InternalCalls.TransformComponent_SetTranslation(ID, value);
    }

    public Quaternion Rotation
    {
        get
        {
            InternalCalls.TransformComponent_GetRotation(ID, out Quaternion quat);
            return quat;
        }
        set => InternalCalls.TransformComponent_SetRotation(ID, value);
    }

    public Vector3 Scale
    {
        get
        {
            InternalCalls.TransformComponent_GetScale(ID, out Vector3 scale);
            return scale;
        }
        set => InternalCalls.TransformComponent_SetScale(ID, value);
    }

    public bool Visible
    {
        get
        {
            InternalCalls.Entity_GetVisibility(ID, out bool result);
            return result;
        }

        set => InternalCalls.Entity_SetVisibility(ID, value);
    }

    public void Destroy()
    {
        InternalCalls.Entity_Destroy(ID);
    }

    public T As<T>() where T : Entity, new()
    {
        object instance = InternalCalls.GetScriptInstance(ID);
        return instance as T;
    }

    public T AddComponent<T>() where T : IComponent, new()
    {
        if (HasComponent<T>())
            return GetComponent<T>();

        T component = new() { Entity = this };
        Type componentType = typeof(T);
        InternalCalls.Entity_AddComponent(ID, componentType);
        return component;
    }

    public bool HasComponent<T>() where T : IComponent, new()
    {
        Type componentType = typeof(T);
        return InternalCalls.Entity_HasComponent(ID, componentType);
    }

    public T GetComponent<T>() where T : IComponent, new()
    {
        if (!HasComponent<T>())
            return AddComponent<T>();

        T component = new() { Entity = this };
        return component;
    }

    public string GetName()
    {
        return InternalCalls.Entity_GetName(ID);
    }

    // Instantiate function
    public static Entity Instantiate(string name, Vector3 translation)
    {
        ulong entityID = InternalCalls.Entity_Instantiate(name, translation);
        return new Entity(entityID);
    }

    public static Entity Instantiate(Entity entity)
    {
        ulong entityID = InternalCalls.Entity_Instantiate(entity.ID, entity.Translation);
        return new Entity(entityID);
    }

    public static Entity Instantiate(Entity entity, Vector3 translation)
    {
        ulong entityID = InternalCalls.Entity_Instantiate(entity.ID, translation);
        return new Entity(entityID);
    }

    public static void Destroy(Entity entity)
    {
        InternalCalls.Entity_Destroy(entity.ID);
    }

    public static Entity FindEntity(string name)
    {
        ulong entityID = InternalCalls.Entity_FindEntity(name);
        if (entityID == 0)
            return null;

        return new Entity(entityID);
    }

    public Entity FindChild(string childName)
    {
        ulong entityID = InternalCalls.Entity_FindChildEntity(ID, childName);
        if (entityID == 0)
            return null;
        return new Entity(entityID);
    }

    public bool IsParent(Entity entity)
    {
        return InternalCalls.Entity_IsParent(ID, entity.ID);
    }

    public Entity GetParent()
    {
        ulong parentID = InternalCalls.Entity_GetParent(ID);
        return new Entity(parentID);
    }

    public static Entity PickEntityAt(float x, float y)
    {
        ulong entityID = InternalCalls.Scene_PickEntityAt(x, y);
        if (entityID == 0)
            return null;

        return new Entity(entityID);
    }

}
