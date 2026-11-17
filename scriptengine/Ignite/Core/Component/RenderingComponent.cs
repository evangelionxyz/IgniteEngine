// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;

namespace Ignite;

public sealed class RenderingComponent : IComponent
{
    public bool Visible
    {
        get => ComponentInternalCalls.RenderingComponent_GetVisible(Entity!.ID);
        set => ComponentInternalCalls.RenderingComponent_SetVisible(Entity!.ID, value);
    }
}

