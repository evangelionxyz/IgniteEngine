// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using System;
using System.Runtime.InteropServices;

namespace Ignite.Core;

public static class CoreInternalCalls
{
    private static bool s_Initialized;
    private static CoreNativeAPI.Funcs.DebugLogFn s_DebugLog;
    
    private static CoreNativeAPI.Funcs.InputIsKeyPressedFn s_InputIsKeyPressed;
    private static CoreNativeAPI.Funcs.InputIsModifierPressedFn s_InputIsModifierPressed;
    private static CoreNativeAPI.Funcs.InputIsMouseButtonPressedFn s_InputIsMouseButtonPressed;
    private static CoreNativeAPI.Funcs.InputGetMousePositionFn s_InputGetMousePosition;
    private static CoreNativeAPI.Funcs.InputSetMouseToCenterFn s_InputSetMouseToCenter;
    private static CoreNativeAPI.Funcs.InputSetCursorModeFn s_InputSetCursorMode;

    private static CoreNativeAPI.Funcs.AssetManagerQueryFn s_AssetManagerIsAssetHandleValid;
    private static CoreNativeAPI.Funcs.AssetManagerQueryFn s_AssetManagerIsAssetLoaded;
    private static CoreNativeAPI.Funcs.AssetManagerLoadFn s_AssetManagerLoadAssetAsync;
    private static CoreNativeAPI.Funcs.AssetManagerLoadFn s_AssetManagerLoadAssetImmediate;

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
        s_InputSetMouseToCenter = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.InputSetMouseToCenterFn>(api.Input_SetMouseToCenter);
        s_InputSetCursorMode = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.InputSetCursorModeFn>(api.Input_SetCursorMode);
        
        s_AssetManagerIsAssetHandleValid = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.AssetManagerQueryFn>(api.AssetManager_IsAssetHandleValid);
        s_AssetManagerIsAssetLoaded = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.AssetManagerQueryFn>(api.AssetManager_IsAssetLoaded);
        s_AssetManagerLoadAssetAsync = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.AssetManagerLoadFn>(api.AssetManager_LoadAssetAsync);
        s_AssetManagerLoadAssetImmediate = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.AssetManagerLoadFn>(api.AssetManager_LoadAssetImmediate);

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
        try
        {
            s_DebugLog(ptr, level);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static bool Input_IsKeyPressed(uint keyCode)
    {
        EnsureInitialized();
        return s_InputIsKeyPressed(keyCode);
    }

    internal static bool Input_IsModifierPressed(ushort modCode)
    {
        EnsureInitialized();
        return s_InputIsModifierPressed(modCode);
    }

    internal static bool Input_IsMouseButtonPressed(byte button)
    {
        EnsureInitialized();
        return s_InputIsMouseButtonPressed(button);
    }

    internal static void Input_GetMousePosition(out Mathf.Vector2 result)
    {
        EnsureInitialized();
        s_InputGetMousePosition(out NativeObject.Vector2 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void Input_SetMouseToCenter()
    {
        EnsureInitialized();
        s_InputSetMouseToCenter();
    }

    internal static void Input_SetCursorMode(int mode)
    {
        EnsureInitialized();
        s_InputSetCursorMode(mode);
    }

    // Asset manager
    internal static bool AssetManager_IsAssetHandleValid(ulong handle)
    {
        EnsureInitialized();
        return s_AssetManagerIsAssetHandleValid(handle);
    }

    internal static bool AssetManager_IsAssetLoaded(ulong handle)
    {
        EnsureInitialized();
        return s_AssetManagerIsAssetLoaded(handle);
    }

    internal static void AssetManager_LoadAssetAsync(ulong handle)
    {
        EnsureInitialized();
        s_AssetManagerLoadAssetAsync(handle);
    }

    internal static void AssetManager_LoadAssetImmediate(ulong handle)
    {
        EnsureInitialized();
        s_AssetManagerLoadAssetImmediate(handle);
    }
}