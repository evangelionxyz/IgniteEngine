// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public class WidgetComponent : IComponent
{
    public bool CanvasEnabled
    {
        get => ComponentInternalCalls.WidgetComponent_GetCanvasEnabled(Entity!.ID);
        set => ComponentInternalCalls.WidgetComponent_SetCanvasEnabled(Entity!.ID, value);
    }

    public bool HasItem(string itemName)
    {
        if (string.IsNullOrWhiteSpace(itemName))
            return false;
        return ComponentInternalCalls.WidgetComponent_HasItem(Entity!.ID, itemName);
    }

    public void SetItemVisible(string itemName, bool visible)
    {
        if (!string.IsNullOrWhiteSpace(itemName))
            ComponentInternalCalls.WidgetComponent_SetItemVisible(Entity!.ID, itemName, visible);
    }

    public bool IsItemVisible(string itemName)
    {
        if (string.IsNullOrWhiteSpace(itemName))
            return false;
        return ComponentInternalCalls.WidgetComponent_GetItemVisible(Entity!.ID, itemName);
    }

    public WidgetPanel? GetPanel(string panelName)
    {
        if (string.IsNullOrWhiteSpace(panelName))
            return null;

        return ComponentInternalCalls.WidgetComponent_HasItem(Entity!.ID, panelName)
            ? new WidgetPanel(Entity!.ID, panelName)
            : null;
    }

    public WidgetButton? GetButton(string buttonName)
    {
        if (string.IsNullOrWhiteSpace(buttonName))
            return null;

        return ComponentInternalCalls.WidgetComponent_HasButton(Entity!.ID, buttonName)
            ? new WidgetButton(Entity!.ID, buttonName)
            : null;
    }

    public WidgetLabel? GetLabel(string labelName)
    {
        if (string.IsNullOrWhiteSpace(labelName))
            return null;

        return ComponentInternalCalls.WidgetComponent_HasLabel(Entity!.ID, labelName)
            ? new WidgetLabel(Entity!.ID, labelName)
            : null;
    }

    public WidgetImage? GetImage(string imageName)
    {
        if (string.IsNullOrWhiteSpace(imageName))
            return null;

        return ComponentInternalCalls.WidgetComponent_HasImage(Entity!.ID, imageName)
            ? new WidgetImage(Entity!.ID, imageName)
            : null;
    }
}
