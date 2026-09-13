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
    public event Action<ulong, bool>? EntityPicked;

    private static NativeEngineBridge.EntitySelectedCallback? s_EntitySelectedCallback;
    public static NativeViewportControl? Instance { get; private set; }
    private static long s_LastPickTimestamp;
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

    private static void OnNativeEntitySelected(ulong uuid, bool isMultiSelect)
    {
        s_LastPickTimestamp = Stopwatch.GetTimestamp();
        Dispatcher.UIThread.Post(() =>
        {
            Instance?.EntityPicked?.Invoke(uuid, isMultiSelect);
        });
    }

    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        base.OnPointerPressed(e);
        if (_childHwnd != IntPtr.Zero)
        {
            SetFocus(_childHwnd);
        }

        var pt = e.GetCurrentPoint(this);
        if (pt.Properties.IsLeftButtonPressed && _isInitialized)
        {
            var now = Stopwatch.GetTimestamp();
            if (now - s_LastPickTimestamp < Stopwatch.Frequency / 10)
                return;

            s_LastPickTimestamp = now;

            var isDoubleClick = e.ClickCount >= 2;
            var isShiftDown = (e.KeyModifiers & KeyModifiers.Shift) != 0;
            var isCtrlDown = (e.KeyModifiers & KeyModifiers.Control) != 0;
            var isMultiSelect = isShiftDown || isCtrlDown;
            var w = (float)(Bounds.Width > 0 ? Bounds.Width : _currentWidth);
            var h = (float)(Bounds.Height > 0 ? Bounds.Height : _currentHeight);
            var picked = NativeEngineBridge.Ignite_Viewport_PickEntity((float)pt.Position.X, (float)pt.Position.Y,
                (uint)w, (uint)h, isDoubleClick, isMultiSelect);

            if (!isMultiSelect)
            {
                var isAlreadySelected = NativeEngineBridge.Ignite_Viewport_IsEntitySelected(picked);
                var currentCount = NativeEngineBridge.Ignite_Viewport_GetSelectedEntityCount();

                ulong finalSelection = picked;
                if (picked != 0 && isAlreadySelected && currentCount == 1 && !isDoubleClick)
                {
                    finalSelection = 0;
                    NativeEngineBridge.Ignite_Viewport_ClearSelectedEntities();
                }
                else if (picked == 0)
                {
                    finalSelection = 0;
                    NativeEngineBridge.Ignite_Viewport_ClearSelectedEntities();
                }
                else
                {
                    NativeEngineBridge.Ignite_Viewport_SetSelectedEntity(finalSelection);
                }

                EntityPicked?.Invoke(finalSelection, false);
            }
            else
            {
                if (picked != 0)
                {
                    if (NativeEngineBridge.Ignite_Viewport_IsEntitySelected(picked))
                    {
                        NativeEngineBridge.Ignite_Viewport_DeselectEntity(picked);
                    }
                    else
                    {
                        NativeEngineBridge.Ignite_Viewport_SelectEntity(picked, true);
                    }
                    EntityPicked?.Invoke(picked, true);
                }
            }
        }
    }
    protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
    {
        var width  = (int)Bounds.Width  > 0 ? (int)Bounds.Width  : 1280;
        var height = (int)Bounds.Height > 0 ? (int)Bounds.Height : 720;
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
                        Instance = this;
                        s_EntitySelectedCallback = OnNativeEntitySelected;
                        NativeEngineBridge.Ignite_Viewport_SetEntitySelectedCallback(s_EntitySelectedCallback);

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
                    Shutdown();
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

        if (dt > 0.1f)
            dt = 0.1f;

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

        var w = Math.Max(1, (int)e.NewSize.Width);
        var h = Math.Max(1, (int)e.NewSize.Height);

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
            if (Instance == this)
            {
                NativeEngineBridge.Ignite_Viewport_SetEntitySelectedCallback(null);
                Instance = null;
                s_EntitySelectedCallback = null;
            }

            NativeEngineBridge.Ignite_Shutdown();
            Shutdown();
        }

        if (_childHwnd != IntPtr.Zero)
        {
            DestroyWindow(_childHwnd);
            _childHwnd = IntPtr.Zero;
        }

        base.DestroyNativeControlCore(control);
    }

    public void Shutdown()
    {
        _isInitialized = false;
        EngineConnectionChanged?.Invoke(false);
    }

    private const uint SWP_NOZORDER   = 0x0004;
    private const uint SWP_NOACTIVATE = 0x0010;

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr CreateWindowExW(
        int dwExStyle, string lpClassName, string lpWindowName,
        int dwStyle, int x, int y, int nWidth, int nHeight,
        IntPtr hWndParent, IntPtr hMenu, IntPtr hInstance, IntPtr lpParam);

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
