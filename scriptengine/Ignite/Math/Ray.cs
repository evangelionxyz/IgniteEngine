// Copyright (c) 2026 Evangelion Manuhutu

namespace Ignite;

public struct Ray
{
    public Mathf.Vector3 Origin;
    public Mathf.Vector3 Direction;

    public Ray(Mathf.Vector3 origin, Mathf.Vector3 direction)
    {
        Origin = origin;
        Direction = direction;
    }
}
