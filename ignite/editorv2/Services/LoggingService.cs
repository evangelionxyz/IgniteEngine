using System;
using System.Collections.ObjectModel;
using IgniteEditor.Models;

namespace IgniteEditor.Services;

public class LoggingService
{
    private static readonly Lazy<LoggingService> _instance = new(() => new LoggingService());
    public static LoggingService Instance => _instance.Value;

    private const int MaxLogEntries = 10000;
    public ObservableCollection<LogEntry> Logs { get; } = new();

    public void Log(LogLevel level, string message)
    {
        var entry = new LogEntry(DateTime.Now, level, message);

        try
        {
            if (Avalonia.Threading.Dispatcher.UIThread.CheckAccess())
            {
                Logs.Add(entry);
                while (Logs.Count > MaxLogEntries)
                    Logs.RemoveAt(0);
            }
            else
            {
                Avalonia.Threading.Dispatcher.UIThread.Post(() =>
                {
                    Logs.Add(entry);
                    while (Logs.Count > MaxLogEntries)
                        Logs.RemoveAt(0);
                });
            }
        }
        catch
        {
            Logs.Add(entry);
        }
    }

    public void Clear()
    {
        if (Avalonia.Threading.Dispatcher.UIThread.CheckAccess())
        {
            Logs.Clear();
        }
        else
        {
            Avalonia.Threading.Dispatcher.UIThread.Post(() => Logs.Clear());
        }
    }

    public void Trace(string message) => Log(LogLevel.Trace, message);
    public void Debug(string message) => Log(LogLevel.Debug, message);
    public void Info(string message) => Log(LogLevel.Info, message);
    public void Warn(string message) => Log(LogLevel.Warning, message);
    public void Error(string message) => Log(LogLevel.Error, message);
    public void Critical(string message) => Log(LogLevel.Critical, message);

    public void PopulateWithSampleLogs()
    {
        Info("Ignite Editor initialized");
        Info("Avalonia UI framework loaded");
        Debug("Scene service created with mock data");
        Trace("Registering panel layouts...");
        Info("Project: SampleProject loaded");
        Warn("Texture 'ground_diffuse.png' not found, using fallback");
        Info("Scene 'MainScene' loaded successfully (7 entities)");
        Debug("Asset registry: 42 assets indexed");
        Trace("Render pipeline initialized");
        Info("Editor ready");
    }
}
