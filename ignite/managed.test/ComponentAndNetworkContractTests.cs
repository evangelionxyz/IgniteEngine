// Copyright (c) 2026 Evangelion Manuhutu

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using Ignite.Core;
using Ignite.Core.Component;
using NUnit.Framework;

namespace Ignite.Managed.Test;

[TestFixture]
public class ComponentAndNetworkContractTests
{
    private static readonly Type[] s_ExpectedComponents =
    {
        typeof(TransformComponent),
        typeof(RenderingComponent),
        typeof(CameraComponent),
        typeof(DirectionalLightComponent),
        typeof(PointLightComponent),
        typeof(SpotLightComponent),
        typeof(PointLight2DComponent),
        typeof(StaticMeshComponent),
        typeof(WorldEnvironmentComponent),
        typeof(ParticleSystemComponent),
        typeof(NetworkIdentityComponent),
        typeof(PlaneColliderComponent),
        typeof(TextComponent),
        typeof(AnimatorComponent),
        typeof(AudioSourceComponent),
        typeof(WidgetComponent),
        typeof(Sprite2DComponent),
        typeof(Circle2DComponent),
        typeof(RigidbodyComponent),
        typeof(Rigidbody2DComponent),
        typeof(BoxColliderComponent),
        typeof(SphereColliderComponent),
        typeof(CapsuleColliderComponent),
        typeof(CharacterControllerComponent),
        typeof(BoxCollider2DComponent),
        typeof(CircleCollider2DComponent)
    };

    private sealed class SampleNetworkBehaviour : Entity
    {
        public bool JoinedCalled;
        public uint JoinedClientId;
        public string JoinedClientName = string.Empty;
        public bool InputCalled;
        public ushort InputButtons;

        public override void OnNetworkClientJoined(uint clientId, string clientName)
        {
            JoinedCalled = true;
            JoinedClientId = clientId;
            JoinedClientName = clientName;
        }

        public override void OnNetworkInput(uint clientId, float moveX, float moveY, ushort buttons, float yaw, float deltaTime)
        {
            InputCalled = true;
            InputButtons = buttons;
        }
    }

    [Test]
    public void AllExpectedComponentsAreConstructibleIComponents()
    {
        foreach (Type compType in s_ExpectedComponents)
        {
            Assert.That(typeof(IComponent).IsAssignableFrom(compType), Is.True,
                $"{compType.Name} must inherit IComponent");
            Assert.That(compType.IsAbstract, Is.False,
                $"{compType.Name} must not be abstract");

            object? instance = Activator.CreateInstance(compType);
            Assert.That(instance, Is.Not.Null,
                $"{compType.Name} must have a parameterless constructor");
        }
    }

    [Test]
    public void NewlyExposedComponentsExposeExpectedMembers()
    {
        Assert.That(typeof(RenderingComponent).GetProperty(nameof(RenderingComponent.Visible)), Is.Not.Null);
        Assert.That(typeof(CameraComponent).GetProperty(nameof(CameraComponent.Primary)), Is.Not.Null);
        Assert.That(typeof(CameraComponent).GetProperty(nameof(CameraComponent.Fov)), Is.Not.Null);
        Assert.That(typeof(CameraComponent).GetProperty(nameof(CameraComponent.ProjectionType)), Is.Not.Null);
        Assert.That(typeof(DirectionalLightComponent).GetProperty(nameof(DirectionalLightComponent.ShadowDistance)), Is.Not.Null);
        Assert.That(typeof(PointLightComponent).GetProperty(nameof(PointLightComponent.Range)), Is.Not.Null);
        Assert.That(typeof(SpotLightComponent).GetProperty(nameof(SpotLightComponent.InnerConeAngle)), Is.Not.Null);
        Assert.That(typeof(PointLight2DComponent).GetProperty(nameof(PointLight2DComponent.Radius)), Is.Not.Null);
        Assert.That(typeof(StaticMeshComponent).GetProperty(nameof(StaticMeshComponent.MeshHandle)), Is.Not.Null);
        Assert.That(typeof(WorldEnvironmentComponent).GetProperty(nameof(WorldEnvironmentComponent.FogDensity)), Is.Not.Null);
        Assert.That(typeof(WorldEnvironmentComponent).GetMethod(nameof(WorldEnvironmentComponent.MarkDirty)), Is.Not.Null);
        Assert.That(typeof(ParticleSystemComponent).GetMethod(nameof(ParticleSystemComponent.Play)), Is.Not.Null);
        Assert.That(typeof(ParticleSystemComponent).GetMethod(nameof(ParticleSystemComponent.Restart)), Is.Not.Null);
        Assert.That(typeof(NetworkIdentityComponent).GetProperty(nameof(NetworkIdentityComponent.NetId)), Is.Not.Null);
        Assert.That(typeof(NetworkIdentityComponent).GetProperty(nameof(NetworkIdentityComponent.Prefab)), Is.Not.Null);
        Assert.That(typeof(NetworkIdentityComponent).GetMethod(nameof(NetworkIdentityComponent.MarkDirty)), Is.Not.Null);
        Assert.That(typeof(PlaneColliderComponent).GetProperty(nameof(PlaneColliderComponent.Scale)), Is.Not.Null);
        Assert.That(typeof(TextComponent).GetProperty(nameof(TextComponent.ScreenSpace)), Is.Not.Null);
        Assert.That(typeof(TextComponent).GetProperty(nameof(TextComponent.FontSize)), Is.Not.Null);
    }

