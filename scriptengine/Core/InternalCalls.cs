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
using System.Runtime.InteropServices;

namespace IgniteScriptEngine;

public static class InternalCalls
{
    [StructLayout(LayoutKind.Sequential)]
    private struct NativeVector3
    {
        public float X;
        public float Y;
        public float Z;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeQuaternion
    {
        public float X;
        public float Y;
        public float Z;
        public float W;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeApi
    {
        public IntPtr Debug_Log;
        public IntPtr Entity_HasComponent;
        public IntPtr Entity_AddComponent;
        public IntPtr Entity_FindEntityByName;
        public IntPtr Entity_Instantiate;
        public IntPtr Entity_Destroy;
        public IntPtr Entity_SetVisibility;
        public IntPtr Entity_GetVisibility;
        public IntPtr TransformComponent_GetForward;
        public IntPtr TransformComponent_SetForward;
        public IntPtr TransformComponent_GetRight;
        public IntPtr TransformComponent_SetRight;
        public IntPtr TransformComponent_GetUp;
        public IntPtr TransformComponent_SetUp;
        public IntPtr TransformComponent_GetTranslation;
        public IntPtr TransformComponent_SetTranslation;
        public IntPtr TransformComponent_GetRotation;
        public IntPtr TransformComponent_SetRotation;
        public IntPtr TransformComponent_GetEulerAngles;
        public IntPtr TransformComponent_SetEulerAngles;
        public IntPtr TransformComponent_GetScale;
        public IntPtr TransformComponent_SetScale;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void DebugLogFn(IntPtr message);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private delegate bool EntityHasComponentFn(ulong entityID, IntPtr componentTypeName);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void EntityAddComponentFn(ulong entityID, IntPtr componentTypeName);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate ulong EntityFindEntityByNameFn(IntPtr name);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate ulong EntityInstantiateFn(ulong entityID, NativeVector3 value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void EntityDestroyFn(ulong entityID);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void EntitySetVisibilityFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] bool value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void EntityGetVisibilityFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] out bool result);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void TransformGetVec3Fn(ulong entityID, out NativeVector3 result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void TransformSetVec3Fn(ulong entityID, NativeVector3 value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void TransformGetQuatFn(ulong entityID, out NativeQuaternion result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void TransformSetQuatFn(ulong entityID, NativeQuaternion value);

    private static bool s_Initialized;
    private static DebugLogFn s_DebugLog;
    private static EntityHasComponentFn s_EntityHasComponent;
    private static EntityAddComponentFn s_EntityAddComponent;
    private static EntityFindEntityByNameFn s_EntityFindEntityByName;
    private static EntityInstantiateFn s_EntityInstantiate;
    private static EntityDestroyFn s_EntityDestroy;
    private static EntitySetVisibilityFn s_EntitySetVisibility;
    private static EntityGetVisibilityFn s_EntityGetVisibility;
    private static TransformGetVec3Fn s_TransformGetForward;
    private static TransformSetVec3Fn s_TransformSetForward;
    private static TransformGetVec3Fn s_TransformGetRight;
    private static TransformSetVec3Fn s_TransformSetRight;
    private static TransformGetVec3Fn s_TransformGetUp;
    private static TransformSetVec3Fn s_TransformSetUp;
    private static TransformGetVec3Fn s_TransformGetTranslation;
    private static TransformSetVec3Fn s_TransformSetTranslation;
    private static TransformGetQuatFn s_TransformGetRotation;
    private static TransformSetQuatFn s_TransformSetRotation;
    private static TransformGetVec3Fn s_TransformGetEulerAngles;
    private static TransformSetVec3Fn s_TransformSetEulerAngles;
    private static TransformGetVec3Fn s_TransformGetScale;
    private static TransformSetVec3Fn s_TransformSetScale;

    public static void Initialize(ulong apiPtr)
    {
        if (apiPtr == 0)
            throw new ArgumentException("Invalid internal calls API pointer", nameof(apiPtr));

        NativeApi api = Marshal.PtrToStructure<NativeApi>((IntPtr)apiPtr);

        s_DebugLog = Marshal.GetDelegateForFunctionPointer<DebugLogFn>(api.Debug_Log);
        s_EntityHasComponent = Marshal.GetDelegateForFunctionPointer<EntityHasComponentFn>(api.Entity_HasComponent);
        s_EntityAddComponent = Marshal.GetDelegateForFunctionPointer<EntityAddComponentFn>(api.Entity_AddComponent);
        s_EntityFindEntityByName = Marshal.GetDelegateForFunctionPointer<EntityFindEntityByNameFn>(api.Entity_FindEntityByName);
        s_EntityInstantiate = Marshal.GetDelegateForFunctionPointer<EntityInstantiateFn>(api.Entity_Instantiate);
        s_EntityDestroy = Marshal.GetDelegateForFunctionPointer<EntityDestroyFn>(api.Entity_Destroy);
        s_EntitySetVisibility = Marshal.GetDelegateForFunctionPointer<EntitySetVisibilityFn>(api.Entity_SetVisibility);
        s_EntityGetVisibility = Marshal.GetDelegateForFunctionPointer<EntityGetVisibilityFn>(api.Entity_GetVisibility);
        s_TransformGetForward = Marshal.GetDelegateForFunctionPointer<TransformGetVec3Fn>(api.TransformComponent_GetForward);
        s_TransformSetForward = Marshal.GetDelegateForFunctionPointer<TransformSetVec3Fn>(api.TransformComponent_SetForward);
        s_TransformGetRight = Marshal.GetDelegateForFunctionPointer<TransformGetVec3Fn>(api.TransformComponent_GetRight);
        s_TransformSetRight = Marshal.GetDelegateForFunctionPointer<TransformSetVec3Fn>(api.TransformComponent_SetRight);
        s_TransformGetUp = Marshal.GetDelegateForFunctionPointer<TransformGetVec3Fn>(api.TransformComponent_GetUp);
        s_TransformSetUp = Marshal.GetDelegateForFunctionPointer<TransformSetVec3Fn>(api.TransformComponent_SetUp);
        s_TransformGetTranslation = Marshal.GetDelegateForFunctionPointer<TransformGetVec3Fn>(api.TransformComponent_GetTranslation);
        s_TransformSetTranslation = Marshal.GetDelegateForFunctionPointer<TransformSetVec3Fn>(api.TransformComponent_SetTranslation);
        s_TransformGetRotation = Marshal.GetDelegateForFunctionPointer<TransformGetQuatFn>(api.TransformComponent_GetRotation);
        s_TransformSetRotation = Marshal.GetDelegateForFunctionPointer<TransformSetQuatFn>(api.TransformComponent_SetRotation);
        s_TransformGetEulerAngles = Marshal.GetDelegateForFunctionPointer<TransformGetVec3Fn>(api.TransformComponent_GetEulerAngles);
        s_TransformSetEulerAngles = Marshal.GetDelegateForFunctionPointer<TransformSetVec3Fn>(api.TransformComponent_SetEulerAngles);
        s_TransformGetScale = Marshal.GetDelegateForFunctionPointer<TransformGetVec3Fn>(api.TransformComponent_GetScale);
        s_TransformSetScale = Marshal.GetDelegateForFunctionPointer<TransformSetVec3Fn>(api.TransformComponent_SetScale);

        s_Initialized = true;
    }

    private static void EnsureInitialized()
    {
        if (!s_Initialized)
            throw new InvalidOperationException("InternalCalls bridge is not initialized.");
    }

    private static NativeVector3 ToNative(Vector3 value) => new NativeVector3 { X = value.X, Y = value.Y, Z = value.Z };
    private static Vector3 ToManaged(NativeVector3 value) => new Vector3(value.X, value.Y, value.Z);
    private static NativeQuaternion ToNative(Quaternion value) => new NativeQuaternion { X = value.X, Y = value.Y, Z = value.Z, W = value.W };
    private static Quaternion ToManaged(NativeQuaternion value) => new Quaternion(value.X, value.Y, value.Z, value.W);

    private static IntPtr StringToUtf8(string value) => Marshal.StringToCoTaskMemUTF8(value ?? string.Empty);

    internal static void Debug_Log(string message)
    {
        EnsureInitialized();

        IntPtr ptr = StringToUtf8(message);
        try
        {
            s_DebugLog(ptr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static bool Entity_HasComponent(ulong entityID, Type componentType)
    {
        EnsureInitialized();

        string typeName = componentType?.FullName ?? componentType?.Name ?? string.Empty;
        IntPtr ptr = StringToUtf8(typeName);
        try
        {
            return s_EntityHasComponent(entityID, ptr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static void Entity_AddComponent(ulong entityID, Type componentType)
    {
        EnsureInitialized();

        string typeName = componentType?.FullName ?? componentType?.Name ?? string.Empty;
        IntPtr ptr = StringToUtf8(typeName);
        try
        {
            s_EntityAddComponent(entityID, ptr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static ulong Entity_FindEntityByName(string name)
    {
        EnsureInitialized();

        IntPtr ptr = StringToUtf8(name);
        try
        {
            return s_EntityFindEntityByName(ptr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static ulong Entity_Instantiate(ulong entityID, Vector3 value)
    {
        EnsureInitialized();
        return s_EntityInstantiate(entityID, ToNative(value));
    }

    internal static void Entity_Destroy(ulong entityID)
    {
        EnsureInitialized();
        s_EntityDestroy(entityID);
    }

    internal static void Entity_SetVisibility(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_EntitySetVisibility(entityID, value);
    }

    internal static void Entity_GetVisibility(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_EntityGetVisibility(entityID, out result);
    }

    internal static object GetScriptInstance(ulong entityID)
    {
        _ = entityID;
        return null;
    }

    internal static void TransformComponent_GetForward(ulong entityID, out Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetForward(entityID, out NativeVector3 native);
        result = ToManaged(native);
    }

    internal static void TransformComponent_SetForward(ulong entityID, Vector3 result)
    {
        EnsureInitialized();
        s_TransformSetForward(entityID, ToNative(result));
    }

    internal static void TransformComponent_GetRight(ulong entityID, out Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetRight(entityID, out NativeVector3 native);
        result = ToManaged(native);
    }

    internal static void TransformComponent_SetRight(ulong entityID, Vector3 result)
    {
        EnsureInitialized();
        s_TransformSetRight(entityID, ToNative(result));
    }

    internal static void TransformComponent_GetUp(ulong entityID, out Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetUp(entityID, out NativeVector3 native);
        result = ToManaged(native);
    }

    internal static void TransformComponent_SetUp(ulong entityID, Vector3 result)
    {
        EnsureInitialized();
        s_TransformSetUp(entityID, ToNative(result));
    }

    internal static void TransformComponent_GetTranslation(ulong entityID, out Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetTranslation(entityID, out NativeVector3 native);
        result = ToManaged(native);
    }

    internal static void TransformComponent_SetTranslation(ulong entityID, Vector3 value)
    {
        EnsureInitialized();
        s_TransformSetTranslation(entityID, ToNative(value));
    }

    internal static void TransformComponent_GetRotation(ulong entityID, out Quaternion result)
    {
        EnsureInitialized();
        s_TransformGetRotation(entityID, out NativeQuaternion native);
        result = ToManaged(native);
    }

    internal static void TransformComponent_SetRotation(ulong entityID, Quaternion value)
    {
        EnsureInitialized();
        s_TransformSetRotation(entityID, ToNative(value ?? Quaternion.Identity));
    }

    internal static void TransformComponent_GetEulerAngles(ulong entityID, out Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetEulerAngles(entityID, out NativeVector3 native);
        result = ToManaged(native);
    }

    internal static void TransformComponent_SetEulerAngles(ulong entityID, Vector3 value)
    {
        EnsureInitialized();
        s_TransformSetEulerAngles(entityID, ToNative(value));
    }

    internal static void TransformComponent_GetScale(ulong entityID, out Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetScale(entityID, out NativeVector3 native);
        result = ToManaged(native);
    }

    internal static void TransformComponent_SetScale(ulong entityID, Vector3 value)
    {
        EnsureInitialized();
        s_TransformSetScale(entityID, ToNative(value));
    }
}
