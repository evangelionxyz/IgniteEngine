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
}
