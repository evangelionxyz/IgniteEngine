// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core;
namespace Ignite;

public class Widget : IComponent
{
    /// <summary>Returns a <see cref="WidgetButton"/> by name, or null if not found.</summary>
    public WidgetButton GetButton(string buttonName)
    {
        if (string.IsNullOrWhiteSpace(buttonName))
            return null;

        return InternalCalls.WidgetComponent_HasButton(Entity.ID, buttonName)
            ? new WidgetButton(Entity.ID, buttonName)
            : null;
    }

    /// <summary>Returns a <see cref="WidgetLabel"/> by name, or null if not found.</summary>
    public WidgetLabel GetLabel(string labelName)
    {
        if (string.IsNullOrWhiteSpace(labelName))
            return null;

        return InternalCalls.WidgetComponent_HasLabel(Entity.ID, labelName)
            ? new WidgetLabel(Entity.ID, labelName)
            : null;
    }

    /// <summary>Returns a <see cref="WidgetImage"/> by name, or null if not found.</summary>
    public WidgetImage GetImage(string imageName)
    {
        if (string.IsNullOrWhiteSpace(imageName))
            return null;

        return InternalCalls.WidgetComponent_HasImage(Entity.ID, imageName)
            ? new WidgetImage(Entity.ID, imageName)
            : null;
    }
}
