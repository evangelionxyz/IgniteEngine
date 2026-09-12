using System;
using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Chrome;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.VisualTree;
using IgniteEditor.ViewModels;
using Ignite.Managed.Services;

namespace IgniteEditor;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
        Views.Panels.ConsolePanel.RegisterNativeLoggerBridge();
        DataContext = new MainWindowViewModel();

        Loaded += (_, _) => HideBuiltInTitleBarElements();

        AddHandler(PointerPressedEvent, (sender, e) =>
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                if (e.Source is Visual visual && !IsDescendantOfNativeViewport(visual))
                {
                    if (TryGetPlatformHandle()?.Handle is { } handle && handle != IntPtr.Zero)
                    {
                        SetFocus(handle);
                    }
                }
            }
        }, RoutingStrategies.Tunnel);
    }

    protected override void OnOpened(EventArgs e)
    {
        base.OnOpened(e);
        HideBuiltInTitleBarElements();
    }

    private void HideBuiltInTitleBarElements()
    {
        foreach (var visual in this.GetVisualDescendants())
        {
            if (visual is TextBlock tb)
            {
                if (tb.Name == "PART_Title" || tb.Text == "Ignite Editor" || tb.Text == "IgniteEditor")
                {
                    tb.IsVisible = false;
                    tb.Opacity = 0;
                    tb.Width = 0;
                    tb.Height = 0;
                }
            }
            else if (visual is Button btn)
            {
                if (btn.Name == "PART_FullScreenButton" ||
                    WindowDecorationProperties.GetElementRole(btn) == WindowDecorationsElementRole.FullScreenButton)
                {
                    btn.IsVisible = false;
                    btn.IsEnabled = false;
                    btn.Width = 0;
                    btn.Height = 0;
                    btn.Opacity = 0;
                }
            }
        }
    }

    private void OnTitleBarPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (e.ClickCount == 2)
        {
            WindowState = WindowState == WindowState.Maximized ? WindowState.Normal : WindowState.Maximized;
            return;
        }

        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
        {
            BeginMoveDrag(e);
        }
    }

    protected override void OnClosing(WindowClosingEventArgs e)
    {
        base.OnClosing(e);




        try { NativeEngineBridge.Ignite_SetLogCallback(null); } catch { }
    }

    private static bool IsDescendantOfNativeViewport(Visual? visual)
    {
        while (visual != null)
        {
            if (visual is Controls.NativeViewportControl)
                return true;
            visual = visual.GetVisualParent();
        }
        return false;
    }

    [DllImport("user32.dll")]
    private static extern IntPtr SetFocus(IntPtr hWnd);
}
