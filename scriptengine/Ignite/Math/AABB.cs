// Copyright (c) 2026 Evangelion Manuhutu

namespace Ignite;

public struct AABB
{
    public Mathf.Vector3 Min;
    public Mathf.Vector3 Max;

    public AABB(Mathf.Vector3 min, Mathf.Vector3 max)
    {
        Min = min;
        Max = max;
    }

    public Mathf.Vector3 Center => (Min + Max) * 0.5f;
    public Mathf.Vector3 Size => Max - Min;

    public bool IntersectRay(Ray ray, out float distance)
    {
        distance = 0.0f;
        Mathf.Vector3 invDir = new Mathf.Vector3(1.0f / ray.Direction.X, 1.0f / ray.Direction.Y, 1.0f / ray.Direction.Z);
        Mathf.Vector3 tMin = (Min - ray.Origin) * invDir;
        Mathf.Vector3 tMax = (Max - ray.Origin) * invDir;
        Mathf.Vector3 t1 = Mathf.Vector3.Min(tMin, tMax);
        Mathf.Vector3 t2 = Mathf.Vector3.Max(tMin, tMax);
        float tNear = Mathf.Max(Mathf.Max(t1.X, t1.Y), t1.Z);
        float tFar = Mathf.Min(Mathf.Min(t2.X, t2.Y), t2.Z);
        
        if (tNear > tFar || tFar <= 0)
            return false;
        
        distance = tNear;
        return true;
    }
}
