// Copyright (c) 2026 Evangelion Manuhutu

using System;
using Ignite.Core;
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

        InternalCalls.WidgetComponent_AddButtonEventCallback(
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

        InternalCalls.WidgetComponent_RemoveButtonEventCallback(
            m_EntityId,
            m_ButtonName,
            (int)eventType,
            callback.Method.Name);
    }
}
