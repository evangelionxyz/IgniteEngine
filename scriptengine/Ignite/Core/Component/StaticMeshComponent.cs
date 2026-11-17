// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;

namespace Ignite;

public sealed class StaticMeshComponent : IComponent
{
    public ulong MeshHandle
    {
        get => ComponentInternalCalls.StaticMeshComponent_GetMeshHandle(Entity!.ID);
        set => ComponentInternalCalls.StaticMeshComponent_SetMeshHandle(Entity!.ID, value);
    }

    public void SetOverrideMaterial(int slot, ulong materialHandle)
        => ComponentInternalCalls.StaticMeshComponent_SetOverrideMaterial(Entity!.ID, slot, materialHandle);

    public ulong GetOverrideMaterial(int slot)
        => ComponentInternalCalls.StaticMeshComponent_GetOverrideMaterial(Entity!.ID, slot);
}

