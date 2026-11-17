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

    private static CoreNativeAPI.Funcs.NetBoolQueryFn? s_NetIsConnected;
    private static CoreNativeAPI.Funcs.NetBoolQueryFn? s_NetIsServerRunning;
    private static CoreNativeAPI.Funcs.NetBoolQueryFn? s_NetIsConnecting;
    private static CoreNativeAPI.Funcs.NetUIntQueryFn? s_NetGetClientId;
    private static CoreNativeAPI.Funcs.NetStartServerFn? s_NetStartServer;
    private static CoreNativeAPI.Funcs.NetStartServerDefaultFn? s_NetStartServerDefault;
    private static CoreNativeAPI.Funcs.NetActionFn? s_NetStopServer;
    private static CoreNativeAPI.Funcs.NetConnectFn? s_NetConnect;
    private static CoreNativeAPI.Funcs.NetActionFn? s_NetDisconnect;
    private static CoreNativeAPI.Funcs.NetSpawnEntityFn? s_NetSpawnEntity;
    private static CoreNativeAPI.Funcs.NetDespawnEntityFn? s_NetDespawnEntity;
    private static CoreNativeAPI.Funcs.NetSendInputFn? s_NetSendInput;
    private static CoreNativeAPI.Funcs.NetSendChatFn? s_NetSendChat;
    private static CoreNativeAPI.Funcs.NetKickClientFn? s_NetKickClient;
    private static CoreNativeAPI.Funcs.NetGetStatsFn? s_NetGetStats;
    private static CoreNativeAPI.Funcs.NetUIntQueryFn? s_NetGetServerTick;
    private static CoreNativeAPI.Funcs.NetUIntQueryFn? s_NetGetClientServerTick;
    private static CoreNativeAPI.Funcs.NetUIntQueryFn? s_NetGetClientCount;
    private static CoreNativeAPI.Funcs.NetGetConnectedClientAtFn? s_NetGetConnectedClientAt;
    private static CoreNativeAPI.Funcs.NetUIntQueryFn? s_NetGetChatCount;
    private static CoreNativeAPI.Funcs.NetGetChatAtFn? s_NetGetChatAt;
    private static CoreNativeAPI.Funcs.NetSetSimulationSettingsFn? s_NetSetSimulationSettings;

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

            if (api.Net_IsConnected != IntPtr.Zero)
                s_NetIsConnected = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetBoolQueryFn>(api.Net_IsConnected);
            if (api.Net_IsServerRunning != IntPtr.Zero)
                s_NetIsServerRunning = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetBoolQueryFn>(api.Net_IsServerRunning);
            if (api.Net_IsConnecting != IntPtr.Zero)
                s_NetIsConnecting = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetBoolQueryFn>(api.Net_IsConnecting);
            if (api.Net_GetClientId != IntPtr.Zero)
                s_NetGetClientId = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetUIntQueryFn>(api.Net_GetClientId);
            if (api.Net_StartServer != IntPtr.Zero)
                s_NetStartServer = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetStartServerFn>(api.Net_StartServer);
            if (api.Net_StartServerDefault != IntPtr.Zero)
                s_NetStartServerDefault = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetStartServerDefaultFn>(api.Net_StartServerDefault);
            if (api.Net_StopServer != IntPtr.Zero)
                s_NetStopServer = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetActionFn>(api.Net_StopServer);
            if (api.Net_Connect != IntPtr.Zero)
                s_NetConnect = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetConnectFn>(api.Net_Connect);
            if (api.Net_Disconnect != IntPtr.Zero)
                s_NetDisconnect = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetActionFn>(api.Net_Disconnect);
            if (api.Net_SpawnEntity != IntPtr.Zero)
                s_NetSpawnEntity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetSpawnEntityFn>(api.Net_SpawnEntity);
            if (api.Net_DespawnEntity != IntPtr.Zero)
                s_NetDespawnEntity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetDespawnEntityFn>(api.Net_DespawnEntity);
            if (api.Net_SendInput != IntPtr.Zero)
                s_NetSendInput = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetSendInputFn>(api.Net_SendInput);
            if (api.Net_SendChat != IntPtr.Zero)
                s_NetSendChat = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetSendChatFn>(api.Net_SendChat);
            if (api.Net_KickClient != IntPtr.Zero)
                s_NetKickClient = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetKickClientFn>(api.Net_KickClient);
            if (api.Net_GetStats != IntPtr.Zero)
                s_NetGetStats = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetGetStatsFn>(api.Net_GetStats);
            if (api.Net_GetServerTick != IntPtr.Zero)
                s_NetGetServerTick = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetUIntQueryFn>(api.Net_GetServerTick);
            if (api.Net_GetClientServerTick != IntPtr.Zero)
                s_NetGetClientServerTick = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetUIntQueryFn>(api.Net_GetClientServerTick);
            if (api.Net_GetClientCount != IntPtr.Zero)
                s_NetGetClientCount = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetUIntQueryFn>(api.Net_GetClientCount);
            if (api.Net_GetConnectedClientAt != IntPtr.Zero)
                s_NetGetConnectedClientAt = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetGetConnectedClientAtFn>(api.Net_GetConnectedClientAt);
            if (api.Net_GetChatCount != IntPtr.Zero)
                s_NetGetChatCount = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetUIntQueryFn>(api.Net_GetChatCount);
            if (api.Net_GetChatAt != IntPtr.Zero)
                s_NetGetChatAt = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetGetChatAtFn>(api.Net_GetChatAt);
            if (api.Net_SetSimulationSettings != IntPtr.Zero)
                s_NetSetSimulationSettings = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.NetSetSimulationSettingsFn>(api.Net_SetSimulationSettings);
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

    internal static bool Net_IsConnected()
    {
        EnsureInitialized();
        return s_NetIsConnected != null && s_NetIsConnected();
    }

    internal static bool Net_IsServerRunning()
    {
        EnsureInitialized();
        return s_NetIsServerRunning != null && s_NetIsServerRunning();
    }

    internal static bool Net_IsConnecting()
    {
        EnsureInitialized();
        return s_NetIsConnecting != null && s_NetIsConnecting();
    }

    internal static uint Net_GetClientId()
    {
        EnsureInitialized();
        return s_NetGetClientId != null ? s_NetGetClientId() : 0u;
    }

    internal static bool Net_StartServer()
    {
        EnsureInitialized();
        return s_NetStartServerDefault != null && s_NetStartServerDefault();
    }

    internal static bool Net_StartServer(ushort port, ushort tickRate, ushort snapshotRate)
    {
        EnsureInitialized();
        return s_NetStartServer != null && s_NetStartServer(port, tickRate, snapshotRate);
    }

    internal static void Net_StopServer()
    {
        EnsureInitialized();
        s_NetStopServer?.Invoke();
    }

    internal static bool Net_Connect(string address, ushort port, string playerName)
    {
        EnsureInitialized();
        if (s_NetConnect == null) return false;
        IntPtr addrPtr = NativeObject.StringToUtf8(address);
        IntPtr namePtr = NativeObject.StringToUtf8(playerName);
        try { return s_NetConnect(addrPtr, port, namePtr); }
        finally
        {
            Marshal.FreeCoTaskMem(addrPtr);
            Marshal.FreeCoTaskMem(namePtr);
        }
    }

    internal static void Net_Disconnect()
    {
        EnsureInitialized();
        s_NetDisconnect?.Invoke();
    }

    internal static ulong Net_SpawnEntity(string templateOrPrefab, Mathf.Vector3 position, Mathf.Quaternion rotation, uint ownerClient)
    {
        EnsureInitialized();
        if (s_NetSpawnEntity == null) return 0;
        NativeObject.Vector3 pos = NativeObject.ToNative(position);
        NativeObject.Quaternion rot = NativeObject.ToNative(rotation);
        IntPtr prefabPtr = NativeObject.StringToUtf8(templateOrPrefab);
        try { return s_NetSpawnEntity(prefabPtr, ref pos, ref rot, ownerClient); }
        finally { Marshal.FreeCoTaskMem(prefabPtr); }
    }

    internal static void Net_DespawnEntity(ulong netId)
    {
        EnsureInitialized();
        s_NetDespawnEntity?.Invoke(netId);
    }

    internal static void Net_SendInput(Mathf.Vector2 move, ushort buttons, float yaw)
    {
        EnsureInitialized();
        if (s_NetSendInput == null) return;
        NativeObject.Vector2 m = NativeObject.ToNative(move);
        s_NetSendInput(ref m, buttons, yaw);
    }

    internal static void Net_SendChat(string text)
    {
        EnsureInitialized();
        if (s_NetSendChat == null) return;
        IntPtr ptr = NativeObject.StringToUtf8(text);
        try { s_NetSendChat(ptr); }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    internal static void Net_KickClient(uint clientId, string reason)
    {
        EnsureInitialized();
        if (s_NetKickClient == null) return;
        IntPtr ptr = NativeObject.StringToUtf8(reason);
        try { s_NetKickClient(clientId, ptr); }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    internal static NetStats Net_GetStats()
    {
        EnsureInitialized();
        if (s_NetGetStats == null) return default;
        s_NetGetStats(out int rtt, out float loss, out float inKBps, out float outKBps, out uint count);
        return new NetStats
        {
            RttMs = rtt,
            PacketLossPercent = loss,
            InKBytesPerSec = inKBps,
            OutKBytesPerSec = outKBps,
            ClientCount = count
        };
    }

    internal static uint Net_GetServerTick()
    {
        EnsureInitialized();
        return s_NetGetServerTick != null ? s_NetGetServerTick() : 0u;
    }

    internal static uint Net_GetClientServerTick()
    {
        EnsureInitialized();
        return s_NetGetClientServerTick != null ? s_NetGetClientServerTick() : 0u;
    }

    internal static uint Net_GetClientCount()
    {
        EnsureInitialized();
        return s_NetGetClientCount != null ? s_NetGetClientCount() : 0u;
    }

    internal static NetConnectedClient[] Net_GetConnectedClients()
    {
        EnsureInitialized();
        if (s_NetGetClientCount == null || s_NetGetConnectedClientAt == null)
            return Array.Empty<NetConnectedClient>();

        uint count = s_NetGetClientCount();
        if (count == 0)
            return Array.Empty<NetConnectedClient>();

        var result = new NetConnectedClient[count];
        for (uint i = 0; i < count; ++i)
        {
            if (s_NetGetConnectedClientAt(i, out uint clientId, out IntPtr namePtr))
            {
                result[i] = new NetConnectedClient
                {
                    ClientId = clientId,
                    Name = NativeObject.Utf8ToString(namePtr) ?? string.Empty
                };
            }
        }
        return result;
    }

    internal static NetChatMessage[] Net_GetChatHistory()
    {
        EnsureInitialized();
        if (s_NetGetChatCount == null || s_NetGetChatAt == null)
            return Array.Empty<NetChatMessage>();

        uint count = s_NetGetChatCount();
        if (count == 0)
            return Array.Empty<NetChatMessage>();

        var result = new NetChatMessage[count];
        for (uint i = 0; i < count; ++i)
        {
            if (s_NetGetChatAt(i, out uint senderId, out IntPtr senderNamePtr, out IntPtr textPtr))
            {
                result[i] = new NetChatMessage
                {
                    SenderId = senderId,
                    SenderName = NativeObject.Utf8ToString(senderNamePtr) ?? string.Empty,
                    Text = NativeObject.Utf8ToString(textPtr) ?? string.Empty
                };
            }
        }
        return result;
    }

    internal static void Net_SetSimulationSettings(int lagMs, float lossPercent)
    {
        EnsureInitialized();
        s_NetSetSimulationSettings?.Invoke(lagMs, lossPercent);
    }
}
