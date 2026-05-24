// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;

using static Ignite.Mathf;
namespace Ignite;

public class Camera : Entity
{
    internal Camera(ulong id) : base(id) { }
    public Camera() : base(0) { }

    public static Camera? main
    {
        get
        {
            ulong cameraID = ComponentInternalCalls.Scene_GetPrimaryCamera();
            if (cameraID == 0)
                return null;
            return new Camera(cameraID);
        }
    }

    public Ray ScreenPointToRay(Vector2 screenPosition)
    {
        return Physics.ScreenToWorldRay(screenPosition);
    }

    public Ray ScreenPointToRay(Vector3 screenPosition)
    {
        return Physics.ScreenToWorldRay(new Vector2(screenPosition.X, screenPosition.Y));
    }
}
