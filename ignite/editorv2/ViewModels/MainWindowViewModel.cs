using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Ignite.Managed.Services;
using IgniteEditor.Services;
using System.Threading.Tasks;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Platform.Storage;
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

    public bool IsPlaying => PlayState == PlayModeState.Playing;
    public bool IsSimulating => PlayState == PlayModeState.Simulating;
    public bool IsPaused => PlayState == PlayModeState.Paused;
    public bool CanPause => PlayState != PlayModeState.Stopped;
    public bool CanStep => PlayState == PlayModeState.Paused;

    public string PlayButtonText => IsPlaying ? "⏹ Stop" : "▶ Play";
    public string SimulateButtonText => IsSimulating ? "⏹ Stop" : "⚡ Simulate";
    public string PauseButtonText => IsPaused ? "▶ Resume" : "⏸ Pause";

    partial void OnPlayStateChanged(PlayModeState value)
    {
        OnPropertyChanged(nameof(IsPlaying));
        OnPropertyChanged(nameof(IsSimulating));
        OnPropertyChanged(nameof(IsPaused));
        OnPropertyChanged(nameof(CanPause));
        OnPropertyChanged(nameof(CanStep));
        OnPropertyChanged(nameof(PlayButtonText));
        OnPropertyChanged(nameof(SimulateButtonText));
        OnPropertyChanged(nameof(PauseButtonText));
        PauseCommand.NotifyCanExecuteChanged();
        StepCommand.NotifyCanExecuteChanged();
    }

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
        _sceneService = new EngineSceneService();
        _loggingService = LoggingService.Instance;
        _loggingService.PopulateWithSampleLogs();

        SceneHierarchy = new SceneHierarchyViewModel(_sceneService);
        Properties = new PropertiesViewModel(_sceneService);
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
        if (PlayState == PlayModeState.Playing)
        {
            _sceneService.StopScene();
            PlayState = PlayModeState.Stopped;
            StatusText = "Scene Stopped";
            _loggingService.Info("Scene stopped");
        }
        else
        {
            if (PlayState == PlayModeState.Simulating || PlayState == PlayModeState.Paused)
            {
                _sceneService.StopScene();
            }

            _sceneService.PlayScene();
            PlayState = PlayModeState.Playing;
            StatusText = "Playing...";
            _loggingService.Info("Scene play started");
        }
    }

    [RelayCommand]
    private void Simulate()
    {
        if (PlayState == PlayModeState.Simulating)
        {
            _sceneService.StopScene();
            PlayState = PlayModeState.Stopped;
            StatusText = "Simulation Stopped";
            _loggingService.Info("Simulation stopped");
        }
        else
        {
            if (PlayState == PlayModeState.Playing || PlayState == PlayModeState.Paused)
            {
                _sceneService.StopScene();
            }

            _sceneService.SimulateScene();
            PlayState = PlayModeState.Simulating;
            StatusText = "Simulating physics...";
            _loggingService.Info("Physics simulation started");
        }
    }

    [RelayCommand(CanExecute = nameof(CanPause))]
    private void Pause()
    {
        if (PlayState == PlayModeState.Stopped) return;

        _sceneService.PauseScene();
        if (PlayState == PlayModeState.Paused)
        {
            int nativeState = _sceneService.GetSceneState();
            PlayState = nativeState == 2 ? PlayModeState.Simulating : PlayModeState.Playing;
            StatusText = PlayState == PlayModeState.Playing ? "Playing..." : "Simulating...";
            _loggingService.Info("Scene resumed");
        }
        else
        {
            PlayState = PlayModeState.Paused;
            StatusText = "Paused";
            _loggingService.Info("Scene paused");
        }
    }

    [RelayCommand]
    private void Stop()
    {
        if (PlayState != PlayModeState.Stopped)
        {
            _sceneService.StopScene();
            PlayState = PlayModeState.Stopped;
            StatusText = "Ready";
            _loggingService.Info("Scene stopped");
        }
    }

    [RelayCommand(CanExecute = nameof(CanStep))]
    private void Step()
    {
        if (PlayState == PlayModeState.Paused)
        {
            _sceneService.StepScene(1);
            _loggingService.Trace("Step simulation by 1 frame");
            StatusText = "Stepped 1 frame";
        }
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
        if (_sceneService.NewScene())
        {
            _loggingService.Info("New scene created");
            StatusText = "New scene created";

            _sceneService.LoadActiveScene();
            SceneHierarchy.RefreshHierarchy();
        }
    }

    [RelayCommand]
    private async Task OpenScene()
    {
        if (Avalonia.Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop && desktop.MainWindow != null)
        {
            var options = new FilePickerOpenOptions
            {
                Title = "Open Ignite Scene",
                AllowMultiple = false,
                FileTypeFilter = new[]
                {
                    new FilePickerFileType("Ignite Scene (*.ixscene)")
                    {
                        Patterns = new[] { "*.ixscene" }
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
                _loggingService.Info($"Opening scene: {path}");
                bool success = _sceneService.LoadScene(path);
                if (success)
                {
                    SceneHierarchy.RefreshHierarchy();
                    StatusText = $"Loaded scene: {System.IO.Path.GetFileName(path)}";
                    _loggingService.Info($"Scene loaded: {path}");
                }
                else
                {
                    StatusText = "Failed to load scene";
                    _loggingService.Error($"Failed to load scene: {path}");
                }
            }
        }
    }

    [RelayCommand]
    private void SaveScene()
    {
        bool ok = _sceneService.SaveActiveScene();
        if (ok)
        {
            _loggingService.Info("Scene saved successfully.");
            StatusText = "Scene saved";
        }
        else
        {
            _loggingService.Error("Failed to save scene.");
            StatusText = "Failed to save scene";
        }
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
                    _sceneService.LoadActiveScene();
                    SceneHierarchy.RefreshHierarchy();
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
                    _sceneService.LoadActiveScene();
                    SceneHierarchy.RefreshHierarchy();
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
