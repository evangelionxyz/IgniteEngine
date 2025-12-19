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

﻿using System;

namespace IgniteEngine;

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

    public T AddComponent<T>() where T : Component, new()
    {
        if (HasComponent<T>())
            return GetComponent<T>();

        T component = new T() { Entity = this };
        Type componentType = typeof(T);
        InternalCalls.Entity_AddComponent(ID, componentType);
        return component;
    }

    public bool HasComponent<T>() where T : Component, new()
    {
        Type componentType = typeof(T);
        return InternalCalls.Entity_HasComponent(ID, componentType);
    }

    public T GetComponent<T>() where T : Component, new()
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
