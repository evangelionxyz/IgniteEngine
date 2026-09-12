using CommunityToolkit.Mvvm.ComponentModel;

namespace IgniteEditor.ViewModels;

public partial class ViewportViewModel : ViewModelBase
{
    [ObservableProperty]
    private GizmoOperation _currentGizmoOperation = GizmoOperation.Translate;

    [ObservableProperty]
    private double _viewportWidth = 800;

    [ObservableProperty]
    private double _viewportHeight = 600;

    [ObservableProperty]
    private string _statusMessage = "No Engine Connected";

    [ObservableProperty]
    private bool _isEngineConnected;

    [ObservableProperty]
    private float _fps;
}

