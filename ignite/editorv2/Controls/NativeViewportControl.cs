using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using Avalonia.Controls;
using Avalonia.Platform;
using Avalonia.Threading;
using IgniteEditor.Services;

namespace IgniteEditor.Controls;

public class NativeViewportControl : NativeControlHost
{
    private IntPtr _childHwnd = IntPtr.Zero;
    private DispatcherTimer? _renderTimer;
    private readonly DispatcherTimer _resizeDebounceTimer;
    private readonly Stopwatch _resizeThrottleStopwatch = new();
    private readonly Stopwatch _stopwatch = new();
    private TimeSpan _lastTime;
    private bool _isInitialized;
    private bool _firstResizeApplied;

    private int _pendingWidth;
    private int _pendingHeight;
    private int _currentWidth;
    private int _currentHeight;

    public event Action<bool>? EngineConnectionChanged;
    public event Action<float>? FpsUpdated;

    public bool IsEngineConnected => _isInitialized;

    public NativeViewportControl()
    {
        _resizeDebounceTimer = new DispatcherTimer(DispatcherPriority.Render)
        {
            Interval = TimeSpan.FromMilliseconds(50)
        };
        _resizeDebounceTimer.Tick += OnResizeDebounceTick;
    }

    protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
    {
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
        {
            var parentHwnd = parent.Handle;
            int width = (int)Bounds.Width > 0 ? (int)Bounds.Width : 1280;
            int height = (int)Bounds.Height > 0 ? (int)Bounds.Height : 720;
            _currentWidth = width;
            _currentHeight = height;
            _pendingWidth = width;
            _pendingHeight = height;

            // Win32 Window Styles
            const int WS_CHILD = 0x40000000;
            const int WS_VISIBLE = 0x10000000;
            const int WS_CLIPCHILDREN = 0x02000000;
            const int WS_CLIPSIBLINGS = 0x04000000;

            _childHwnd = CreateWindowExW(
                0,
                "STATIC",
                "IgniteViewportHost",
                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
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
                    Width = (uint)width,
                    Height = (uint)height,
                    Offscreen = false, // Direct native viewport swapchain
                    GraphicsApi = 0,   // Vulkan first
                    EnableDebug = false
                };

                try
                {
                    _isInitialized = NativeEngineBridge.Ignite_Init(ref config);
                    EngineConnectionChanged?.Invoke(_isInitialized);

                    if (_isInitialized)
                    {
                        _stopwatch.Start();
                        _lastTime = _stopwatch.Elapsed;

                        _renderTimer = new DispatcherTimer(DispatcherPriority.Render)
                        {
                            Interval = TimeSpan.FromMilliseconds(16) // ~60 FPS
                        };
                        _renderTimer.Tick += OnRenderTick;
                        _renderTimer.Start();
                    }
                }
                catch (Exception ex)
                {
                    Debug.WriteLine($"[NativeViewportControl] Failed to initialize engine: {ex.Message}");
                    _isInitialized = false;
                    EngineConnectionChanged?.Invoke(false);
                }

                return new PlatformHandle(_childHwnd, "HWND");
            }
        }

        return base.CreateNativeControlCore(parent);
    }

    private int _frameCount;
    private double _fpsAccumulator;

    private void OnRenderTick(object? sender, EventArgs e)
    {
        if (!_isInitialized) return;

        var currentTime = _stopwatch.Elapsed;
        var dt = (float)(currentTime - _lastTime).TotalSeconds;
        _lastTime = currentTime;

        if (dt > 0.1f) dt = 0.1f;

        NativeEngineBridge.Ignite_Tick(dt);

        _frameCount++;
        _fpsAccumulator += dt;
        if (_fpsAccumulator >= 0.5)
        {
            float fps = (float)(_frameCount / _fpsAccumulator);
            FpsUpdated?.Invoke(fps);
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

        _pendingWidth = w;
        _pendingHeight = h;

        // Apply immediately on first layout or before engine is initialized
        if (!_firstResizeApplied || !_isInitialized)
        {
            _firstResizeApplied = true;
            ApplyPendingResize();
            return;
        }

        // Throttle during continuous dragging: at most once every 100ms
        if (!_resizeThrottleStopwatch.IsRunning || _resizeThrottleStopwatch.ElapsedMilliseconds >= 100)
        {
            _resizeThrottleStopwatch.Restart();
            ApplyPendingResize();
            return;
        }

        // Debounce: wait 50ms after the user stops dragging
        _resizeDebounceTimer.Stop();
        _resizeDebounceTimer.Start();
    }

    private void OnResizeDebounceTick(object? sender, EventArgs e)
    {
        _resizeDebounceTimer.Stop();
        _resizeThrottleStopwatch.Reset();
        ApplyPendingResize();
    }

    private void ApplyPendingResize()
    {
        if (_childHwnd == IntPtr.Zero || _pendingWidth <= 0 || _pendingHeight <= 0)
            return;

        if (_pendingWidth == _currentWidth && _pendingHeight == _currentHeight)
            return;

        _currentWidth = _pendingWidth;
        _currentHeight = _pendingHeight;

        SetWindowPos(_childHwnd, IntPtr.Zero, 0, 0, _currentWidth, _currentHeight,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);

        if (_isInitialized)
        {
            NativeEngineBridge.Ignite_Resize((uint)_currentWidth, (uint)_currentHeight);
        }
    }

    protected override void DestroyNativeControlCore(IPlatformHandle control)
    {
        _resizeDebounceTimer.Stop();
        _resizeThrottleStopwatch.Reset();

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

    private const uint SWP_NOZORDER = 0x0004;
    private const uint SWP_NOACTIVATE = 0x0010;
    private const uint SWP_NOCOPYBITS = 0x0100;

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr CreateWindowExW(int dwExStyle, string lpClassName, string lpWindowName,
        int dwStyle, int x, int y, int nWidth, int nHeight, IntPtr hWndParent, IntPtr hMenu, IntPtr hInstance, IntPtr lpParam);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DestroyWindow(IntPtr hWnd);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr GetModuleHandleW(string? lpModuleName);
}
