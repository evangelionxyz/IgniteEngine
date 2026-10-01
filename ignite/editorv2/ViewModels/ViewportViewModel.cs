using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;

namespace IgniteEditor.ViewModels;

public partial class ViewportViewModel : ViewModelBase
{
    [ObservableProperty]
    private GizmoOperation _currentGizmoOperation = GizmoOperation.None;

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

    [ObservableProperty]
    private CameraNavigationMode _navigationMode = CameraNavigationMode.Orbit;

    public List<CameraNavigationMode> NavigationModes { get; } =
    [
        CameraNavigationMode.Orbit,
        CameraNavigationMode.Fly,
        CameraNavigationMode.Mode2D
    ];

    public event Action<ulong, bool>? EntityPicked;

    public void NotifyEntityPicked(ulong uuid, bool isMultiSelect)
    {
        EntityPicked?.Invoke(uuid, isMultiSelect);
    }

    public event Action<GizmoOperation>? GizmoOperationChanged;

    public void NotifyGizmoOperationChanged(GizmoOperation op)
    {
        CurrentGizmoOperation = op;
        GizmoOperationChanged?.Invoke(op);
    }
}
