// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class WidgetLabel
{
    private readonly ulong m_EntityID;
    private readonly string m_LabelName;

    internal WidgetLabel(ulong entityID, string labelName)
    {
        m_EntityID = entityID;
        m_LabelName = labelName;
    }

    public string Text
    {
        get => ComponentInternalCalls.WidgetComponent_GetLabelText(m_EntityID, m_LabelName);
        set => ComponentInternalCalls.WidgetComponent_SetLabelText(m_EntityID, m_LabelName, value ?? string.Empty);
    }

    public Vector4 Color
    {
        get
        {
            ComponentInternalCalls.WidgetComponent_GetLabelColor(m_EntityID, m_LabelName, out Vector4 result);
            return result;
        }
        set => ComponentInternalCalls.WidgetComponent_SetLabelColor(m_EntityID, m_LabelName, value);
    }

    public float FontSize
    {
        get
        {
            ComponentInternalCalls.WidgetComponent_GetLabelFontSize(m_EntityID, m_LabelName, out float result);
            return result;
        }
        set => ComponentInternalCalls.WidgetComponent_SetLabelFontSize(m_EntityID, m_LabelName, value);
    }
}
