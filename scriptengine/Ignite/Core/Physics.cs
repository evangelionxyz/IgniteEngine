// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;

namespace Ignite;

/// <summary>Carries the result of a physics (Jolt narrow-phase) raycast.</summary>
public struct RaycastHit
{
    /// <summary>The entity that was hit (null if nothing was hit).</summary>
    public Entity? Entity;

    /// <summary>World-space position of the contact point.</summary>
    public Mathf.Vector3 Point;

    /// <summary>World-space surface normal at the contact point.</summary>
    public Mathf.Vector3 Normal;

    /// <summary>True when something was hit.</summary>
    public bool IsHit => Entity != null;
}

public static class Physics
{
    public static Ray ScreenToWorldRay(Mathf.Vector2 screenPosition)
    {
        ComponentInternalCalls.Scene_GetScreenToWorldRay(screenPosition.X, screenPosition.Y, out Mathf.Vector3 origin, out Mathf.Vector3 direction);
        return new Ray(origin, direction);
    }

    /// <summary>
    /// Broad-phase raycast against mesh AABBs and 2D quads.
    /// Fast but imprecise — use <see cref="PhysicsRaycast"/> for solid-body collisions.
    /// </summary>
    public static Entity? Raycast(Ray ray)
    {
        ulong entityID = ComponentInternalCalls.Scene_Raycast(ray.Origin, ray.Direction);
        if (entityID == 0)
            return null;
        return new Entity(entityID);
    }

    /// <summary>
    /// Narrow-phase raycast using Jolt physics. Only hits entities with a Rigidbody
    /// and collider component. Returns rich hit information.
    /// </summary>
    /// <param name="ray">The ray to cast.</param>
    /// <param name="maxDistance">Maximum ray length in world units.</param>
    /// <param name="hit">Populated with hit entity, point and normal on success.</param>
    /// <returns>True if anything was hit.</returns>
    public static bool PhysicsRaycast(Ray ray, float maxDistance, out RaycastHit hit)
    {
        ulong entityID = ComponentInternalCalls.Scene_PhysicsRaycast(
            ray.Origin, ray.Direction, maxDistance,
            out Mathf.Vector3 hitPoint, out Mathf.Vector3 hitNormal);

        if (entityID == 0)
        {
            hit = default;
            return false;
        }

        hit = new RaycastHit
        {
            Entity = new Entity(entityID),
            Point  = hitPoint,
            Normal = hitNormal,
        };
        return true;
    }

    /// <summary>
    /// Narrow-phase raycast using Jolt physics. Simple overload that only returns the hit entity.
    /// </summary>
    public static Entity? PhysicsRaycast(Ray ray, float maxDistance = 1000f)
    {
        ulong entityID = ComponentInternalCalls.Scene_PhysicsRaycast(
            ray.Origin, ray.Direction, maxDistance,
            out _, out _);
        return entityID != 0 ? new Entity(entityID) : null;
    }
}
