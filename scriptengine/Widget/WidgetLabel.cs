// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core;
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
        get => InternalCalls.WidgetComponent_GetLabelText(m_EntityID, m_LabelName);
        set => InternalCalls.WidgetComponent_SetLabelText(m_EntityID, m_LabelName, value ?? string.Empty);
    }

    public Vector4 Color
    {
        get
        {
            InternalCalls.WidgetComponent_GetLabelColor(m_EntityID, m_LabelName, out Vector4 result);
            return result;
        }
        set => InternalCalls.WidgetComponent_SetLabelColor(m_EntityID, m_LabelName, value);
    }

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
