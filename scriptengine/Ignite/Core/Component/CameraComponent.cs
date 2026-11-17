// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;

namespace Ignite;

public enum CameraProjectionType : int
{
    Orthographic = 0,
    Perspective = 1
}

public enum CameraAspectRatioPreset : int
{
    Free = 0,
    Ratio16x9 = 1,
    Ratio16x10 = 2,
    Ratio4x3 = 3,
    Ratio21x9 = 4,
    Ratio1x1 = 5
}

public sealed class CameraComponent : IComponent
{
    public bool Primary
    {
        get => ComponentInternalCalls.CameraComponent_GetPrimary(Entity!.ID);
        set => ComponentInternalCalls.CameraComponent_SetPrimary(Entity!.ID, value);
    }

    public CameraProjectionType ProjectionType
    {
        get => (CameraProjectionType)ComponentInternalCalls.CameraComponent_GetProjectionType(Entity!.ID);
        set => ComponentInternalCalls.CameraComponent_SetProjectionType(Entity!.ID, (int)value);
    }

    public float Fov
    {
        get => ComponentInternalCalls.CameraComponent_GetFov(Entity!.ID);
        set => ComponentInternalCalls.CameraComponent_SetFov(Entity!.ID, value);
    }

    public float OrthoSize
    {
        get => ComponentInternalCalls.CameraComponent_GetOrthoSize(Entity!.ID);
        set => ComponentInternalCalls.CameraComponent_SetOrthoSize(Entity!.ID, value);
    }

    public float NearClip
    {
        get => ComponentInternalCalls.CameraComponent_GetNearClip(Entity!.ID);
        set => ComponentInternalCalls.CameraComponent_SetNearClip(Entity!.ID, value);
    }

    public float FarClip
    {
        get => ComponentInternalCalls.CameraComponent_GetFarClip(Entity!.ID);
        set => ComponentInternalCalls.CameraComponent_SetFarClip(Entity!.ID, value);
    }

    public CameraAspectRatioPreset AspectRatioPreset
    {
        get => (CameraAspectRatioPreset)ComponentInternalCalls.CameraComponent_GetAspectRatioPreset(Entity!.ID);
        set => ComponentInternalCalls.CameraComponent_SetAspectRatioPreset(Entity!.ID, (int)value);
    }
}

