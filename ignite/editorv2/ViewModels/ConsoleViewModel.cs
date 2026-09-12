using System.Collections.ObjectModel;
using System.Linq;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using IgniteEditor.Models;
using IgniteEditor.Services;

namespace IgniteEditor.ViewModels;

public partial class ConsoleViewModel : ViewModelBase
{
    private readonly LoggingService _loggingService;

    [ObservableProperty]
    private ObservableCollection<LogEntryViewModel> _filteredLogs = new();

    [ObservableProperty] private bool _showTrace = true;
    [ObservableProperty] private bool _showDebug = true;
    [ObservableProperty] private bool _showInfo = true;
    [ObservableProperty] private bool _showWarning = true;
    [ObservableProperty] private bool _showError = true;
    [ObservableProperty] private bool _showCritical = true;

    [ObservableProperty]
    private string _commandInput = string.Empty;

    public ConsoleViewModel(LoggingService loggingService)
    {
        _loggingService = loggingService;
        _loggingService.Logs.CollectionChanged += (_, _) => RefreshFilter();
        RefreshFilter();
    }

    [RelayCommand]
    private void Clear()
    {
        _loggingService.Clear();
        FilteredLogs.Clear();
    }

    [RelayCommand]
    private void ExecuteCommand()
    {
        if (string.IsNullOrWhiteSpace(CommandInput)) return;
        _loggingService.Info($"> {CommandInput}");
        // Future: dispatch command to engine
        _loggingService.Trace($"Command '{CommandInput}' executed (no engine connected)");
        CommandInput = string.Empty;
    }

    partial void OnShowTraceChanged(bool value) => RefreshFilter();
    partial void OnShowDebugChanged(bool value) => RefreshFilter();
    partial void OnShowInfoChanged(bool value) => RefreshFilter();
    partial void OnShowWarningChanged(bool value) => RefreshFilter();
    partial void OnShowErrorChanged(bool value) => RefreshFilter();
    partial void OnShowCriticalChanged(bool value) => RefreshFilter();

    private void RefreshFilter()
    {
        FilteredLogs.Clear();
        foreach (var log in _loggingService.Logs)
        {
            if (ShouldShow(log.Level))
            {
                FilteredLogs.Add(new LogEntryViewModel(log));
            }
        }
    }

    private bool ShouldShow(LogLevel level) => level switch
    {
        LogLevel.Trace => ShowTrace,
        LogLevel.Debug => ShowDebug,
        LogLevel.Info => ShowInfo,
        LogLevel.Warning => ShowWarning,
        LogLevel.Error => ShowError,
        LogLevel.Critical => ShowCritical,
        _ => true
    };
}

public partial class LogEntryViewModel : ViewModelBase
{
    public LogEntry Entry { get; }

    public string TimestampText => Entry.Timestamp.ToString("HH:mm:ss.fff");
    public string LevelText => Entry.Level.ToString().ToUpperInvariant();
    public string Message => Entry.Message;
    public LogLevel Level => Entry.Level;

    public LogEntryViewModel(LogEntry entry)
    {
        Entry = entry;
    }
}

