// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;

namespace Ignite;

public static class Physics
{
    public static Ray ScreenToWorldRay(Mathf.Vector2 screenPosition)
    {
        ComponentInternalCalls.Scene_GetScreenToWorldRay(screenPosition.X, screenPosition.Y, out Mathf.Vector3 origin, out Mathf.Vector3 direction);
        return new Ray(origin, direction);
    }

    public static Entity Raycast(Ray ray)
    {
        ulong entityID = ComponentInternalCalls.Scene_Raycast(ray.Origin, ray.Direction);
        if (entityID == 0)
            return null;
        return new Entity(entityID);
    }
}
