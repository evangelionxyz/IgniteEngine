// Copyright (c) 2026 Evangelion Manuhutu

using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Platform;
using Avalonia.Threading;

using Ignite.Managed.Services;
using IgniteEditor.ViewModels;

namespace IgniteEditor.Controls;

/// <summary>
/// Native viewport control that hosts the Ignite engine directly via Vulkan swapchain.
/// All 3D rendering and camera navigation (Orbit/Fly/2D via SDL3 InputSystem) runs natively
/// in C++ at 120+ FPS with zero GPU readback overhead.
/// </summary>
public class NativeViewportControl : NativeControlHost
{
    private IntPtr _childHwnd = IntPtr.Zero;
    private DispatcherTimer? _renderTimer;
    private readonly Stopwatch _stopwatch = new();
    private TimeSpan _lastTime;
    private bool _isInitialized;
    private CameraNavigationMode _currentNavigationMode = CameraNavigationMode.Orbit;

    private int _currentWidth;
    private int _currentHeight;

    public event Action<bool>? EngineConnectionChanged;
    public event Action<float>? FpsUpdated;

    public bool IsEngineConnected => _isInitialized;

    // FPS tracking
    private int    _frameCount;
    private double _fpsAccumulator;

    public NativeViewportControl()
    {
        Focusable = true;
    }

    public void SetNavigationMode(CameraNavigationMode mode)
    {
        _currentNavigationMode = mode;
        if (_isInitialized)
        {
            NativeEngineBridge.Ignite_Camera_SetNavigationMode((int)mode);
        }
    }
    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        base.OnPointerPressed(e);
        if (_childHwnd != IntPtr.Zero)
        {
            SetFocus(_childHwnd);
        }
    }
    protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
    {
        int width  = (int)Bounds.Width  > 0 ? (int)Bounds.Width  : 1280;
        int height = (int)Bounds.Height > 0 ? (int)Bounds.Height : 720;
        _currentWidth  = width;
        _currentHeight = height;

        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
        {
            var parentHwnd = parent.Handle;

            const int WS_CHILD        = 0x40000000;
            const int WS_VISIBLE      = 0x10000000;
            const int WS_CLIPCHILDREN = 0x02000000;
            const int WS_CLIPSIBLINGS = 0x04000000;
            const int SS_NOTIFY       = 0x00000100;

            _childHwnd = CreateWindowExW(
                0,
                "STATIC",
                "IgniteViewportHost",
                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SS_NOTIFY,
                0, 0, width, height,
                parentHwnd,
                IntPtr.Zero,
                GetModuleHandleW(null),
                IntPtr.Zero);

            if (_childHwnd != IntPtr.Zero)
            {
                var config = new IgniteAppConfig
                {
                    NativeWindowHandle = _childHwnd,
                    Width       = (uint)width,
                    Height      = (uint)height,
                    Offscreen   = false, // Direct native Vulkan swapchain (120+ FPS!)
                    GraphicsApi = 0,     // Vulkan
                    EnableDebug = false
                };

                try
                {
                    _isInitialized = NativeEngineBridge.Ignite_Init(ref config);
                    EngineConnectionChanged?.Invoke(_isInitialized);

                    if (_isInitialized)
                    {
                        NativeEngineBridge.Ignite_Camera_SetNavigationMode((int)_currentNavigationMode);

                        _stopwatch.Start();
                        _lastTime = _stopwatch.Elapsed;

                        _renderTimer = new DispatcherTimer(DispatcherPriority.Render)
                        {
                            Interval = TimeSpan.FromMicroseconds(0) // Run as fast as display allows
                        };
                        _renderTimer.Tick += OnRenderTick;
                        _renderTimer.Start();
                    }
                }
                catch (Exception ex)
                {
                    Debug.WriteLine($"[NativeViewportControl] Engine init failed: {ex.Message}");
                    _isInitialized = false;
                    EngineConnectionChanged?.Invoke(false);
                }

                return new PlatformHandle(_childHwnd, "HWND");
            }
        }

        return base.CreateNativeControlCore(parent);
    }

    private void OnRenderTick(object? sender, EventArgs e)
    {
        if (!_isInitialized)
            return;

        var now = _stopwatch.Elapsed;
        var dt  = (float)(now - _lastTime).TotalSeconds;
        _lastTime = now;
        if (dt > 0.1f) dt = 0.1f;

        // Native engine step: polls SDL3 events, updates EditorCamera, renders directly to swapchain
        NativeEngineBridge.Ignite_Tick(dt);

        // FPS counter
        _frameCount++;
        _fpsAccumulator += dt;
        if (_fpsAccumulator >= 0.5)
        {
            FpsUpdated?.Invoke((float)(_frameCount / _fpsAccumulator));
            _frameCount = 0;
            _fpsAccumulator = 0;
        }
    }

    protected override void OnSizeChanged(SizeChangedEventArgs e)
    {
        base.OnSizeChanged(e);

        int w = Math.Max(1, (int)e.NewSize.Width);
        int h = Math.Max(1, (int)e.NewSize.Height);

        if (w == _currentWidth && h == _currentHeight)
            return;

        _currentWidth  = w;
        _currentHeight = h;

        if (_childHwnd != IntPtr.Zero)
        {
            SetWindowPos(_childHwnd, IntPtr.Zero, 0, 0, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        }

        if (_isInitialized)
        {
            NativeEngineBridge.Ignite_Resize((uint)w, (uint)h);
        }
    }

    protected override void DestroyNativeControlCore(IPlatformHandle control)
    {
        _renderTimer?.Stop();
        _renderTimer = null;

        if (_isInitialized)
        {
            NativeEngineBridge.Ignite_Shutdown();
            _isInitialized = false;
            EngineConnectionChanged?.Invoke(false);
        }

        if (_childHwnd != IntPtr.Zero)
        {
            DestroyWindow(_childHwnd);
            _childHwnd = IntPtr.Zero;
        }

        base.DestroyNativeControlCore(control);
    }

    private const uint SWP_NOZORDER   = 0x0004;
    private const uint SWP_NOACTIVATE = 0x0010;

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr CreateWindowExW(int dwExStyle, string lpClassName, string lpWindowName,
        int dwStyle, int x, int y, int nWidth, int nHeight, IntPtr hWndParent, IntPtr hMenu,
        IntPtr hInstance, IntPtr lpParam);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DestroyWindow(IntPtr hWnd);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter,
        int X, int Y, int cx, int cy, uint uFlags);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr GetModuleHandleW(string? lpModuleName);

    [DllImport("user32.dll")]
    private static extern IntPtr SetFocus(IntPtr hWnd);
}
