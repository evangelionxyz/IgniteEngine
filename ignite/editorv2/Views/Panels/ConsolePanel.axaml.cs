using System;
using System.Collections.Specialized;
using Avalonia.Controls;
using Avalonia.Threading;

using Ignite.Managed.Models;
using Ignite.Managed.Services;

using IgniteEditor.Services;
using IgniteEditor.ViewModels;

namespace IgniteEditor.Views.Panels;

public partial class ConsolePanel : UserControl
{
    private static NativeEngineBridge.LogCallback? s_NativeLogCallback;
    private static bool s_IsBridgeRegistered;

    public ConsolePanel()
    {
        InitializeComponent();

        RegisterNativeLoggerBridge();

        DataContextChanged += OnDataContextChanged;
    }

    public static void RegisterNativeLoggerBridge()
    {
        if (s_IsBridgeRegistered)
            return;

        try
        {
            // Keep strong reference to prevent GC collection of native delegate
            s_NativeLogCallback = (level, message) =>
            {
                var logLevel = level switch
                {
                    0 => LogLevel.Trace,
                    1 => LogLevel.Debug,
                    2 => LogLevel.Info,
                    3 => LogLevel.Warning,
                    4 => LogLevel.Error,
                    5 => LogLevel.Critical,
                    _ => LogLevel.Info
                };

                Avalonia.Threading.Dispatcher.UIThread.Post(() =>
                {
                    LoggingService.Instance.Log(logLevel, message);
                });
            };

            NativeEngineBridge.Ignite_SetLogCallback(s_NativeLogCallback);
            s_IsBridgeRegistered = true;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"[ConsolePanel] Could not register native logger callback: {ex.Message}");
        }
    }

    private void OnDataContextChanged(object? sender, EventArgs e)
    {
        if (DataContext is ConsoleViewModel vm)
        {
            vm.FilteredLogs.CollectionChanged += OnFilteredLogsCollectionChanged;
        }
    }

    private void OnFilteredLogsCollectionChanged(object? sender, NotifyCollectionChangedEventArgs e)
    {
        if (e.Action == NotifyCollectionChangedAction.Add && DataContext is ConsoleViewModel vm && vm.FilteredLogs.Count > 0)
        {
            Dispatcher.UIThread.Post(() =>
            {
                var lastItem = vm.FilteredLogs[^1];
                LogListBox?.ScrollIntoView(lastItem);
            }, DispatcherPriority.Background);
        }
    }
}
