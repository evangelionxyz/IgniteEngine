using System;
using System.Runtime.InteropServices;

namespace Ignite.Managed.Services;

[StructLayout(LayoutKind.Sequential)]
public struct IgniteAppConfig
{
    public IntPtr NativeWindowHandle;
    public uint Width;
    public uint Height;
    [MarshalAs(UnmanagedType.I1)]
    public bool Offscreen;
    public int GraphicsApi; // 0 = Vulkan, 1 = D3D12
    [MarshalAs(UnmanagedType.I1)]
    public bool EnableDebug;
}

public static class NativeEngineBridge
{
    private const string DllName = "Ignite.Engine.dll";

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Init(ref IgniteAppConfig config);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Ignite_Tick(float deltaTime);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Ignite_Resize(uint width, uint height);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Ignite_Shutdown();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Ignite_GetNativeWindow();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Ignite_GetSharedTextureHandle();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_ReadbackViewportPixels(IntPtr outBuffer, uint bufferSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void LogCallback(int level, [MarshalAs(UnmanagedType.LPUTF8Str)] string message);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Ignite_SetLogCallback(LogCallback? callback);

    // Project API
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Project_New(string name, string parentDirectory);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Project_Open(string filepath);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Project_Save();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Ignite_Project_Close();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Project_IsOpen();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Ignite_Project_GetName();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Ignite_Project_GetDirectory();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Ignite_Project_GetFilePath();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Ignite_Project_GetAssetDirectory();

    // Asset Manager API
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern uint Ignite_AssetManager_GetAssetCount();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Ignite_AssetManager_Refresh();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Ignite_AssetManager_SyncFromRust();

    // Editor Camera API
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Ignite_Camera_SetNavigationMode(int mode);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int Ignite_Camera_GetNavigationMode();

    // Scene API
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern IntPtr Ignite_Scene_DeserializeHierarchyJson(string filepath);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Ignite_Scene_GetActiveHierarchyJson();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Scene_Open(string filepath);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Scene_Save();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Scene_New();

    public static string GetActiveSceneHierarchyJson()
    {
        var ptr = Ignite_Scene_GetActiveHierarchyJson();
        return ptr == IntPtr.Zero ? "[]" : Marshal.PtrToStringAnsi(ptr) ?? "[]";
    }

    public static string DeserializeSceneHierarchyJson(string filepath)
    {
        var ptr = Ignite_Scene_DeserializeHierarchyJson(filepath);
        return ptr == IntPtr.Zero ? "[]" : Marshal.PtrToStringAnsi(ptr) ?? "[]";
    }

    // Entity & Component API
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern ulong Ignite_Entity_Create(string name, ulong parentUuid);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_Delete(ulong uuid);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_Rename(ulong uuid, string newName);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_Reparent(ulong entityUuid, ulong newParentUuid);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern ulong Ignite_Entity_Duplicate(ulong uuid);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetActive(ulong uuid, [MarshalAs(UnmanagedType.I1)] bool active);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetTransform(ulong uuid, float px, float py, float pz, float rx, float ry, float rz, float sx, float sy, float sz);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetSpriteColor(ulong uuid, float r, float g, float b, float a);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_AddComponent(ulong uuid, int componentType);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_RemoveComponent(ulong uuid, int componentType);

    // ======================================
    // Scene Lifecycle P/Invokes
    // ======================================
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Ignite_Scene_Play();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Ignite_Scene_Stop();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Ignite_Scene_Simulate();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Ignite_Scene_Pause();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Ignite_Scene_StepFrame(int frames);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int Ignite_Scene_GetState();

    // ======================================
    // Component Setter P/Invokes
    // ======================================
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetCamera(ulong uuid, [MarshalAs(UnmanagedType.I1)] bool isPerspective, float fov, float nearPlane, float farPlane, float orthoSize);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetDirectionalLight(ulong uuid, float r, float g, float b, float a, float intensity, float shadowDistance, [MarshalAs(UnmanagedType.I1)] bool castShadows);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetPointLight(ulong uuid, float r, float g, float b, float a, float intensity, float range, [MarshalAs(UnmanagedType.I1)] bool enabled);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetSpotLight(ulong uuid, float r, float g, float b, float a, float intensity, float range, float innerCone, float outerCone, [MarshalAs(UnmanagedType.I1)] bool enabled);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetPointLight2D(ulong uuid, float r, float g, float b, float a, float radius, float intensity, [MarshalAs(UnmanagedType.I1)] bool enabled);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetSprite2D(ulong uuid, float r, float g, float b, float a, float tilingX, float tilingY, [MarshalAs(UnmanagedType.I1)] bool flipX, [MarshalAs(UnmanagedType.I1)] bool flipY);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetCircle2D(ulong uuid, float r, float g, float b, float a, float thickness, float fade);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetRigidbody(ulong uuid, int bodyType, float mass, float linearDamping, float angularDamping, float friction, float restitution, [MarshalAs(UnmanagedType.I1)] bool useGravity);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetRigidbody2D(ulong uuid, int bodyType, float gravityScale, float linearDamping, float angularDamping, [MarshalAs(UnmanagedType.I1)] bool fixedRotation, [MarshalAs(UnmanagedType.I1)] bool isAwake, [MarshalAs(UnmanagedType.I1)] bool isEnabled);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetBoxCollider(ulong uuid, float cx, float cy, float cz, float sx, float sy, float sz);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetSphereCollider(ulong uuid, float cx, float cy, float cz, float radius);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetCapsuleCollider(ulong uuid, float cx, float cy, float cz, float radius, float height);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetBoxCollider2D(ulong uuid, float ox, float oy, float sx, float sy, float density, float friction, float restitution, [MarshalAs(UnmanagedType.I1)] bool isSensor);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetCircleCollider2D(ulong uuid, float cx, float cy, float radius, float density, float friction, float restitution, [MarshalAs(UnmanagedType.I1)] bool isSensor);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetCharacterController(ulong uuid, float radius, float height, float stepHeight, float slopeAngle, float mass, float friction);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetAudioSource(ulong uuid, float volume, float pitch, float pan, [MarshalAs(UnmanagedType.I1)] bool playOnStart, [MarshalAs(UnmanagedType.I1)] bool loop);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetText(ulong uuid, string text, float r, float g, float b, float a, float kerning, float lineSpacing, [MarshalAs(UnmanagedType.I1)] bool screenSpace);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool Ignite_Entity_SetWorldEnvironment(ulong uuid, float exposure, float gamma, float ambient, float fogDensity, float fr, float fg, float fb, float fa, float fogStart, float fogEnd);
}
