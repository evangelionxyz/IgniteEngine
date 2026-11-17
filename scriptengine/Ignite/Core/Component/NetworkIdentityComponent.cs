// Copyright (c) 2026 Evangelion Manuhutu

using System;
using Ignite.Core.Component;

namespace Ignite;

[Flags]
public enum NetCompFlag : uint
{
    None                = 0,
    Transform           = 1 << 0,
    PointLight          = 1 << 1,
    DirectionalLight    = 1 << 2,
    SpotLight           = 1 << 3,
    Sprite2D            = 1 << 4,
    Circle2D            = 1 << 5,
    Text                = 1 << 6,
    StaticMesh          = 1 << 7,
    AudioSource         = 1 << 8,
    Rigidbody           = 1 << 9,
    Camera              = 1 << 10,
    CharacterController = 1 << 11,
    All                 = 0xFFFFFFFF
}

public sealed class NetworkIdentityComponent : IComponent
{
    public ulong NetId => ComponentInternalCalls.NetworkIdentityComponent_GetNetId(Entity!.ID);

    public uint OwnerClient
    {
        get => ComponentInternalCalls.NetworkIdentityComponent_GetOwnerClient(Entity!.ID);
        set => ComponentInternalCalls.NetworkIdentityComponent_SetOwnerClient(Entity!.ID, value);
    }

    public bool IsLocalPlayer
    {
        get => ComponentInternalCalls.NetworkIdentityComponent_GetIsLocalPlayer(Entity!.ID);
        set => ComponentInternalCalls.NetworkIdentityComponent_SetIsLocalPlayer(Entity!.ID, value);
    }

    public bool Replicate
    {
        get => ComponentInternalCalls.NetworkIdentityComponent_GetReplicate(Entity!.ID);
        set => ComponentInternalCalls.NetworkIdentityComponent_SetReplicate(Entity!.ID, value);
    }

    public string Prefab
    {
        get => ComponentInternalCalls.NetworkIdentityComponent_GetPrefab(Entity!.ID);
        set => ComponentInternalCalls.NetworkIdentityComponent_SetPrefab(Entity!.ID, value ?? string.Empty);
    }

    public float ReplicatedMovementSpeed
    {
        get => ComponentInternalCalls.NetworkIdentityComponent_GetReplicatedMovementSpeed(Entity!.ID);
        set => ComponentInternalCalls.NetworkIdentityComponent_SetReplicatedMovementSpeed(Entity!.ID, value);
    }

    public bool ReplicatedIsGrounded
    {
        get => ComponentInternalCalls.NetworkIdentityComponent_GetReplicatedIsGrounded(Entity!.ID);
        set => ComponentInternalCalls.NetworkIdentityComponent_SetReplicatedIsGrounded(Entity!.ID, value);
    }

    public NetCompFlag SyncComponentsMask
    {
        get => (NetCompFlag)ComponentInternalCalls.NetworkIdentityComponent_GetSyncComponentsMask(Entity!.ID);
        set => ComponentInternalCalls.NetworkIdentityComponent_SetSyncComponentsMask(Entity!.ID, (uint)value);
    }

    public void MarkDirty(NetCompFlag flags = NetCompFlag.All)
        => ComponentInternalCalls.NetworkIdentityComponent_MarkDirty(Entity!.ID, (uint)flags);
}

