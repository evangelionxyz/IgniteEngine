using System;
using System.Runtime.InteropServices;

namespace Ignite.Core;

public static class CoreNativeAPI
{
    [StructLayout(LayoutKind.Sequential)]
    public struct API
    {
        public IntPtr Debug_Log;

        public IntPtr Input_IsKeyPressed;
        public IntPtr Input_IsModifierPressed;
        public IntPtr Input_IsMouseButtonPressed;
        public IntPtr Input_GetMousePosition;
        public IntPtr Input_SetMouseToCenter;
        public IntPtr Input_SetCursorMode;

        public IntPtr AssetManager_IsAssetHandleValid;

        public IntPtr AssetManager_LoadAssetAsyncFromFile;
        public IntPtr AssetManager_LoadAssetImmedateFromFile;

        public IntPtr AssetManager_IsAssetLoaded;
        public IntPtr AssetManager_LoadAssetAsync;
        public IntPtr AssetManager_LoadAssetImmediate;

        // ScriptableObject
        public IntPtr ScriptableObject_GetFieldValueFloat;
        public IntPtr ScriptableObject_GetFieldValueInt;
        public IntPtr ScriptableObject_GetFieldValueBool;
        public IntPtr ScriptableObject_GetFieldValueString;
        public IntPtr ScriptableObject_GetClassName;
    }

    public struct Funcs
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void DebugLogFn(IntPtr message, Debug.LogLevel level);

        // =============================
        // Basic type
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetStringFn(ulong entityID, out IntPtr result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetStringFn(ulong entityID, IntPtr value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetIntFn(ulong entityID, out int result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetIntFn(ulong entityID, int value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetFloatFn(ulong entityID, out float result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetFloatFn(ulong entityID, float value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetBoolFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] out bool result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetBoolFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] bool value);


        // ================================
        // Vectors & Quaternion
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetVector2Fn(ulong entityID, out NativeObject.Vector2 result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetVector2Fn(ulong entityID, NativeObject.Vector2 value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate ulong GetVector3Fn(ulong entityID, out NativeObject.Vector3 value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate ulong SetVector3Fn(ulong entityID, NativeObject.Vector3 value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate ulong GetVector4Fn(ulong entityID, out NativeObject.Vector4 value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate ulong SetVector4Fn(ulong entityID, NativeObject.Vector4 value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetQuaternionFn(ulong entityID, out NativeObject.Quaternion result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetQuaternionFn(ulong entityID, NativeObject.Quaternion value);

        // Input system
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool InputIsKeyPressedFn(uint keyCode);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool InputIsModifierPressedFn(ushort modCode);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool InputIsMouseButtonPressedFn(byte button);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void InputGetMousePositionFn(out NativeObject.Vector2 result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void InputSetMouseToCenterFn();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void InputSetCursorModeFn(int mode);

        // Asset Manager
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool AssetManagerQueryFn(ulong handle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void AssetManagerLoadFn(ulong handle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate ulong AssetManagerLoadFromPathFn(IntPtr filename);

        // ScriptableObject field access (keyed by AssetHandle + field name)
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate float  ScriptableObjectGetFieldFloatFn(ulong handle, IntPtr fieldName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate int    ScriptableObjectGetFieldIntFn(ulong handle, IntPtr fieldName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool   ScriptableObjectGetFieldBoolFn(ulong handle, IntPtr fieldName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate IntPtr ScriptableObjectGetFieldStringFn(ulong handle, IntPtr fieldName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate IntPtr ScriptableObjectGetClassNameFn(ulong handle);
    }
}
