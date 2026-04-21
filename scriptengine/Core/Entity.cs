// Copyright (c) 2026 Evangelion Manuhutu

using System;

namespace Ignite;

public class Entity
{
    internal Entity(ulong id)
    {
        ID = id;
    }

    public ulong ID { get; internal set; }
    protected Entity() { ID = 0; }

    internal void SetID(ulong id)
    {
        ID = id;
    }

    public virtual void OnCreate() { }
    public virtual void OnUpdate(float deltaTime) { }
    public virtual void OnDestroy() { }

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

    public T AddComponent<T>() where T : IComponent, new()
    {
        if (HasComponent<T>())
            return GetComponent<T>();

        T component = new T() { Entity = this };
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

        T component = new T() { Entity = this };
        return component;
    }

    public Entity FindEntityByName(string name)
    {
        ulong entityID = InternalCalls.Entity_FindEntityByName(name);
        if (entityID == 0)
            return null;

        return new Entity(entityID);
    }

    public string GetName()
    {
        return InternalCalls.Entity_GetName(ID);
    }

    public Entity Instantiate(Entity entity)
    {
        ulong entityID = InternalCalls.Entity_Instantiate(entity.ID, entity.Translation);
        return new Entity(entityID);
    }

    public Entity Instantiate(Entity entity, Vector3 translation)
    {
        ulong entityID = InternalCalls.Entity_Instantiate(entity.ID, translation);
        Entity newEntity = new Entity(entityID);
        newEntity.Translation = new Vector3(translation);

        return newEntity;
    }

    public void Destroy(Entity entity)
    {
        InternalCalls.Entity_Destroy(entity.ID);
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

    public Entity PickEntityAt(float x, float y)
    {
        ulong entityID = InternalCalls.Scene_PickEntityAt(x, y);
        if (entityID == 0)
            return null;

        return new Entity(entityID);
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
}
