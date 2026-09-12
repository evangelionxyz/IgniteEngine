using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Ignite.Managed.Services;
using IgniteEditor.Services;
using System.Threading.Tasks;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Platform.Storage;
using System;
using System.Runtime.InteropServices;

namespace IgniteEditor.ViewModels;

public partial class MainWindowViewModel : ViewModelBase
{
    [ObservableProperty]
    private string _title = "Ignite Editor";

    [ObservableProperty]
    private string _statusText = "Ready";

    [ObservableProperty]
    private float _loadingProgress;

    [ObservableProperty]
    private PlayModeState _playState = PlayModeState.Stopped;

    [ObservableProperty]
    private GizmoOperation _activeGizmo = GizmoOperation.Translate;

    [ObservableProperty]
    private bool _snapEnabled = true;

    [ObservableProperty]
    private float _snapValue = 0.25f;

    [ObservableProperty]
    private bool _isWorldSpace = true;

    public string CoordinateSpaceText => IsWorldSpace ? "World" : "Local";

    partial void OnIsWorldSpaceChanged(bool value)
    {
        OnPropertyChanged(nameof(CoordinateSpaceText));
    }

    [ObservableProperty]
    private bool _is2DMode = false;

    [ObservableProperty]
    private bool _showGrid = true;

    [ObservableProperty]
    private float _cameraSpeed = 1.0f;

    [ObservableProperty]
    private string _activeConfiguration = "Debug";

    [ObservableProperty]
    private string _currentSceneName = "MainScene.ixscene";

    // Panel ViewModels
    public SceneHierarchyViewModel SceneHierarchy { get; }
    public PropertiesViewModel Properties { get; }
    public ContentBrowserViewModel ContentBrowser { get; }
    public ConsoleViewModel Console { get; }
    public ViewportViewModel Viewport { get; }

    private readonly ISceneService _sceneService;
    private readonly LoggingService _loggingService;

    public MainWindowViewModel()
    {
        _sceneService = new MockSceneService();
        _loggingService = LoggingService.Instance;
        _loggingService.PopulateWithSampleLogs();

        SceneHierarchy = new SceneHierarchyViewModel(_sceneService);
        Properties = new PropertiesViewModel();
        ContentBrowser = new ContentBrowserViewModel();
        Console = new ConsoleViewModel(_loggingService);
        Viewport = new ViewportViewModel();

        // Wire selection → properties
        SceneHierarchy.SelectionChanged += (_, node) =>
        {
            if (node != null)
            {
                var model = _sceneService.GetEntity(node.EntityId);
                Properties.SetEntity(node, model);
            }
            else
            {
                Properties.SetEntity(null, null);
            }
        };

        // Initialize content browser with editor resources folder if it exists
        var resourcesPath = System.IO.Path.Combine(
            System.IO.Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location) ?? ".",
            "resources");

        if (!System.IO.Directory.Exists(resourcesPath))
        {
            // Fallback: use the ignite editor resources directory
            resourcesPath = @"d:\Dev\Ignite\ignite\editor\resources";
        }

