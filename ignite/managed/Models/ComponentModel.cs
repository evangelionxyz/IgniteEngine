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
    public object? Data { get; set; }
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

public class DirectionalLightData
{
    public Vector4 Color { get; set; } = Vector4.One;
    public float Intensity { get; set; } = 1.0f;
    public float ShadowDistance { get; set; } = 200.0f;

    public bool CastShadows { get; set; } = true;
}

public class PointLightData
{
    public Vector4 Color { get; set; } = Vector4.One;
    public float Intensity { get; set; } = 1.0f;
    public float Range { get; set; } = 10.0f;
    public bool Enabled { get; set; } = true;
}

public class SpotLightData
{
    public Vector4 Color { get; set; } = Vector4.One;
    public float Intensity { get; set; } = 1.0f;
    public float Range { get; set; } = 10.0f;
    public float InnerCone { get; set; } = 12.5f;
    public float OuterCone { get; set; } = 45.0f;
    public bool Enabled { get; set; } = true;
}

public class PointLight2DData
{
    public Vector4 Color { get; set; } = Vector4.One;
    public float Radius { get; set; } = 5.0f;
    public float Intensity { get; set; } = 1.0f;
    public bool Enabled { get; set; } = true;
}

public class SpriteData
{
    public Vector4 Color { get; set; } = new(1, 1, 1, 1);
    public string? TexturePath { get; set; }
    public Vector2 Tiling { get; set; } = Vector2.One;
    public bool FlipX { get; set; } = false;
    public bool FlipY { get; set; } = false;
}

public class Circle2DData
{
    public Vector4 Color { get; set; } = new(1, 1, 1, 1);
    public float Thickness { get; set; } = 1.0f;
    public float Fade { get; set; } = 0.005f;
}

public class RigidbodyData
{
    public int BodyType { get; set; } = 0; // 0=Static, 1=Dynamic, 2=Kinematic
    public float Mass { get; set; } = 1.0f;
    public float LinearDamping { get; set; } = 0.0f;
    public float AngularDamping { get; set; } = 0.05f;
    public float Friction { get; set; } = 0.2f;
    public float Restitution { get; set; } = 0.0f;
    public bool UseGravity { get; set; } = true;

}

public class Rigidbody2DData
{
    public int BodyType { get; set; } = 0; // 0=Static, 1=Dynamic, 2=Kinematic
    public float GravityScale { get; set; } = 1.0f;
    public float LinearDamping { get; set; } = 0.6f;
    public float AngularDamping { get; set; } = 0.2f;
    public bool FixedRotation { get; set; } = false;
    public bool IsAwake { get; set; } = true;
    public bool IsEnabled { get; set; } = true;
}

public class BoxColliderData
{
    public Vector3 Center { get; set; } = Vector3.Zero;
    public Vector3 Size { get; set; } = Vector3.One;
}

public class SphereColliderData
{
    public Vector3 Center { get; set; } = Vector3.Zero;
    public float Radius { get; set; } = 1.0f;
}

public class CapsuleColliderData
{
    public Vector3 Center { get; set; } = Vector3.Zero;
    public float Radius { get; set; } = 0.5f;
    public float Height { get; set; } = 2.0f;

}

public class BoxCollider2DData
{
    public Vector2 Offset { get; set; } = Vector2.Zero;
    public Vector2 Size { get; set; } = new(0.5f, 0.5f);
    public float Density { get; set; } = 1.0f;
    public float Friction { get; set; } = 0.5f;
    public float Restitution { get; set; } = 0.1f;
    public bool IsSensor { get; set; } = false;
}

public class CircleCollider2DData
{
    public Vector2 Center { get; set; } = Vector2.Zero;
    public float Radius { get; set; } = 0.5f;
    public float Density { get; set; } = 1.0f;
    public float Friction { get; set; } = 0.5f;
    public float Restitution { get; set; } = 0.1f;
    public bool IsSensor { get; set; } = false;
}

public class CharacterControllerData
{
    public float Radius { get; set; } = 0.5f;
    public float Height { get; set; } = 2.0f;
    public float MaxStepHeight { get; set; } = 0.4f;
    public float MaxSlopeAngle { get; set; } = 45.0f;
    public float Mass { get; set; } = 80.0f;
    public float Friction { get; set; } = 0.2f;
}

public class AudioSourceData
{
    public float Volume { get; set; } = 1.0f;
    public float Pitch { get; set; } = 1.0f;
    public float Pan { get; set; } = 0.0f;
    public bool PlayOnStart { get; set; } = false;
    public bool Loop { get; set; } = false;
}

public class TextData
{
    public string Text { get; set; } = "Empty";
    public Vector4 Color { get; set; } = Vector4.One;
    public float Kerning { get; set; } = 0.0f;
    public float LineSpacing { get; set; } = -0.025f;
    public bool ScreenSpace { get; set; } = false;
}

public class WorldEnvironmentData
{
    public float Exposure { get; set; } = 1.1f;
    public float Gamma { get; set; } = 2.2f;
    public float Ambient { get; set; } = 0.5f;
    public float FogDensity { get; set; } = 0.0f;
    public Vector4 FogColor { get; set; } = new(0.5f, 0.6f, 0.7f, 1.0f);
    public float FogStart { get; set; } = 10.0f;
    public float FogEnd { get; set; } = 100.0f;
}

public class ScriptData
{
    public string ClassName { get; set; } = string.Empty;
}

public class LightData
{
    public Vector3 Color { get; set; } = Vector3.One;
    public float Intensity { get; set; } = 1.0f;
    public float Range { get; set; } = 10.0f;
    public float SpotAngle { get; set; } = 30.0f;
    public bool CastShadows { get; set; } = true;
}

public class ColliderData
{
    public Vector3 Center { get; set; } = Vector3.Zero;
    public Vector3 Size { get; set; } = Vector3.One;
    public float Radius { get; set; } = 0.5f;
    public float Height { get; set; } = 1.0f;
    public bool IsTrigger { get; set; } = false;
}
