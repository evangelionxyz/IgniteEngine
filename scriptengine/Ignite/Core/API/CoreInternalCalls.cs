// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using System;
using System.Reflection.Metadata;
using System.Runtime.InteropServices;

namespace Ignite.Core;

public static class CoreInternalCalls
{
    private static bool s_Initialized;
    private static CoreNativeAPI.Funcs.DebugLogFn? s_DebugLog;
    
    private static CoreNativeAPI.Funcs.InputIsKeyPressedFn? s_InputIsKeyPressed;
    private static CoreNativeAPI.Funcs.InputIsModifierPressedFn? s_InputIsModifierPressed;
    private static CoreNativeAPI.Funcs.InputIsMouseButtonPressedFn? s_InputIsMouseButtonPressed;
    private static CoreNativeAPI.Funcs.InputGetMousePositionFn? s_InputGetMousePosition;
    private static CoreNativeAPI.Funcs.InputGetMouseDeltaFn? s_InputGetMouseDelta;
    private static CoreNativeAPI.Funcs.InputSetMouseToCenterFn? s_InputSetMouseToCenter;
    private static CoreNativeAPI.Funcs.InputSetCursorModeFn? s_InputSetCursorMode;
    private static CoreNativeAPI.Funcs.InputGetCursorModeFn? s_InputGetCursorMode;
    private static CoreNativeAPI.Funcs.InputIsMouseOverUIFn? s_InputIsMouseOverUI;
    private static CoreNativeAPI.Funcs.InputIsActionPressedFn? s_InputIsActionPressed;

    private static CoreNativeAPI.Funcs.AssetManagerQueryFn? s_AssetManagerIsAssetHandleValid;

    private static CoreNativeAPI.Funcs.AssetManagerLoadFromPathFn? s_AssetManagerLoadAssetAsyncFromFile;
    private static CoreNativeAPI.Funcs.AssetManagerLoadFromPathFn? s_AssetManagerLoadAssetImmediateFromFile;
    
    private static CoreNativeAPI.Funcs.AssetManagerQueryFn? s_AssetManagerIsAssetLoaded;
    private static CoreNativeAPI.Funcs.AssetManagerLoadFn? s_AssetManagerLoadAssetAsync;
    private static CoreNativeAPI.Funcs.AssetManagerLoadFn? s_AssetManagerLoadAssetImmediate;
    private static CoreNativeAPI.Funcs.SceneLoadFn? s_SceneLoad;

    public static void Initialize(ulong apiPtr)
    {
        if (apiPtr == 0)
            throw new ArgumentException("Invalid internal calls API pointer", nameof(apiPtr));

        CoreNativeAPI.API api = Marshal.PtrToStructure<CoreNativeAPI.API>((IntPtr)apiPtr);
        s_DebugLog = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.DebugLogFn>(api.Debug_Log);
        s_InputIsKeyPressed = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.InputIsKeyPressedFn>(api.Input_IsKeyPressed);
        s_InputIsModifierPressed = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.InputIsModifierPressedFn>(api.Input_IsModifierPressed);
        s_InputIsMouseButtonPressed = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.InputIsMouseButtonPressedFn>(api.Input_IsMouseButtonPressed);
        s_InputGetMousePosition = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.InputGetMousePositionFn>(api.Input_GetMousePosition);
        s_InputGetMouseDelta = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.InputGetMouseDeltaFn>(api.Input_GetMouseDelta);
        s_InputSetMouseToCenter = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.InputSetMouseToCenterFn>(api.Input_SetMouseToCenter);
        s_InputSetCursorMode = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.InputSetCursorModeFn>(api.Input_SetCursorMode);
        s_InputGetCursorMode = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.InputGetCursorModeFn>(api.Input_GetCursorMode);
        s_InputIsMouseOverUI = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.InputIsMouseOverUIFn>(api.Input_IsMouseOverUI);
        s_InputIsActionPressed = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.InputIsActionPressedFn>(api.Input_IsActionPressed);
        
        s_AssetManagerIsAssetHandleValid = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.AssetManagerQueryFn>(api.AssetManager_IsAssetHandleValid);

        s_AssetManagerLoadAssetAsyncFromFile = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.AssetManagerLoadFromPathFn>(api.AssetManager_LoadAssetAsyncFromFile);
        s_AssetManagerLoadAssetImmediateFromFile = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.AssetManagerLoadFromPathFn>(api.AssetManager_LoadAssetImmedateFromFile);

        s_AssetManagerIsAssetLoaded = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.AssetManagerQueryFn>(api.AssetManager_IsAssetLoaded);
        s_AssetManagerLoadAssetAsync = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.AssetManagerLoadFn>(api.AssetManager_LoadAssetAsync);
        s_AssetManagerLoadAssetImmediate = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.AssetManagerLoadFn>(api.AssetManager_LoadAssetImmediate);

        // Optional: ScriptableObject runtime field access
        TryBindScriptableObjectFunctions(api);

        try
        {
            if (api.Scene_TransitionTo != IntPtr.Zero)
                s_SceneLoad = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SceneLoadFn>(api.Scene_TransitionTo);
        }
        catch { /* optional binding */ }

        s_Initialized = true;
    }

