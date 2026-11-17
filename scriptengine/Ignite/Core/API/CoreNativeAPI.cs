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
        public IntPtr Input_GetMouseDelta;
        public IntPtr Input_SetMouseToCenter;
        public IntPtr Input_SetCursorMode;
        public IntPtr Input_GetCursorMode;
        public IntPtr Input_IsMouseOverUI;
        public IntPtr Input_IsActionPressed;

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
        public IntPtr Scene_TransitionTo;

        // Networking (NetworkManager)
        public IntPtr Net_IsConnected;
        public IntPtr Net_IsServerRunning;
        public IntPtr Net_IsConnecting;
        public IntPtr Net_GetClientId;
        public IntPtr Net_StartServer;
        public IntPtr Net_StartServerDefault;
        public IntPtr Net_StopServer;
        public IntPtr Net_Connect;
        public IntPtr Net_Disconnect;
        public IntPtr Net_SpawnEntity;
        public IntPtr Net_DespawnEntity;
        public IntPtr Net_SendInput;
        public IntPtr Net_SendChat;
        public IntPtr Net_KickClient;
        public IntPtr Net_GetStats;
        public IntPtr Net_GetServerTick;
        public IntPtr Net_GetClientServerTick;
        public IntPtr Net_GetClientCount;
        public IntPtr Net_GetConnectedClientAt;
        public IntPtr Net_GetChatCount;
        public IntPtr Net_GetChatAt;
        public IntPtr Net_SetSimulationSettings;
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

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetUInt32Fn(ulong entityID, out uint result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetUInt32Fn(ulong entityID, uint value);


        // ================================
        // Vectors & Quaternion
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetVector2Fn(ulong entityID, out NativeObject.Vector2 result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetVector2Fn(ulong entityID, ref NativeObject.Vector2 value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetVector3Fn(ulong entityID, out NativeObject.Vector3 value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetVector3Fn(ulong entityID, ref NativeObject.Vector3 value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetVector4Fn(ulong entityID, out NativeObject.Vector4 value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetVector4Fn(ulong entityID, ref NativeObject.Vector4 value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetQuaternionFn(ulong entityID, out NativeObject.Quaternion result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetQuaternionFn(ulong entityID, ref NativeObject.Quaternion value);

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
        public delegate void InputGetMouseDeltaFn(out NativeObject.Vector2 result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void InputSetMouseToCenterFn();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void InputSetCursorModeFn(int mode);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate int InputGetCursorModeFn();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool InputIsMouseOverUIFn();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool InputIsActionPressedFn(IntPtr actionName);

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

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SceneLoadFn(ulong sceneAssetHandle);

        // Networking delegates
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool NetBoolQueryFn();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate uint NetUIntQueryFn();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool NetStartServerFn(ushort port, ushort tickRate, ushort snapshotRate);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool NetStartServerDefaultFn();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void NetActionFn();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool NetConnectFn(IntPtr address, ushort port, IntPtr playerName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate ulong NetSpawnEntityFn(IntPtr templateOrPrefab, ref NativeObject.Vector3 position, ref NativeObject.Quaternion rotation, uint ownerClient);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void NetDespawnEntityFn(ulong netId);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void NetSendInputFn(ref NativeObject.Vector2 move, ushort buttons, float yaw);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void NetSendChatFn(IntPtr text);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void NetKickClientFn(uint clientId, IntPtr reason);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void NetGetStatsFn(out int outRttMs, out float outPacketLoss, out float outInKBps, out float outOutKBps, out uint outClientCount);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool NetGetConnectedClientAtFn(uint index, out uint outClientId, out IntPtr outName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool NetGetChatAtFn(uint index, out uint outSenderId, out IntPtr outSenderName, out IntPtr outText);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void NetSetSimulationSettingsFn(int lagMs, float lossPercent);
    }
}
