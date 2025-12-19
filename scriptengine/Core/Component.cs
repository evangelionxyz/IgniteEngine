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

public abstract class Component
{
    public Entity Entity { get; internal set; }
}

public class Transform : Component
{
    public Vector3 Forward
    {
        get
        {
            InternalCalls.TransformComponent_GetForward(Entity.ID, out Vector3 forward);
            return forward;
        }

        set
        {
            InternalCalls.TransformComponent_SetForward(Entity.ID, value);
        }
    }

    public Vector3 Right
    {
        get
        {
            InternalCalls.TransformComponent_GetRight(Entity.ID, out Vector3 right);
            return right;
        }

        set
        {
            InternalCalls.TransformComponent_SetRight(Entity.ID, value);
        }
    }

    public Vector3 Up
    {
        get
        {
            InternalCalls.TransformComponent_GetUp(Entity.ID, out Vector3 up);
            return up;
        }

        set
        {
            InternalCalls.TransformComponent_SetUp(Entity.ID, value);
        }
    }

    public Vector3 Translation
    {
        get
        {
            InternalCalls.TransformComponent_GetTranslation(Entity.ID, out Vector3 translation);
            return translation;
        }
        set
        {
            InternalCalls.TransformComponent_SetTranslation(Entity.ID, value);
        }
    }

    public Quaternion Rotation
    {
        get
        {
            InternalCalls.TransformComponent_GetRotation(Entity.ID, out Quaternion quat);
            return quat;
        }
        set
        {
            InternalCalls.TransformComponent_SetRotation(Entity.ID, value);
        }
    }

    public Vector3 EulerAngles
    {
        get
        {
            InternalCalls.TransformComponent_GetEulerAngles(Entity.ID, out Vector3 quat);
            return quat;
        }
        set
        {
            InternalCalls.TransformComponent_SetEulerAngles(Entity.ID, value);
        }
    }

    public Vector3 Scale
    {
        get
        {
            InternalCalls.TransformComponent_GetScale(Entity.ID, out Vector3 scale);
            return scale;
        }
        set
        {
            InternalCalls.TransformComponent_SetScale(Entity.ID, value);
        }
    }
}