        if (System.IO.Directory.Exists(resourcesPath))
        {
            ContentBrowser.Initialize(resourcesPath);
        }
    }

    // ---- Play Controls ----

    [RelayCommand]
    private void Play()
    {
        PlayState = PlayModeState.Playing;
        StatusText = "Playing...";
        _loggingService.Info("Scene play started");
    }

    [RelayCommand]
    private void Pause()
    {
        PlayState = PlayState == PlayModeState.Paused ? PlayModeState.Playing : PlayModeState.Paused;
        StatusText = PlayState == PlayModeState.Paused ? "Paused" : "Playing...";
    }

    [RelayCommand]
    private void Stop()
    {
        PlayState = PlayModeState.Stopped;
        StatusText = "Ready";
        _loggingService.Info("Scene play stopped");
    }

    [RelayCommand]
    private void Step()
    {
        _loggingService.Trace("Step simulation by 1 frame");
        StatusText = "Stepped 1 frame";
    }

    [RelayCommand]
    private void Simulate()
    {
        PlayState = PlayModeState.Simulating;
        StatusText = "Simulating physics...";
        _loggingService.Info("Physics simulation started");
    }

    [RelayCommand]
    private void ToggleCoordinateSpace()
    {
        IsWorldSpace = !IsWorldSpace;
        _loggingService.Trace($"Coordinate space changed to: {(IsWorldSpace ? "World" : "Local")}");
    }

    [RelayCommand]
    private void Toggle2DMode()
    {
        Is2DMode = !Is2DMode;
        _loggingService.Trace($"Viewport mode set to: {(Is2DMode ? "2D" : "3D")}");
    }

    [RelayCommand]
    private void SetConfiguration(string config)
    {
        ActiveConfiguration = config;
        _loggingService.Info($"Active configuration set to: {config}");
        StatusText = $"Configuration: {config}";
    }

    [RelayCommand]
    private void BuildSolution()
    {
        _loggingService.Info("Building game solution (Configuration: " + ActiveConfiguration + ")...");
        StatusText = "Building solution...";
    }

    // ---- Gizmo ----

    [RelayCommand]
    private void SetGizmo(string mode)
    {
        ActiveGizmo = mode switch
        {
            "Translate" => GizmoOperation.Translate,
            "Rotate" => GizmoOperation.Rotate,
            "Scale" => GizmoOperation.Scale,
            _ => GizmoOperation.None
        };
    }

    // ---- File Menu ----

    [RelayCommand]
    private void NewScene()
    {
        _loggingService.Info("New scene created");
        StatusText = "New scene created";
    }

    [RelayCommand]
    private void OpenScene()
    {
        _loggingService.Info("Open scene dialog (not yet connected)");
    }

    [RelayCommand]
    private void SaveScene()
    {
        _loggingService.Info("Scene saved (not yet connected)");
        StatusText = "Scene saved";
    }

    [RelayCommand]
    private async Task NewProject()
    {
        if (Avalonia.Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop && desktop.MainWindow != null)
        {
            var options = new FolderPickerOpenOptions
            {
                Title = "Select Location for New Project"
            };

            var folders = await desktop.MainWindow.StorageProvider.OpenFolderPickerAsync(options);
            if (folders != null && folders.Count > 0)
            {
                string parentDir = folders[0].Path.LocalPath;
                string defaultName = "NewProject";
                _loggingService.Info($"Creating new project '{defaultName}' in: {parentDir}");
                bool success = NativeEngineBridge.Ignite_Project_New(defaultName, parentDir);
                if (success)
                {
                    var name = Marshal.PtrToStringUTF8(NativeEngineBridge.Ignite_Project_GetName()) ?? "";
                    Title = $"Ignite Editor - {name}";
                    StatusText = $"Created project: {name}";
                    _loggingService.Info($"Project '{name}' created successfully.");
                }
                else
                {
                    StatusText = "Failed to create project";
                    _loggingService.Error($"Failed to create project in: {parentDir}");
                }
            }
        }
    }

    [RelayCommand]
    private async Task OpenProject()
    {
        if (Avalonia.Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop && desktop.MainWindow != null)
        {
            var options = new FilePickerOpenOptions
            {
                Title = "Open Ignite Project",
                AllowMultiple = false,
                FileTypeFilter = new[]
                {
                    new FilePickerFileType("Ignite Project (*.ixproj)")
                    {
                        Patterns = new[] { "*.ixproj" }
                    },
                    new FilePickerFileType("All Files (*.*)")
                    {
                        Patterns = new[] { "*.*" }
                    }
                }
            };

            var files = await desktop.MainWindow.StorageProvider.OpenFilePickerAsync(options);
            if (files != null && files.Count > 0)
            {
                string path = files[0].Path.LocalPath;
                _loggingService.Info($"Opening project: {path}");
                bool success = NativeEngineBridge.Ignite_Project_Open(path);
                if (success)
                {
                    var name = Marshal.PtrToStringUTF8(NativeEngineBridge.Ignite_Project_GetName()) ?? "";
                    Title = $"Ignite Editor - {name}";
                    StatusText = $"Opened project: {name}";
                    _loggingService.Info($"Project '{name}' opened successfully.");
                }
                else
                {
                    StatusText = "Failed to open project";
                    _loggingService.Error($"Failed to open project: {path}");
                }
            }
        }
    }

    [RelayCommand]
    private void SaveProject()
    {
        if (NativeEngineBridge.Ignite_Project_IsOpen())
        {
            bool ok = NativeEngineBridge.Ignite_Project_Save();
            if (ok)
            {
                _loggingService.Info("Project saved.");
                StatusText = "Project saved";
            }
            else
            {
                _loggingService.Error("Failed to save project.");
                StatusText = "Failed to save project";
            }
        }
        else
        {
            _loggingService.Warn("No active project to save.");
            StatusText = "No active project";
        }
    }
}

