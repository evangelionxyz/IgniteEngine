using System;
using System.Runtime.InteropServices;

namespace IgniteEditor.Services;

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
}
