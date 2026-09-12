using System;
using System.Numerics;

namespace Ignite.Managed.Models;

public enum ComponentType
{
    Transform, Camera, Sprite2D, Circle2D, StaticMesh, SkeletalMesh,
    DirectionalLight, PointLight, SpotLight, PointLight2D,
    Rigidbody, Rigidbody2D, BoxCollider, BoxCollider2D, SphereCollider,
    CapsuleCollider, CircleCollider2D, MeshCollider, CharacterController,
    AudioSource, Script, Widget, Text, WorldEnvironment,
    Animator2D, Arrow, Terrain, HeightFieldCollider, Prefab
}

public class ComponentModel
{
    public ComponentType Type { get; set; }
    public string DisplayName { get; set; } = string.Empty;
    public bool IsEnabled { get; set; } = true;
}

public class TransformData
{
    public Vector3 Position { get; set; } = Vector3.Zero;
    public Vector3 Rotation { get; set; } = Vector3.Zero;
    public Vector3 Scale { get; set; } = Vector3.One;
}

public class CameraData
{
    public bool IsPerspective { get; set; } = true;
    public float FieldOfView { get; set; } = 60.0f;
    public float NearClip { get; set; } = 0.1f;
    public float FarClip { get; set; } = 1000.0f;
    public float OrthographicSize { get; set; } = 10.0f;
}

public class LightData
{
    public Vector3 Color { get; set; } = Vector3.One;
    public float Intensity { get; set; } = 1.0f;
    public float Range { get; set; } = 10.0f;
    public float SpotAngle { get; set; } = 30.0f;
    public bool CastShadows { get; set; } = true;
}

public class RigidbodyData
{
    public float Mass { get; set; } = 1.0f;
    public float LinearDrag { get; set; } = 0.0f;
    public float AngularDrag { get; set; } = 0.05f;
    public bool UseGravity { get; set; } = true;
    public bool IsKinematic { get; set; } = false;
}

public class ColliderData
{
    public Vector3 Center { get; set; } = Vector3.Zero;
    public Vector3 Size { get; set; } = Vector3.One;
    public float Radius { get; set; } = 0.5f;
    public float Height { get; set; } = 1.0f;
    public bool IsTrigger { get; set; } = false;
}

public class SpriteData
{
    public Vector4 Color { get; set; } = new(1, 1, 1, 1);
    public string? TexturePath { get; set; }
    public Vector2 Tiling { get; set; } = Vector2.One;
}