    [Test]
    public void EntityExposesNetworkLifecycleCallbacksAndRemoveComponent()
    {
        Assert.That(typeof(Entity).GetMethod(nameof(Entity.RemoveComponent)), Is.Not.Null);

        string[] nativeCallbacks =
        {
            "Internal_OnNetworkClientJoinedNative",
            "Internal_OnNetworkClientLeftNative",
            "Internal_OnNetworkConnectedNative",
            "Internal_OnNetworkDisconnectedNative",
            "Internal_OnNetworkInputNative"
        };

        foreach (string methodName in nativeCallbacks)
        {
            MethodInfo? m = typeof(Entity).GetMethod(methodName, BindingFlags.Instance | BindingFlags.NonPublic);
            Assert.That(m, Is.Not.Null, $"Missing native callback bridge Entity.{methodName}");
        }

        // Simulate native UTF-8 string marshaling into SampleNetworkBehaviour
        var script = new SampleNetworkBehaviour();
        IntPtr utf8Ptr = NativeObject.StringToUtf8("ManagedTestClient");
        try
        {
            MethodInfo joinedNative = typeof(Entity).GetMethod("Internal_OnNetworkClientJoinedNative", BindingFlags.Instance | BindingFlags.NonPublic)!;
            joinedNative.Invoke(script, new object[] { 7u, (ulong)utf8Ptr.ToInt64() });
        }
        finally
        {
            Marshal.FreeCoTaskMem(utf8Ptr);
        }

        Assert.That(script.JoinedCalled, Is.True);
        Assert.That(script.JoinedClientId, Is.EqualTo(7u));
        Assert.That(script.JoinedClientName, Is.EqualTo("ManagedTestClient"));

        MethodInfo inputNative = typeof(Entity).GetMethod("Internal_OnNetworkInputNative", BindingFlags.Instance | BindingFlags.NonPublic)!;
        inputNative.Invoke(script, new object[] { 7u, 1.0f, -0.5f, (ushort)NetInputButtons.Jump, 90.0f, 0.016f });
        Assert.That(script.InputCalled, Is.True);
        Assert.That(script.InputButtons, Is.EqualTo((ushort)NetInputButtons.Jump));
    }

    [Test]
    public void NetStaticApiHasAllServerAndClientMethods()
    {
        string[] expectedMembers =
        {
            nameof(Net.IsConnected),
            nameof(Net.IsServerRunning),
            nameof(Net.IsConnecting),
            nameof(Net.GetClientId),
            nameof(Net.StartServer),
            nameof(Net.StopServer),
            nameof(Net.Connect),
            nameof(Net.Disconnect),
            nameof(Net.SpawnEntity),
            nameof(Net.DespawnEntity),
            nameof(Net.SendInput),
            nameof(Net.SendChat),
            nameof(Net.KickClient),
            nameof(Net.GetStats),
            nameof(Net.GetServerTick),
            nameof(Net.GetClientServerTick),
            nameof(Net.GetClientCount),
            nameof(Net.GetConnectedClients),
            nameof(Net.GetChatHistory),
            nameof(Net.SetSimulationSettings)
        };

        var members = new HashSet<string>(typeof(Net).GetMembers(BindingFlags.Public | BindingFlags.Static).Select(m => m.Name));
        foreach (string expected in expectedMembers)
        {
            Assert.That(members.Contains(expected), Is.True, $"Missing Net.{expected}");
        }

        // Calling Net API without an active native HostFXR bridge should throw InvalidOperationException rather than AV
        Assert.Throws<InvalidOperationException>(() => _ = Net.IsServerRunning);
    }

    [Test]
    public void NativeApiStructsMatchExpectedSequentialLayout()
    {
        // Verify CoreNativeAPI.API and ComponentNativeAPI.API have only IntPtr sequential fields
        FieldInfo[] coreFields = typeof(CoreNativeAPI.API).GetFields(BindingFlags.Public | BindingFlags.Instance);
        Assert.That(coreFields.Length, Is.EqualTo(45));
        Assert.That(coreFields.All(f => f.FieldType == typeof(IntPtr)), Is.True);

        int coreExpectedBytes = coreFields.Length * IntPtr.Size;
        Assert.That(Marshal.SizeOf<CoreNativeAPI.API>(), Is.EqualTo(coreExpectedBytes));

        FieldInfo[] compFields = typeof(ComponentNativeAPI.API).GetFields(BindingFlags.Public | BindingFlags.Instance);
        Assert.That(compFields.Length, Is.GreaterThanOrEqualTo(240));
        Assert.That(compFields.All(f => f.FieldType == typeof(IntPtr)), Is.True);

        int compExpectedBytes = compFields.Length * IntPtr.Size;
        Assert.That(Marshal.SizeOf<ComponentNativeAPI.API>(), Is.EqualTo(compExpectedBytes));
    }
}

