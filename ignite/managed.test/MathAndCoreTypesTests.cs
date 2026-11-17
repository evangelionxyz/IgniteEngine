// Copyright (c) 2026 Evangelion Manuhutu

using NUnit.Framework;
using static Ignite.Mathf;

namespace Ignite.Managed.Test;

[TestFixture]
public class MathAndCoreTypesTests
{
    [Test]
    public void Vector3OperationsAndNormalization()
    {
        var a = new Vector3(3.0f, 0.0f, 4.0f);
        Assert.That(a.Length(), Is.EqualTo(5.0f).Within(1e-5f));
        Assert.That(a.LengthSquared(), Is.EqualTo(25.0f).Within(1e-5f));

        Vector3 norm = a.Normalized();
        Assert.That(norm.Length(), Is.EqualTo(1.0f).Within(1e-5f));
        Assert.That(norm.X, Is.EqualTo(0.6f).Within(1e-5f));
        Assert.That(norm.Z, Is.EqualTo(0.8f).Within(1e-5f));

        Vector3 zeroNorm = Vector3.Zero.Normalized();
        Assert.That(zeroNorm.X, Is.EqualTo(0.0f));
        Assert.That(zeroNorm.Y, Is.EqualTo(0.0f));
        Assert.That(zeroNorm.Z, Is.EqualTo(0.0f));
    }

    [Test]
    public void QuaternionRotationFromEulerRotatesForwardVector()
    {
        const float halfPi = 1.57079632679f;
        Quaternion qYaw90 = Quaternion.FromEuler(0.0f, halfPi, 0.0f);
        Vector3 rotated = qYaw90 * new Vector3(0.0f, 0.0f, 1.0f);
        Assert.That(rotated.X, Is.EqualTo(1.0f).Within(1e-4f));
        Assert.That(rotated.Y, Is.EqualTo(0.0f).Within(1e-4f));
        Assert.That(rotated.Z, Is.EqualTo(0.0f).Within(1e-4f));

        Quaternion slerped = Quaternion.Lerp(Quaternion.Identity, qYaw90, 0.5f);
        Assert.That(slerped.Magnitude, Is.EqualTo(1.0f).Within(1e-4f));
    }

    [Test]
    public void AABBIntersectionHitsAndMissesCleanly()
    {
        var box = new AABB(new Vector3(-1.0f), new Vector3(1.0f));
        Assert.That(box.Center.X, Is.EqualTo(0.0f));
        Assert.That(box.Size.X, Is.EqualTo(2.0f));

        var hitRay = new Ray(new Vector3(0.0f, 0.0f, -5.0f), new Vector3(0.0f, 0.0f, 1.0f));
        bool didHit = box.IntersectRay(hitRay, out float hitDist);
        Assert.That(didHit, Is.True);
        Assert.That(hitDist, Is.EqualTo(4.0f).Within(1e-4f));

        var missRay = new Ray(new Vector3(0.0f, 5.0f, -5.0f), new Vector3(0.0f, 0.0f, 1.0f));
        Assert.That(box.IntersectRay(missRay, out _), Is.False);
    }

    [Test]
    public void NetworkAndComponentEnumsMatchNativeBitmasks()
    {
        Assert.That((ushort)NetInputButtons.None, Is.EqualTo(0));
        Assert.That((ushort)NetInputButtons.Jump, Is.EqualTo(1 << 0));
        Assert.That((ushort)NetInputButtons.Sprint, Is.EqualTo(1 << 1));
        Assert.That((ushort)NetInputButtons.Fire, Is.EqualTo(1 << 2));
        Assert.That((ushort)NetInputButtons.Use, Is.EqualTo(1 << 3));
        Assert.That((ushort)NetInputButtons.Respawn, Is.EqualTo(1 << 4));
        Assert.That((ushort)NetInputButtons.Bounce, Is.EqualTo(1 << 5));

        Assert.That((uint)NetCompFlag.Transform, Is.EqualTo(1u << 0));
        Assert.That((uint)NetCompFlag.CharacterController, Is.EqualTo(1u << 11));
        Assert.That((uint)NetCompFlag.All, Is.EqualTo(0xFFFFFFFFu));

        Assert.That((int)CameraProjectionType.Orthographic, Is.EqualTo(0));
        Assert.That((int)CameraProjectionType.Perspective, Is.EqualTo(1));

        Assert.That((int)ParticleEmitterPreset.Smoke, Is.EqualTo(0));
        Assert.That((int)ParticleEmitterPreset.Fire, Is.EqualTo(1));
        Assert.That((int)ParticleEmitterPreset.Spark, Is.EqualTo(2));
    }
}

