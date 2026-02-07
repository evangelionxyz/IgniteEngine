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

using System;
using System.Runtime.CompilerServices;

namespace IgniteEngine;

public static class InternalCalls
{
    // Entity Method
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static bool Entity_HasComponent(ulong entityID, Type componentType);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void Entity_AddComponent(ulong entityID, Type componentType);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static ulong Entity_FindEntityByName(string name);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static ulong Entity_Instantiate(ulong entityID, Vector3 value);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void Entity_Destroy(ulong entityID);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void Entity_SetVisibility(ulong entityID, bool value);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void Entity_GetVisibility(ulong entityID, out bool result);

    // Script Instance
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static object GetScriptInstance(ulong entityID);

    // Transform
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void TransformComponent_GetForward(ulong entityID, out Vector3 result);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void TransformComponent_SetForward(ulong entityID, Vector3 result);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void TransformComponent_GetRight(ulong entityID, out Vector3 result);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void TransformComponent_SetRight(ulong entityID, Vector3 result);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void TransformComponent_GetUp(ulong entityID, out Vector3 result);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void TransformComponent_SetUp(ulong entityID, Vector3 result);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void TransformComponent_GetTranslation(ulong entityID, out Vector3 result);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void TransformComponent_SetTranslation(ulong entityID, Vector3 value);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void TransformComponent_GetRotation(ulong entityID, out Quaternion result);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void TransformComponent_SetRotation(ulong entityID, Quaternion value);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void TransformComponent_GetEulerAngles(ulong entityID, out Vector3 result);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void TransformComponent_SetEulerAngles(ulong entityID, Vector3 value);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void TransformComponent_GetScale(ulong entityID, out Vector3 result);
    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal extern static void TransformComponent_SetScale(ulong entityID, Vector3 value);
}
