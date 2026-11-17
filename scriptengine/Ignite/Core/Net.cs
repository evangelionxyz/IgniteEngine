// Copyright (c) 2026 Evangelion Manuhutu

using System;
using Ignite.Core;
using static Ignite.Mathf;

namespace Ignite;

[Flags]
public enum NetInputButtons : ushort
{
    None    = 0,
    Jump    = 1 << 0,
    Sprint  = 1 << 1,
    Fire    = 1 << 2,
    Use     = 1 << 3,
    Respawn = 1 << 4,
    Bounce  = 1 << 5
}

public struct NetStats
{
    public int RttMs;
    public float PacketLossPercent;
    public float InKBytesPerSec;
    public float OutKBytesPerSec;
    public uint ClientCount;
}

public struct NetConnectedClient
{
    public uint ClientId;
    public string Name;
}

public struct NetChatMessage
{
    public uint SenderId;
    public string SenderName;
    public string Text;
}

/// <summary>
/// Managed wrapper for Ignite's client/server NetworkManager subsystem.
/// </summary>
public static class Net
{
    public static bool IsConnected => CoreInternalCalls.Net_IsConnected();
    public static bool IsServerRunning => CoreInternalCalls.Net_IsServerRunning();
    public static bool IsConnecting => CoreInternalCalls.Net_IsConnecting();

    public static uint GetClientId() => CoreInternalCalls.Net_GetClientId();

    public static bool StartServer() => CoreInternalCalls.Net_StartServer();

    public static bool StartServer(ushort port, ushort tickRate = 60, ushort snapshotRate = 20)
        => CoreInternalCalls.Net_StartServer(port, tickRate, snapshotRate);

    public static void StopServer() => CoreInternalCalls.Net_StopServer();

    public static bool Connect(string address = "127.0.0.1", ushort port = 8192, string playerName = "Player")
        => CoreInternalCalls.Net_Connect(address, port, playerName);

    public static void Disconnect() => CoreInternalCalls.Net_Disconnect();

    public static Entity? SpawnEntity(string templateOrPrefab, Vector3 position, Quaternion rotation, uint ownerClient = 0)
    {
        ulong id = CoreInternalCalls.Net_SpawnEntity(templateOrPrefab, position, rotation, ownerClient);
        return id != 0 ? new Entity(id) : null;
    }

    public static Entity? SpawnEntity(string templateOrPrefab, Vector3 position, float yawDeg, float pitchDeg = 0.0f, uint ownerClient = 0)
    {
        const float degToRad = 0.017453292519943295f;
        Quaternion rot = Quaternion.FromEuler(pitchDeg * degToRad, yawDeg * degToRad, 0.0f);
        return SpawnEntity(templateOrPrefab, position, rot, ownerClient);
    }

    public static void DespawnEntity(ulong netId) => CoreInternalCalls.Net_DespawnEntity(netId);

    public static void SendInput(Vector2 move, ushort buttons = 0, float yaw = 0.0f)
        => CoreInternalCalls.Net_SendInput(move, buttons, yaw);

    public static void SendInput(Vector2 move, NetInputButtons buttons, float yaw = 0.0f)
        => CoreInternalCalls.Net_SendInput(move, (ushort)buttons, yaw);

    public static void SendChat(string text) => CoreInternalCalls.Net_SendChat(text);

    public static void KickClient(uint clientId, string reason = "")
        => CoreInternalCalls.Net_KickClient(clientId, reason);

    public static NetStats GetStats() => CoreInternalCalls.Net_GetStats();
    public static uint GetServerTick() => CoreInternalCalls.Net_GetServerTick();
    public static uint GetClientServerTick() => CoreInternalCalls.Net_GetClientServerTick();
    public static uint GetClientCount() => CoreInternalCalls.Net_GetClientCount();
    public static NetConnectedClient[] GetConnectedClients() => CoreInternalCalls.Net_GetConnectedClients();
    public static NetChatMessage[] GetChatHistory() => CoreInternalCalls.Net_GetChatHistory();

    public static void SetSimulationSettings(int lagMs, float lossPercent)
        => CoreInternalCalls.Net_SetSimulationSettings(lagMs, lossPercent);
}
