// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class WidgetPanel
{
    private readonly ulong m_EntityID;
    private readonly string m_ItemName;

    internal WidgetPanel(ulong entityID, string itemName)
    {
        m_EntityID = entityID;
        m_ItemName = itemName;
    }

    public string Name => m_ItemName;

    public bool Visible
    {
        get => ComponentInternalCalls.WidgetComponent_GetItemVisible(m_EntityID, m_ItemName);
        set => ComponentInternalCalls.WidgetComponent_SetItemVisible(m_EntityID, m_ItemName, value);
    }

    public Vector4 BackgroundColor
    {
        get => ComponentInternalCalls.WidgetComponent_GetItemBackgroundColor(m_EntityID, m_ItemName);
        set => ComponentInternalCalls.WidgetComponent_SetItemBackgroundColor(m_EntityID, m_ItemName, value);
    }

    public Vector4 BorderColor
    {
        get => ComponentInternalCalls.WidgetComponent_GetItemBorderColor(m_EntityID, m_ItemName);
        set => ComponentInternalCalls.WidgetComponent_SetItemBorderColor(m_EntityID, m_ItemName, value);
    }

    public float Opacity
    {
        get => ComponentInternalCalls.WidgetComponent_GetItemOpacity(m_EntityID, m_ItemName);
        set => ComponentInternalCalls.WidgetComponent_SetItemOpacity(m_EntityID, m_ItemName, value);
    }

    public Vector2 Size
    {
        get => ComponentInternalCalls.WidgetComponent_GetItemSize(m_EntityID, m_ItemName);
        set => ComponentInternalCalls.WidgetComponent_SetItemSize(m_EntityID, m_ItemName, value);
    }

    public Vector2 Position
    {
        get => ComponentInternalCalls.WidgetComponent_GetItemPosition(m_EntityID, m_ItemName);
        set => ComponentInternalCalls.WidgetComponent_SetItemPosition(m_EntityID, m_ItemName, value);
    }
}
