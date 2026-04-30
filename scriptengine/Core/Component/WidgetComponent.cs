// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public class WidgetComponent : IComponent
{
    public WidgetButton GetButton(string buttonName)
    {
        if (string.IsNullOrWhiteSpace(buttonName))
            return null;

        return ComponentInternalCalls.WidgetComponent_HasButton(Entity.ID, buttonName)
            ? new WidgetButton(Entity.ID, buttonName)
            : null;
    }

    public WidgetLabel GetLabel(string labelName)
    {
        if (string.IsNullOrWhiteSpace(labelName))
            return null;

        return ComponentInternalCalls.WidgetComponent_HasLabel(Entity.ID, labelName)
            ? new WidgetLabel(Entity.ID, labelName)
            : null;
    }

    public WidgetImage GetImage(string imageName)
    {
        if (string.IsNullOrWhiteSpace(imageName))
            return null;

        return ComponentInternalCalls.WidgetComponent_HasImage(Entity.ID, imageName)
            ? new WidgetImage(Entity.ID, imageName)
            : null;
    }
}
