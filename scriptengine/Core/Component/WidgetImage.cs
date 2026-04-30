// Copyright (c) 2026 Evangelion Manuhutu

namespace Ignite.Core;

/// <summary>
/// Managed wrapper for a WidgetImage widget item found on an entity's WidgetComponent.
/// Use <see cref="Widget.GetImage"/> to obtain an instance.
/// </summary>
public sealed class WidgetImage
{
    private readonly ulong m_EntityID;
    private readonly string m_ImageName;

    internal WidgetImage(ulong entityID, string imageName)
    {
        m_EntityID = entityID;
        m_ImageName = imageName;
    }

    /// <summary>Gets or sets the texture asset handle used by this image widget.</summary>
    public ulong ImageHandle
    {
        get => InternalCalls.WidgetComponent_GetImageHandle(m_EntityID, m_ImageName);
        set => InternalCalls.WidgetComponent_SetImageHandle(m_EntityID, m_ImageName, value);
    }
}
