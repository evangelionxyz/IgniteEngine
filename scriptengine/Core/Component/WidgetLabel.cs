// Copyright (c) 2026 Evangelion Manuhutu

using System;

namespace Ignite.Core;

/// <summary>
/// Managed wrapper for a WidgetLabel widget item found on an entity's WidgetComponent.
/// Use <see cref="Widget.GetLabel"/> to obtain an instance.
/// </summary>
public sealed class WidgetLabel
{
    private readonly ulong m_EntityID;
    private readonly string m_LabelName;

    internal WidgetLabel(ulong entityID, string labelName)
    {
        m_EntityID = entityID;
        m_LabelName = labelName;
    }

    /// <summary>Gets or sets the displayed text.</summary>
    public string Text
    {
        get => InternalCalls.WidgetComponent_GetLabelText(m_EntityID, m_LabelName);
        set => InternalCalls.WidgetComponent_SetLabelText(m_EntityID, m_LabelName, value ?? string.Empty);
    }

    /// <summary>Gets or sets the label's RGBA tint color.</summary>
    public Vector4 Color
    {
        get
        {
            InternalCalls.WidgetComponent_GetLabelColor(m_EntityID, m_LabelName, out Vector4 result);
            return result;
        }
        set => InternalCalls.WidgetComponent_SetLabelColor(m_EntityID, m_LabelName, value);
    }

    /// <summary>Gets or sets the font size in pixels.</summary>
    public float FontSize
    {
        get
        {
            InternalCalls.WidgetComponent_GetLabelFontSize(m_EntityID, m_LabelName, out float result);
            return result;
        }
        set => InternalCalls.WidgetComponent_SetLabelFontSize(m_EntityID, m_LabelName, value);
    }
}