    private static void EnsureInitialized()
    {
        if (!s_Initialized)
            throw new InvalidOperationException("InternalCalls bridge is not initialized.");
    }

    internal static void Debug_Log(string message, Debug.LogLevel level)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(message);
        try { s_DebugLog!(ptr, level); }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    internal static bool Input_IsKeyPressed(uint keyCode)
    {
        EnsureInitialized();
        return s_InputIsKeyPressed!(keyCode);
    }

    internal static bool Input_IsModifierPressed(ushort modCode)
    {
        EnsureInitialized();
        return s_InputIsModifierPressed!(modCode);
    }

    internal static bool Input_IsMouseButtonPressed(byte button)
    {
        EnsureInitialized();
        return s_InputIsMouseButtonPressed!(button);
    }

    internal static void Input_GetMousePosition(out Mathf.Vector2 result)
    {
        EnsureInitialized();
        s_InputGetMousePosition!(out NativeObject.Vector2 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void Input_GetMouseDelta(out Mathf.Vector2 result)
    {
        EnsureInitialized();
        s_InputGetMouseDelta!(out NativeObject.Vector2 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void Input_SetMouseToCenter()
    {
        EnsureInitialized();
        s_InputSetMouseToCenter!();
    }

    internal static void Input_SetCursorMode(int mode)
    {
        EnsureInitialized();
        s_InputSetCursorMode!(mode);
    }

    internal static int Input_GetCursorMode()
    {
        EnsureInitialized();
        return s_InputGetCursorMode!();
    }

    internal static bool Input_IsMouseOverUI()
    {
        EnsureInitialized();
        return s_InputIsMouseOverUI!();
    }

    internal static bool Input_IsActionPressed(string actionName)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(actionName);
        try { return s_InputIsActionPressed!(ptr); }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    // Asset manager
    internal static bool AssetManager_IsAssetHandleValid(ulong handle)
    {
        EnsureInitialized();
        return s_AssetManagerIsAssetHandleValid!(handle);
    }

    internal static bool AssetManager_IsAssetLoaded(ulong handle)
    {
        EnsureInitialized();
        return s_AssetManagerIsAssetLoaded!(handle);
    }

    internal static AssetHandle AssetManager_LoadAssetAsyncFromFile(string filename)
    {
        EnsureInitialized();

        IntPtr ptr = NativeObject.StringToUtf8(filename);
        try
        {
            return new AssetHandle(s_AssetManagerLoadAssetAsyncFromFile!(ptr));

        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }

    }

    internal static AssetHandle AssetManager_LoadAssetImmedateFromFile(string filename)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(filename);
        try
        {
            return new AssetHandle(s_AssetManagerLoadAssetImmediateFromFile!(ptr));
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static void AssetManager_LoadAssetAsync(ulong handle)
    {
        EnsureInitialized();
        s_AssetManagerLoadAssetAsync!(handle);
    }

    internal static void AssetManager_LoadAssetImmediate(ulong handle)
    {
        EnsureInitialized();
        s_AssetManagerLoadAssetImmediate!(handle);
    }

    // ---- ScriptableObject field accessors ----
    private static CoreNativeAPI.Funcs.ScriptableObjectGetFieldFloatFn? s_SOGetFieldFloat;
    private static CoreNativeAPI.Funcs.ScriptableObjectGetFieldIntFn? s_SOGetFieldInt;
    private static CoreNativeAPI.Funcs.ScriptableObjectGetFieldBoolFn? s_SOGetFieldBool;
    private static CoreNativeAPI.Funcs.ScriptableObjectGetFieldStringFn? s_SOGetFieldString;
    private static CoreNativeAPI.Funcs.ScriptableObjectGetClassNameFn? s_SOGetClassName;

    internal static bool HasScriptableObjectBridge => s_SOGetFieldFloat != null;

    internal static float ScriptableObject_GetFieldFloat(ulong handle, string fieldName)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(fieldName);
        try { return s_SOGetFieldFloat!(handle, ptr); }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    internal static int ScriptableObject_GetFieldInt(ulong handle, string fieldName)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(fieldName);
        try { return s_SOGetFieldInt!(handle, ptr); }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    internal static bool ScriptableObject_GetFieldBool(ulong handle, string fieldName)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(fieldName);
        try { return s_SOGetFieldBool!(handle, ptr); }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    internal static string ScriptableObject_GetFieldString(ulong handle, string fieldName)
    {
        EnsureInitialized();
        IntPtr namePtr = NativeObject.StringToUtf8(fieldName);
        try
        {
            IntPtr result = s_SOGetFieldString!(handle, namePtr);
            return NativeObject.Utf8ToString(result)!;
        }
        finally { Marshal.FreeCoTaskMem(namePtr); }
    }

    internal static string ScriptableObject_GetClassName(ulong handle)
    {
        EnsureInitialized();
        IntPtr result = s_SOGetClassName!(handle);
        return NativeObject.Utf8ToString(result)!;
    }

    /// <summary>
    /// Called during Initialize() to bind the ScriptableObject function pointers if present.
    /// Separate method so the struct size doesn't break older native hosts that don't provide SO funcs.
    /// </summary>
    internal static void TryBindScriptableObjectFunctions(CoreNativeAPI.API api)
    {
        try
        {
            if (api.ScriptableObject_GetFieldValueFloat != IntPtr.Zero)
                s_SOGetFieldFloat = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.ScriptableObjectGetFieldFloatFn>(api.ScriptableObject_GetFieldValueFloat);
            if (api.ScriptableObject_GetFieldValueInt != IntPtr.Zero)
                s_SOGetFieldInt = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.ScriptableObjectGetFieldIntFn>(api.ScriptableObject_GetFieldValueInt);
            if (api.ScriptableObject_GetFieldValueBool != IntPtr.Zero)
                s_SOGetFieldBool = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.ScriptableObjectGetFieldBoolFn>(api.ScriptableObject_GetFieldValueBool);
            if (api.ScriptableObject_GetFieldValueString != IntPtr.Zero)
                s_SOGetFieldString = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.ScriptableObjectGetFieldStringFn>(api.ScriptableObject_GetFieldValueString);
            if (api.ScriptableObject_GetClassName != IntPtr.Zero)
                s_SOGetClassName = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.ScriptableObjectGetClassNameFn>(api.ScriptableObject_GetClassName);
        }
        catch { /* optional binding - older native hosts may not have SO support */ }
     }

    internal static void Scene_Load(ulong handle)
    {
        EnsureInitialized();
        if (s_SceneLoad == null)
            throw new InvalidOperationException("Scene transition native call is not bound.");
        s_SceneLoad(handle);
    }
}