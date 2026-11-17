// Copyright (c) 2026 Evangelion Manuhutu

using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using Ignite.Managed.Models;
using Ignite.Managed.Services;
using NUnit.Framework;

namespace Ignite.Managed.Test;

[TestFixture]
public class LiveEngineManagedIntegrationTests
{
    private const string EngineDll = "Ignite.Engine.dll";

    [DllImport(EngineDll, CallingConvention = CallingConvention.Cdecl)]
    private static extern void Ignite_Scene_Play();

    [DllImport(EngineDll, CallingConvention = CallingConvention.Cdecl)]
    private static extern void Ignite_Scene_Stop();

    [DllImport(EngineDll, CallingConvention = CallingConvention.Cdecl)]
    private static extern int Ignite_Scene_GetState();

    private string _originalCwd = string.Empty;
    private string _tempProjectRoot = string.Empty;
    private bool _engineInitialized;

    [OneTimeSetUp]
    public void SetUpEngine()
    {
        _originalCwd = Directory.GetCurrentDirectory();
        string baseDir = AppContext.BaseDirectory;
        if (!File.Exists(Path.Combine(baseDir, EngineDll)) ||
            !Directory.Exists(Path.Combine(baseDir, "resources", "shaders")))
        {
            Assert.Ignore("Ignite.Engine.dll or synced resources/shaders not found next to Ignite.Managed.Test.");
        }

        Directory.SetCurrentDirectory(baseDir);

        var config = new IgniteAppConfig
        {
            NativeWindowHandle = IntPtr.Zero,
            Width = 128,
            Height = 128,
            Offscreen = true,
            GraphicsApi = 0, // Vulkan
            EnableDebug = false
        };

        _engineInitialized = NativeEngineBridge.Ignite_Init(ref config);
        if (!_engineInitialized)
        {
            Assert.Ignore("Native Ignite_Init offscreen could not initialize graphics device in this environment.");
        }

        _tempProjectRoot = Path.Combine(Path.GetTempPath(), "IgniteManagedTest_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(_tempProjectRoot);
    }

    [OneTimeTearDown]
    public void TearDownEngine()
    {
        if (_engineInitialized)
        {
            try
            {
                if (NativeEngineBridge.Ignite_Project_IsOpen())
                    NativeEngineBridge.Ignite_Project_Close();
                NativeEngineBridge.Ignite_Shutdown();
            }
            catch { }
            _engineInitialized = false;
        }

        if (!string.IsNullOrEmpty(_originalCwd) && Directory.Exists(_originalCwd))
        {
            try { Directory.SetCurrentDirectory(_originalCwd); } catch { }
        }

        if (!string.IsNullOrEmpty(_tempProjectRoot) && Directory.Exists(_tempProjectRoot))
        {
            try { Directory.Delete(_tempProjectRoot, recursive: true); } catch { }
        }
    }

    [Test]
    public void NativeEngineCreatesEntitiesComponentsAndPlayCycleViaBridge()
    {
        const string projectName = "ManagedBridgeSuite";
        Assert.That(NativeEngineBridge.Ignite_Project_New(projectName, _tempProjectRoot), Is.True);
        Assert.That(NativeEngineBridge.Ignite_Project_IsOpen(), Is.True);

        string projDir = Path.Combine(_tempProjectRoot, projectName);
        Assert.That(File.Exists(Path.Combine(projDir, ".luarc.json")), Is.True,
            "SyncLuaTooling must generate .luarc.json on project creation");
        Assert.That(File.Exists(Path.Combine(projDir, ".ignite", "lua", "ignite.d.lua")), Is.True,
            "SyncLuaTooling must copy ignite.d.lua into .ignite/lua/");
        Assert.That(File.Exists(Path.Combine(projDir, ".vscode", "settings.json")), Is.True);
        Assert.That(File.Exists(Path.Combine(projDir, ".zed", "settings.json")), Is.True);

        Assert.That(NativeEngineBridge.Ignite_Scene_New(), Is.True);

        ulong camId = NativeEngineBridge.Ignite_Entity_Create("ManagedTestCamera", 0);
        Assert.That(camId, Is.Not.EqualTo(0ul));
        const int CompTypeCamera = 5;
        const int CompTypeWorldEnvironment = 25;
        const int CompTypePointLight = 27;
        Assert.That(NativeEngineBridge.Ignite_Entity_AddComponent(camId, CompTypeCamera), Is.True);
        Assert.That(NativeEngineBridge.Ignite_Entity_SetCamera(camId, true, 70.0f, 0.2f, 800.0f, 12.0f), Is.True);

        ulong lightId = NativeEngineBridge.Ignite_Entity_Create("ManagedTestPointLight", camId);
        Assert.That(lightId, Is.Not.EqualTo(0ul));
        Assert.That(NativeEngineBridge.Ignite_Entity_AddComponent(lightId, CompTypePointLight), Is.True);
        Assert.That(NativeEngineBridge.Ignite_Entity_SetPointLight(lightId, 1.0f, 0.6f, 0.2f, 1.0f, 3.5f, 18.0f, true), Is.True);

        ulong envId = NativeEngineBridge.Ignite_Entity_Create("ManagedWorldEnv", 0);
        Assert.That(envId, Is.Not.EqualTo(0ul));
        Assert.That(NativeEngineBridge.Ignite_Entity_AddComponent(envId, CompTypeWorldEnvironment), Is.True);
        Assert.That(NativeEngineBridge.Ignite_Entity_SetWorldEnvironment(envId, 1.25f, 2.2f, 0.45f, 0.02f, 0.4f, 0.5f, 0.6f, 1.0f, 5.0f, 90.0f), Is.True);

        // Verify native JSON hierarchy round-trips through managed deserializer
        string hierarchyJson = NativeEngineBridge.GetActiveSceneHierarchyJson();
        List<EntityModel> parsed = SceneHierarchyDeserializer.Deserialize(hierarchyJson);
        Assert.That(parsed.Count, Is.GreaterThanOrEqualTo(2));

        // Step offscreen Play -> Tick -> Stop
        Ignite_Scene_Play();
        NativeEngineBridge.Ignite_Tick(1.0f / 60.0f);
        NativeEngineBridge.Ignite_Tick(1.0f / 60.0f);
        Ignite_Scene_Stop();
        NativeEngineBridge.Ignite_Tick(1.0f / 60.0f);

        Assert.That(NativeEngineBridge.Ignite_Project_Save(), Is.True);
    }
}
