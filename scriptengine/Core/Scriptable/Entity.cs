// Copyright (c) 2026 Evangelion Manuhutu

using System;

using Ignite.Core.Component;
namespace Ignite;

public class Entity : ScriptableObject
{
    internal Entity(ulong id) : base(id) { }
    protected Entity() : base(0) { }

    public Vector3 Translation
    {
        get
        {
            ComponentInternalCalls.TransformComponent_GetTranslation(ID, out Vector3 translation);
            return translation;
        }
        set => ComponentInternalCalls.TransformComponent_SetTranslation(ID, value);
    }

    public Quaternion Rotation
    {
        get
        {
            ComponentInternalCalls.TransformComponent_GetRotation(ID, out Quaternion quat);
            return quat;
        }
        set => ComponentInternalCalls.TransformComponent_SetRotation(ID, value);
    }

    public Vector3 Scale
    {
        get
        {
            ComponentInternalCalls.TransformComponent_GetScale(ID, out Vector3 scale);
            return scale;
        }
        set => ComponentInternalCalls.TransformComponent_SetScale(ID, value);
    }

    public bool Visible
    {
        get
        {
            ComponentInternalCalls.Entity_GetVisibility(ID, out bool result);
            return result;
        }

        set => ComponentInternalCalls.Entity_SetVisibility(ID, value);
    }

    public void Destroy()
    {
        ComponentInternalCalls.Entity_Destroy(ID);
    }

    public T As<T>() where T : Entity, new()
    {
        object instance = ComponentInternalCalls.GetScriptInstance(ID);
        return instance as T;
    }

    public T AddComponent<T>() where T : IComponent, new()
    {
        if (HasComponent<T>())
            return GetComponent<T>();

        T component = new() { Entity = this };
        Type componentType = typeof(T);
        ComponentInternalCalls.Entity_AddComponent(ID, componentType);
        return component;
    }

    public bool HasComponent<T>() where T : IComponent, new()
    {
        Type componentType = typeof(T);
        return ComponentInternalCalls.Entity_HasComponent(ID, componentType);
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
        return ComponentInternalCalls.Entity_GetName(ID);
    }

    // Instantiate function
    public static Entity Instantiate(string name, Vector3 translation)
    {
        ulong entityID = ComponentInternalCalls.Entity_Instantiate(name, translation);
        return new Entity(entityID);
    }

    public static Entity Instantiate(Entity entity)
    {
        ulong entityID = ComponentInternalCalls.Entity_Instantiate(entity.ID, entity.Translation);
        return new Entity(entityID);
    }

    public static Entity Instantiate(Entity entity, Vector3 translation)
    {
        ulong entityID = ComponentInternalCalls.Entity_Instantiate(entity.ID, translation);
        return new Entity(entityID);
    }

    public static void Destroy(Entity entity)
    {
        ComponentInternalCalls.Entity_Destroy(entity.ID);
    }

    public static Entity FindEntity(string name)
    {
        ulong entityID = ComponentInternalCalls.Entity_FindEntity(name);
        if (entityID == 0)
            return null;

        return new Entity(entityID);
    }

    public Entity FindChild(string childName)
    {
        ulong entityID = ComponentInternalCalls.Entity_FindChildEntity(ID, childName);
        if (entityID == 0)
            return null;
        return new Entity(entityID);
    }

    public bool IsParent(Entity entity)
    {
        return ComponentInternalCalls.Entity_IsParent(ID, entity.ID);
    }

    public Entity GetParent()
    {
        ulong parentID = ComponentInternalCalls.Entity_GetParent(ID);
        return new Entity(parentID);
    }

    public static Entity PickEntityAt(float x, float y)
    {
        ulong entityID = ComponentInternalCalls.Scene_PickEntityAt(x, y);
        if (entityID == 0)
            return null;

        return new Entity(entityID);
    }

}
