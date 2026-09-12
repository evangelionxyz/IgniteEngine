using System;
using System.Collections.ObjectModel;
using System.Numerics;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using IgniteEditor.Models;

namespace IgniteEditor.ViewModels;

public partial class PropertiesViewModel : ViewModelBase
{
    [ObservableProperty]
    private string _entityName = string.Empty;

    [ObservableProperty]
    private bool _isEntityActive = true;

    [ObservableProperty]
    private bool _hasSelection;

    [ObservableProperty]
    private ObservableCollection<ComponentEditorViewModel> _components = new();

    public void SetEntity(EntityNodeViewModel? entityNode, Models.EntityModel? model)
    {
        if (entityNode == null || model == null)
        {
            HasSelection = false;
            EntityName = string.Empty;
            Components.Clear();
            return;
        }

        HasSelection = true;
        EntityName = entityNode.Name;
        IsEntityActive = entityNode.IsActive;
        Components.Clear();

        foreach (var comp in model.Components)
        {
            Components.Add(ComponentEditorViewModel.Create(comp));
        }
    }
}

public partial class ComponentEditorViewModel : ViewModelBase
{
    [ObservableProperty]
    private string _componentName = string.Empty;

    [ObservableProperty]
    private ComponentType _componentType;

    [ObservableProperty]
    private bool _isEnabled = true;

    [ObservableProperty]
    private bool _isExpanded = true;

    public static ComponentEditorViewModel Create(ComponentModel model)
    {
        return model.Type switch
        {
            ComponentType.Transform => new TransformComponentVM(model),
            ComponentType.Camera => new CameraComponentVM(model),
            ComponentType.DirectionalLight or ComponentType.PointLight or ComponentType.SpotLight
                => new LightComponentVM(model),
            _ => new GenericComponentVM(model)
        };
    }

    protected ComponentEditorViewModel(ComponentModel model)
    {
        ComponentName = model.DisplayName;
        ComponentType = model.Type;
        IsEnabled = model.IsEnabled;
    }

    protected ComponentEditorViewModel() { }
}

public partial class TransformComponentVM : ComponentEditorViewModel
{
    [ObservableProperty] private float _positionX;
    [ObservableProperty] private float _positionY;
    [ObservableProperty] private float _positionZ;

    [ObservableProperty] private float _rotationX;
    [ObservableProperty] private float _rotationY;
    [ObservableProperty] private float _rotationZ;

    [ObservableProperty] private float _scaleX = 1f;
    [ObservableProperty] private float _scaleY = 1f;
    [ObservableProperty] private float _scaleZ = 1f;

    public TransformComponentVM(ComponentModel model) : base(model) { }
    public TransformComponentVM() { }
}

public partial class CameraComponentVM : ComponentEditorViewModel
{
    [ObservableProperty] private bool _isPerspective = true;
    [ObservableProperty] private float _fieldOfView = 60f;
    [ObservableProperty] private float _nearClip = 0.1f;
    [ObservableProperty] private float _farClip = 1000f;
    [ObservableProperty] private float _orthographicSize = 10f;

    public CameraComponentVM(ComponentModel model) : base(model) { }
    public CameraComponentVM() { }
}

public partial class LightComponentVM : ComponentEditorViewModel
{
    [ObservableProperty] private float _colorR = 1f;
    [ObservableProperty] private float _colorG = 1f;
    [ObservableProperty] private float _colorB = 1f;
    [ObservableProperty] private float _intensity = 1f;
    [ObservableProperty] private float _range = 10f;
    [ObservableProperty] private bool _castShadows = true;

    public LightComponentVM(ComponentModel model) : base(model) { }
    public LightComponentVM() { }
}

public partial class GenericComponentVM : ComponentEditorViewModel
{
    public GenericComponentVM(ComponentModel model) : base(model) { }
    public GenericComponentVM() { }
}

