// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class WidgetImage
{
    private readonly ulong m_EntityID;
    private readonly string m_ImageName;

    internal WidgetImage(ulong entityID, string imageName)
    {
        m_EntityID = entityID;
        m_ImageName = imageName;
    }

    public ulong ImageHandle
    {
        get => ComponentInternalCalls.WidgetComponent_GetImageHandle(m_EntityID, m_ImageName);
        set => ComponentInternalCalls.WidgetComponent_SetImageHandle(m_EntityID, m_ImageName, value);
    }

    public bool Visible
    {
        get => ComponentInternalCalls.WidgetComponent_GetItemVisible(m_EntityID, m_ImageName);
        set => ComponentInternalCalls.WidgetComponent_SetItemVisible(m_EntityID, m_ImageName, value);
    }

    public Vector4 Color
    {
        get => ComponentInternalCalls.WidgetComponent_GetItemBackgroundColor(m_EntityID, m_ImageName);
        set => ComponentInternalCalls.WidgetComponent_SetItemBackgroundColor(m_EntityID, m_ImageName, value);
    }

    public Vector2 Size
    {
        get => ComponentInternalCalls.WidgetComponent_GetItemSize(m_EntityID, m_ImageName);
        set => ComponentInternalCalls.WidgetComponent_SetItemSize(m_EntityID, m_ImageName, value);
    }

    public Vector2 Position
    {
        get => ComponentInternalCalls.WidgetComponent_GetItemPosition(m_EntityID, m_ImageName);
        set => ComponentInternalCalls.WidgetComponent_SetItemPosition(m_EntityID, m_ImageName, value);
    }
}
