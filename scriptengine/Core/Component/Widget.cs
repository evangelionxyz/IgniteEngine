namespace Ignite;

public class Widget : IComponent
{
    public WidgetButton GetButton(string buttonName)
    {
        if (string.IsNullOrWhiteSpace(buttonName))
        {
            return null;
        }

        return InternalCalls.WidgetComponent_HasButton(Entity.ID, buttonName)
            ? new WidgetButton(Entity.ID, buttonName)
            : null;
    }
}
