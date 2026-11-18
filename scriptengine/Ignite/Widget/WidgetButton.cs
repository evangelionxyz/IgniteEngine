// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using System;

namespace Ignite;

internal enum WidgetButtonEventType
{
    Click = 0,
    Pressed = 1,
    Released = 2,
    HoverEnter = 3,
    HoverExit = 4,
}

public sealed class WidgetButton
{
    private readonly ulong m_EntityId;
    private readonly string m_ButtonName;

    internal WidgetButton(ulong entityId, string buttonName)
    {
        m_EntityId = entityId;
        m_ButtonName = buttonName;
    }

    public string Name => m_ButtonName;

    public string Text
    {
        get => ComponentInternalCalls.WidgetComponent_GetButtonText(m_EntityId, m_ButtonName);
        set => ComponentInternalCalls.WidgetComponent_SetButtonText(m_EntityId, m_ButtonName, value ?? string.Empty);
    }

    public bool Visible
    {
        get => ComponentInternalCalls.WidgetComponent_GetItemVisible(m_EntityId, m_ButtonName);
        set => ComponentInternalCalls.WidgetComponent_SetItemVisible(m_EntityId, m_ButtonName, value);
    }

    public bool Interactable
    {
        get => ComponentInternalCalls.WidgetComponent_GetButtonInteractable(m_EntityId, m_ButtonName);
        set => ComponentInternalCalls.WidgetComponent_SetButtonInteractable(m_EntityId, m_ButtonName, value);
    }

    public bool IsHovered => ComponentInternalCalls.WidgetComponent_IsButtonHovered(m_EntityId, m_ButtonName);

    public Mathf.Vector2 Size
    {
        get => ComponentInternalCalls.WidgetComponent_GetItemSize(m_EntityId, m_ButtonName);
        set => ComponentInternalCalls.WidgetComponent_SetItemSize(m_EntityId, m_ButtonName, value);
    }

    public Mathf.Vector2 Position
    {
        get => ComponentInternalCalls.WidgetComponent_GetItemPosition(m_EntityId, m_ButtonName);
        set => ComponentInternalCalls.WidgetComponent_SetItemPosition(m_EntityId, m_ButtonName, value);
    }

    public void SetColors(Mathf.Vector4 normalColor, Mathf.Vector4 hoverColor, Mathf.Vector4 pressedColor)
        => ComponentInternalCalls.WidgetComponent_SetButtonColors(m_EntityId, m_ButtonName, normalColor, hoverColor, pressedColor);

    public event Action OnClickEvent
    {
        add => AddCallback(WidgetButtonEventType.Click, value);
        remove => RemoveCallback(WidgetButtonEventType.Click, value);
    }

    public event Action OnPressedEvent
    {
        add => AddCallback(WidgetButtonEventType.Pressed, value);
        remove => RemoveCallback(WidgetButtonEventType.Pressed, value);
    }

    public event Action OnReleasedEvent
    {
        add => AddCallback(WidgetButtonEventType.Released, value);
        remove => RemoveCallback(WidgetButtonEventType.Released, value);
    }

    public event Action OnHoverEnterEvent
    {
        add => AddCallback(WidgetButtonEventType.HoverEnter, value);
        remove => RemoveCallback(WidgetButtonEventType.HoverEnter, value);
    }

    public event Action OnHoverExitEvent
    {
        add => AddCallback(WidgetButtonEventType.HoverExit, value);
        remove => RemoveCallback(WidgetButtonEventType.HoverExit, value);
    }

    private void AddCallback(WidgetButtonEventType eventType, Action callback)
    {
        if (callback == null)
        {
            return;
        }

        ComponentInternalCalls.WidgetComponent_AddButtonEventCallback(
            m_EntityId,
            m_ButtonName,
            (int)eventType,
            callback.Method.Name);
    }

    private void RemoveCallback(WidgetButtonEventType eventType, Action callback)
    {
        if (callback == null)
        {
            return;
        }

        ComponentInternalCalls.WidgetComponent_RemoveButtonEventCallback(
            m_EntityId,
            m_ButtonName,
            (int)eventType,
            callback.Method.Name);
    }
}
