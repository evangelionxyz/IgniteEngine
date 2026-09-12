using Avalonia.Media;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HarfBuzzSharp;
using Ignite.Managed.Models;
using Ignite.Managed.Services;
using System;
using System.Collections.ObjectModel;
using System.Numerics;

namespace IgniteEditor.ViewModels;

public partial class PropertiesViewModel : ViewModelBase
{
    private readonly ISceneService? _sceneService;
    private EntityNodeViewModel? _currentEntityNode;
    private EntityModel? _currentModel;
    private bool _isSettingEntity;

    [ObservableProperty]
    private string _entityName = string.Empty;

    [ObservableProperty]
    private bool _isEntityActive = true;

    [ObservableProperty]
    private bool _hasSelection;

    [ObservableProperty]
    private ObservableCollection<ComponentEditorViewModel> _components = new();

    public PropertiesViewModel(ISceneService? sceneService = null)
    {
        _sceneService = sceneService;
    }

    public void SetEntity(EntityNodeViewModel? entityNode, EntityModel? model)
    {
        _isSettingEntity = true;
        _currentEntityNode = entityNode;
        _currentModel = model;

        if (entityNode == null || model == null)
        {
            HasSelection = false;
            EntityName = string.Empty;
            Components.Clear();
            _isSettingEntity = false;
            return;
        }

        HasSelection = true;
        EntityName = entityNode.Name;
        IsEntityActive = entityNode.IsActive;
        Components.Clear();

        Guid entityId = entityNode.EntityId;

        foreach (var comp in model.Components)
        {
            var vm = ComponentEditorViewModel.Create(comp, entityId, _sceneService);
            Components.Add(vm);
        }

        _isSettingEntity = false;
    }

    partial void OnEntityNameChanged(string value)
    {
        if (_isSettingEntity || _currentEntityNode == null || string.IsNullOrWhiteSpace(value)) return;
        _currentEntityNode.Name = value;
        _sceneService?.RenameEntity(_currentEntityNode.EntityId, value);
    }

    partial void OnIsEntityActiveChanged(bool value)
    {
        if (_isSettingEntity || _currentEntityNode == null) return;
        _currentEntityNode.IsActive = value;
        _sceneService?.SetEntityActive(_currentEntityNode.EntityId, value);
    }

    [RelayCommand]
    public void AddComponent(ComponentType type)
    {
        if (_currentEntityNode == null || _sceneService == null) return;
        _sceneService.AddComponent(_currentEntityNode.EntityId, type);
        var updated = _sceneService.GetEntity(_currentEntityNode.EntityId);
        if (updated != null)
        {
            SetEntity(_currentEntityNode, updated);
        }
    }

    [RelayCommand]
    public void RemoveComponent(ComponentEditorViewModel? vm)
    {
        if (_currentEntityNode == null || _sceneService == null || vm == null) return;
        if (vm.ComponentType == ComponentType.Transform) return;

        _sceneService.RemoveComponent(_currentEntityNode.EntityId, vm.ComponentType);
        Components.Remove(vm);
    }
}

public abstract partial class ComponentEditorViewModel : ViewModelBase
{
    [ObservableProperty]
    private string _componentName = string.Empty;

    [ObservableProperty]
    private ComponentType _componentType;

    [ObservableProperty]
    private bool _isEnabled = true;

    [ObservableProperty]
    private bool _isExpanded = true;

    public bool CanRemove => ComponentType != ComponentType.Transform;

    protected readonly Guid _entityId;
    protected readonly ISceneService? _sceneService;

    public static ComponentEditorViewModel Create(
        ComponentModel model,
        Guid entityId,
        ISceneService? sceneService)
    {
        return model.Type switch
        {
            ComponentType.Transform => new TransformComponentVM(model, entityId, sceneService),
            ComponentType.Camera => new CameraComponentVM(model, entityId, sceneService),
            ComponentType.Sprite2D => new Sprite2DComponentVM(model, entityId, sceneService),
            ComponentType.Circle2D => new Circle2DComponentVM(model, entityId, sceneService),
            ComponentType.DirectionalLight => new DirectionalLightComponentVM(model, entityId, sceneService),
            ComponentType.PointLight => new PointLightComponentVM(model, entityId, sceneService),
            ComponentType.SpotLight => new SpotLightComponentVM(model, entityId, sceneService),
            ComponentType.PointLight2D => new PointLight2DComponentVM(model, entityId, sceneService),
            ComponentType.Rigidbody => new RigidbodyComponentVM(model, entityId, sceneService),
            ComponentType.Rigidbody2D => new Rigidbody2DComponentVM(model, entityId, sceneService),
            ComponentType.BoxCollider => new BoxColliderComponentVM(model, entityId, sceneService),
            ComponentType.SphereCollider => new SphereColliderComponentVM(model, entityId, sceneService),
            ComponentType.CapsuleCollider => new CapsuleColliderComponentVM(model, entityId, sceneService),
            ComponentType.BoxCollider2D => new BoxCollider2DComponentVM(model, entityId, sceneService),
            ComponentType.CircleCollider2D => new CircleCollider2DComponentVM(model, entityId, sceneService),
            ComponentType.CharacterController => new CharacterControllerComponentVM(model, entityId, sceneService),
            ComponentType.AudioSource => new AudioSourceComponentVM(model, entityId, sceneService),
            ComponentType.Text => new TextComponentVM(model, entityId, sceneService),
            ComponentType.WorldEnvironment => new WorldEnvironmentComponentVM(model, entityId, sceneService),
            ComponentType.Script => new ScriptComponentVM(model, entityId, sceneService),
            _ => new GenericComponentVM(model, entityId, sceneService)
        };
    }

    protected ComponentEditorViewModel(ComponentModel model, Guid entityId, ISceneService? sceneService)
    {
        ComponentName = model.DisplayName;
        ComponentType = model.Type;
        IsEnabled = model.IsEnabled;
        _entityId = entityId;
        _sceneService = sceneService;
    }

    protected ComponentEditorViewModel() { }

    public static Color Vector4ToColor(Vector4 v)
    {
        byte r = (byte)(Math.Clamp(v.X, 0f, 1f) * 255f);
        byte g = (byte)(Math.Clamp(v.Y, 0f, 1f) * 255f);
        byte b = (byte)(Math.Clamp(v.Z, 0f, 1f) * 255f);
        byte a = (byte)(Math.Clamp(v.W, 0f, 1f) * 255f);
        return Color.FromArgb(a, r, g, b);
    }

    public static Vector4 ColorToVector4(Color c)
    {
        return new Vector4(c.R / 255f, c.G / 255f, c.B / 255f, c.A / 255f);
    }
}

// -------------------------------------------------------------
// 1. Transform
// -------------------------------------------------------------
public partial class TransformComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private float _positionX;
    [ObservableProperty] private float _positionY;
    [ObservableProperty] private float _positionZ;

    [ObservableProperty] private float _rotationX;
    [ObservableProperty] private float _rotationY;
    [ObservableProperty] private float _rotationZ;

    [ObservableProperty] private float _scaleX = 1f;
    [ObservableProperty] private float _scaleY = 1f;
    [ObservableProperty] private float _scaleZ = 1f;

    public TransformComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is TransformData data)
        {
            PositionX = data.Position.X;
            PositionY = data.Position.Y;
            PositionZ = data.Position.Z;

            RotationX = data.Rotation.X;
            RotationY = data.Rotation.Y;
            RotationZ = data.Rotation.Z;

            ScaleX = data.Scale.X;
            ScaleY = data.Scale.Y;
            ScaleZ = data.Scale.Z;
        }
        _isUpdating = false;
    }

    public TransformComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityTransform(
            _entityId,
            new Vector3(PositionX, PositionY, PositionZ),
            new Vector3(RotationX, RotationY, RotationZ),
            new Vector3(ScaleX, ScaleY, ScaleZ)
        );
    }

    partial void OnPositionXChanged(float value) => NotifyChange();
    partial void OnPositionYChanged(float value) => NotifyChange();
    partial void OnPositionZChanged(float value) => NotifyChange();
    partial void OnRotationXChanged(float value) => NotifyChange();
    partial void OnRotationYChanged(float value) => NotifyChange();
    partial void OnRotationZChanged(float value) => NotifyChange();
    partial void OnScaleXChanged(float value) => NotifyChange();
    partial void OnScaleYChanged(float value) => NotifyChange();
    partial void OnScaleZChanged(float value) => NotifyChange();
}

// -------------------------------------------------------------
// 2. Camera
// -------------------------------------------------------------
public partial class CameraComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private bool _isPerspective = true;
    [ObservableProperty] private float _fieldOfView = 60f;
    [ObservableProperty] private float _nearClip = 0.1f;
    [ObservableProperty] private float _farClip = 1000f;
    [ObservableProperty] private float _orthographicSize = 10f;

    public CameraComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is CameraData data)
        {
            IsPerspective = data.IsPerspective;
            FieldOfView = data.FieldOfView;
            NearClip = data.NearClip;
            FarClip = data.FarClip;
            OrthographicSize = data.OrthographicSize;
        }
        _isUpdating = false;
    }

    public CameraComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityCamera(_entityId, IsPerspective, FieldOfView, NearClip, FarClip, OrthographicSize);
    }

    partial void OnIsPerspectiveChanged(bool value) => NotifyChange();
    partial void OnFieldOfViewChanged(float value) => NotifyChange();
    partial void OnNearClipChanged(float value) => NotifyChange();
    partial void OnFarClipChanged(float value) => NotifyChange();
    partial void OnOrthographicSizeChanged(float value) => NotifyChange();
}

// -------------------------------------------------------------
// 3. Sprite 2D
// -------------------------------------------------------------
public partial class Sprite2DComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private Color _colorValue = Colors.White;
    [ObservableProperty] private float _tilingX = 1f;
    [ObservableProperty] private float _tilingY = 1f;
    [ObservableProperty] private bool _flipX = false;
    [ObservableProperty] private bool _flipY = false;

    public Sprite2DComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is SpriteData data)
        {
            ColorValue = Vector4ToColor(data.Color);
            TilingX = data.Tiling.X;
            TilingY = data.Tiling.Y;
            FlipX = data.FlipX;
            FlipY = data.FlipY;
        }
        _isUpdating = false;
    }

    public Sprite2DComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntitySprite2D(_entityId, ColorToVector4(ColorValue), new Vector2(TilingX, TilingY), FlipX, FlipY);
    }

    partial void OnColorValueChanged(Color value) => NotifyChange();
    partial void OnTilingXChanged(float value) => NotifyChange();
    partial void OnTilingYChanged(float value) => NotifyChange();
    partial void OnFlipXChanged(bool value) => NotifyChange();
    partial void OnFlipYChanged(bool value) => NotifyChange();
}

// -------------------------------------------------------------
// 4. Circle 2D
// -------------------------------------------------------------
public partial class Circle2DComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private Color _colorValue = Colors.White;
    [ObservableProperty] private float _thickness = 1f;
    [ObservableProperty] private float _fade = 0.005f;

    public Circle2DComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is Circle2DData cd)
        {
            ColorValue = Vector4ToColor(cd.Color);
            Thickness = cd.Thickness;
            Fade = cd.Fade;
        }
        else if (model.Data is SpriteData sd)
        {
            ColorValue = Vector4ToColor(sd.Color);
        }
        _isUpdating = false;
    }

    public Circle2DComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityCircle2D(_entityId, ColorToVector4(ColorValue), Thickness, Fade);
    }

    partial void OnColorValueChanged(Color value) => NotifyChange();
    partial void OnThicknessChanged(float value) => NotifyChange();
    partial void OnFadeChanged(float value) => NotifyChange();
}

// -------------------------------------------------------------
// 5. Directional Light
// -------------------------------------------------------------
public partial class DirectionalLightComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private Color _colorValue = Colors.White;
    [ObservableProperty] private float _intensity = 1f;
    [ObservableProperty] private float _shadowDistance = 200f;
    [ObservableProperty] private bool _castShadows = true;

    public DirectionalLightComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is DirectionalLightData dld)
        {
            ColorValue = Vector4ToColor(dld.Color);
            Intensity = dld.Intensity;
            ShadowDistance = dld.ShadowDistance;
            CastShadows = dld.CastShadows;
        }
        else if (model.Data is LightData ld)
        {
            ColorValue = Vector4ToColor(new Vector4(ld.Color, 1.0f));
            Intensity = ld.Intensity;
            CastShadows = ld.CastShadows;
        }
        _isUpdating = false;
    }

    public DirectionalLightComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityDirectionalLight(_entityId, ColorToVector4(ColorValue), Intensity, ShadowDistance, CastShadows);
    }

    partial void OnColorValueChanged(Color value) => NotifyChange();
    partial void OnIntensityChanged(float value) => NotifyChange();
    partial void OnShadowDistanceChanged(float value) => NotifyChange();
    partial void OnCastShadowsChanged(bool value) => NotifyChange();
}

// -------------------------------------------------------------
// 6. Point Light
// -------------------------------------------------------------
public partial class PointLightComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private Color _colorValue = Colors.White;
    [ObservableProperty] private float _intensity = 1f;
    [ObservableProperty] private float _range = 10f;
    [ObservableProperty] private bool _lightEnabled = true;

    public PointLightComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is PointLightData pld)
        {
            ColorValue = Vector4ToColor(pld.Color);
            Intensity = pld.Intensity;
            Range = pld.Range;
            LightEnabled = pld.Enabled;
        }
        else if (model.Data is LightData ld)
        {
            ColorValue = Vector4ToColor(new Vector4(ld.Color, 1.0f));
            Intensity = ld.Intensity;
            Range = ld.Range;
        }
        _isUpdating = false;
    }

    public PointLightComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityPointLight(_entityId, ColorToVector4(ColorValue), Intensity, Range, LightEnabled);
    }

    partial void OnColorValueChanged(Color value) => NotifyChange();
    partial void OnIntensityChanged(float value) => NotifyChange();
    partial void OnRangeChanged(float value) => NotifyChange();
    partial void OnLightEnabledChanged(bool value) => NotifyChange();
}

// -------------------------------------------------------------
// 7. Spot Light
// -------------------------------------------------------------
public partial class SpotLightComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private Color _colorValue = Colors.White;
    [ObservableProperty] private float _intensity = 1f;
    [ObservableProperty] private float _range = 10f;
    [ObservableProperty] private float _innerCone = 12.5f;
    [ObservableProperty] private float _outerCone = 45f;
    [ObservableProperty] private bool _lightEnabled = true;

    public SpotLightComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is SpotLightData sld)
        {
            ColorValue = Vector4ToColor(sld.Color);
            Intensity = sld.Intensity;
            Range = sld.Range;
            InnerCone = sld.InnerCone;
            OuterCone = sld.OuterCone;
            LightEnabled = sld.Enabled;
        }
        _isUpdating = false;
    }

    public SpotLightComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntitySpotLight(_entityId, ColorToVector4(ColorValue), Intensity, Range, InnerCone, OuterCone, LightEnabled);
    }

    partial void OnColorValueChanged(Color value) => NotifyChange();
    partial void OnIntensityChanged(float value) => NotifyChange();
    partial void OnRangeChanged(float value) => NotifyChange();
    partial void OnInnerConeChanged(float value) => NotifyChange();
    partial void OnOuterConeChanged(float value) => NotifyChange();
    partial void OnLightEnabledChanged(bool value) => NotifyChange();
}

// -------------------------------------------------------------
// 8. Point Light 2D
// -------------------------------------------------------------
public partial class PointLight2DComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private Color _colorValue = Colors.White;
    [ObservableProperty] private float _radius = 5f;
    [ObservableProperty] private float _intensity = 1f;
    [ObservableProperty] private bool _lightEnabled = true;

    public PointLight2DComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is PointLight2DData pld)
        {
            ColorValue = Vector4ToColor(pld.Color);
            Radius = pld.Radius;
            Intensity = pld.Intensity;
            LightEnabled = pld.Enabled;
        }
        _isUpdating = false;
    }

    public PointLight2DComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityPointLight2D(_entityId, ColorToVector4(ColorValue), Radius, Intensity, LightEnabled);
    }

    partial void OnColorValueChanged(Color value) => NotifyChange();
    partial void OnRadiusChanged(float value) => NotifyChange();
    partial void OnIntensityChanged(float value) => NotifyChange();
    partial void OnLightEnabledChanged(bool value) => NotifyChange();
}

// -------------------------------------------------------------
// 9. Rigidbody (3D)
// -------------------------------------------------------------
public partial class RigidbodyComponentVM : ComponentEditorViewModel
{
    public static string[] BodyTypeOptions { get; } = ["Static", "Dynamic", "Kinematic"];
    private bool _isUpdating;

    [ObservableProperty] private int _bodyType = 0;
    [ObservableProperty] private float _mass = 1f;
    [ObservableProperty] private float _linearDamping = 0f;
    [ObservableProperty] private float _angularDamping = 0.05f;
    [ObservableProperty] private float _friction = 0.2f;
    [ObservableProperty] private float _restitution = 0f;
    [ObservableProperty] private bool _useGravity = true;

    public RigidbodyComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is RigidbodyData rd)
        {
            BodyType = rd.BodyType;
            Mass = rd.Mass;
            LinearDamping = rd.LinearDamping;
            AngularDamping = rd.AngularDamping;
            Friction = rd.Friction;
            Restitution = rd.Restitution;
            UseGravity = rd.UseGravity;
        }
        _isUpdating = false;
    }

    public RigidbodyComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityRigidbody(_entityId, BodyType, Mass, LinearDamping, AngularDamping, Friction, Restitution, UseGravity);
    }

    partial void OnBodyTypeChanged(int value) => NotifyChange();
    partial void OnMassChanged(float value) => NotifyChange();
    partial void OnLinearDampingChanged(float value) => NotifyChange();
    partial void OnAngularDampingChanged(float value) => NotifyChange();
    partial void OnFrictionChanged(float value) => NotifyChange();
    partial void OnRestitutionChanged(float value) => NotifyChange();
    partial void OnUseGravityChanged(bool value) => NotifyChange();
}

// -------------------------------------------------------------
// 10. Rigidbody 2D
// -------------------------------------------------------------
public partial class Rigidbody2DComponentVM : ComponentEditorViewModel
{
    public static string[] BodyTypeOptions { get; } = ["Static", "Dynamic", "Kinematic"];
    private bool _isUpdating;

    [ObservableProperty] private int _bodyType = 0;
    [ObservableProperty] private float _gravityScale = 1f;
    [ObservableProperty] private float _linearDamping = 0.6f;
    [ObservableProperty] private float _angularDamping = 0.2f;
    [ObservableProperty] private bool _fixedRotation = false;
    [ObservableProperty] private bool _isAwake = true;
    [ObservableProperty] private bool _isEnabled2D = true;

    public Rigidbody2DComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is Rigidbody2DData rd)
        {
            BodyType = rd.BodyType;
            GravityScale = rd.GravityScale;
            LinearDamping = rd.LinearDamping;
            AngularDamping = rd.AngularDamping;
            FixedRotation = rd.FixedRotation;
            IsAwake = rd.IsAwake;
            IsEnabled2D = rd.IsEnabled;
        }
        _isUpdating = false;
    }

    public Rigidbody2DComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityRigidbody2D(_entityId, BodyType, GravityScale, LinearDamping, AngularDamping, FixedRotation, IsAwake, IsEnabled2D);
    }

    partial void OnBodyTypeChanged(int value) => NotifyChange();
    partial void OnGravityScaleChanged(float value) => NotifyChange();
    partial void OnLinearDampingChanged(float value) => NotifyChange();
    partial void OnAngularDampingChanged(float value) => NotifyChange();
    partial void OnFixedRotationChanged(bool value) => NotifyChange();
    partial void OnIsAwakeChanged(bool value) => NotifyChange();
    partial void OnIsEnabled2DChanged(bool value) => NotifyChange();
}

// -------------------------------------------------------------
// 11. Box Collider (3D)
// -------------------------------------------------------------
public partial class BoxColliderComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private float _centerX;
    [ObservableProperty] private float _centerY;
    [ObservableProperty] private float _centerZ;
    [ObservableProperty] private float _sizeX = 1f;
    [ObservableProperty] private float _sizeY = 1f;
    [ObservableProperty] private float _sizeZ = 1f;

    public BoxColliderComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is BoxColliderData bcd)
        {
            CenterX = bcd.Center.X;
            CenterY = bcd.Center.Y;
            CenterZ = bcd.Center.Z;
            SizeX = bcd.Size.X;
            SizeY = bcd.Size.Y;
            SizeZ = bcd.Size.Z;
        }
        _isUpdating = false;
    }

    public BoxColliderComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityBoxCollider(_entityId, new Vector3(CenterX, CenterY, CenterZ), new Vector3(SizeX, SizeY, SizeZ));
    }

    partial void OnCenterXChanged(float value) => NotifyChange();
    partial void OnCenterYChanged(float value) => NotifyChange();
    partial void OnCenterZChanged(float value) => NotifyChange();
    partial void OnSizeXChanged(float value) => NotifyChange();
    partial void OnSizeYChanged(float value) => NotifyChange();
    partial void OnSizeZChanged(float value) => NotifyChange();
}

// -------------------------------------------------------------
// 12. Sphere Collider (3D)
// -------------------------------------------------------------
public partial class SphereColliderComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private float _centerX;
    [ObservableProperty] private float _centerY;
    [ObservableProperty] private float _centerZ;
    [ObservableProperty] private float _radius = 1f;

    public SphereColliderComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is SphereColliderData scd)
        {
            CenterX = scd.Center.X;
            CenterY = scd.Center.Y;
            CenterZ = scd.Center.Z;
            Radius = scd.Radius;
        }
        _isUpdating = false;
    }

    public SphereColliderComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntitySphereCollider(_entityId, new Vector3(CenterX, CenterY, CenterZ), Radius);
    }

    partial void OnCenterXChanged(float value) => NotifyChange();
    partial void OnCenterYChanged(float value) => NotifyChange();
    partial void OnCenterZChanged(float value) => NotifyChange();
    partial void OnRadiusChanged(float value) => NotifyChange();
}

// -------------------------------------------------------------
// 13. Capsule Collider (3D)
// -------------------------------------------------------------
public partial class CapsuleColliderComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private float _centerX;
    [ObservableProperty] private float _centerY;
    [ObservableProperty] private float _centerZ;
    [ObservableProperty] private float _radius = 0.5f;
    [ObservableProperty] private float _height = 2f;

    public CapsuleColliderComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is CapsuleColliderData ccd)
        {
            CenterX = ccd.Center.X;
            CenterY = ccd.Center.Y;
            CenterZ = ccd.Center.Z;
            Radius = ccd.Radius;
            Height = ccd.Height;
        }
        _isUpdating = false;
    }

    public CapsuleColliderComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityCapsuleCollider(_entityId, new Vector3(CenterX, CenterY, CenterZ), Radius, Height);
    }

    partial void OnCenterXChanged(float value) => NotifyChange();
    partial void OnCenterYChanged(float value) => NotifyChange();
    partial void OnCenterZChanged(float value) => NotifyChange();
    partial void OnRadiusChanged(float value) => NotifyChange();
    partial void OnHeightChanged(float value) => NotifyChange();
}

// -------------------------------------------------------------
// 14. Box Collider 2D
// -------------------------------------------------------------
public partial class BoxCollider2DComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private float _offsetX;
    [ObservableProperty] private float _offsetY;
    [ObservableProperty] private float _sizeX = 0.5f;
    [ObservableProperty] private float _sizeY = 0.5f;
    [ObservableProperty] private float _density = 1f;
    [ObservableProperty] private float _friction = 0.5f;
    [ObservableProperty] private float _restitution = 0.1f;
    [ObservableProperty] private bool _isSensor = false;

    public BoxCollider2DComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is BoxCollider2DData bcd)
        {
            OffsetX = bcd.Offset.X;
            OffsetY = bcd.Offset.Y;
            SizeX = bcd.Size.X;
            SizeY = bcd.Size.Y;
            Density = bcd.Density;
            Friction = bcd.Friction;
            Restitution = bcd.Restitution;
            IsSensor = bcd.IsSensor;
        }
        _isUpdating = false;
    }

    public BoxCollider2DComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityBoxCollider2D(_entityId, new Vector2(OffsetX, OffsetY), new Vector2(SizeX, SizeY), Density, Friction, Restitution, IsSensor);
    }

    partial void OnOffsetXChanged(float value) => NotifyChange();
    partial void OnOffsetYChanged(float value) => NotifyChange();
    partial void OnSizeXChanged(float value) => NotifyChange();
    partial void OnSizeYChanged(float value) => NotifyChange();
    partial void OnDensityChanged(float value) => NotifyChange();
    partial void OnFrictionChanged(float value) => NotifyChange();
    partial void OnRestitutionChanged(float value) => NotifyChange();
    partial void OnIsSensorChanged(bool value) => NotifyChange();
}

// -------------------------------------------------------------
// 15. Circle Collider 2D
// -------------------------------------------------------------
public partial class CircleCollider2DComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private float _centerX;
    [ObservableProperty] private float _centerY;
    [ObservableProperty] private float _radius = 0.5f;
    [ObservableProperty] private float _density = 1f;
    [ObservableProperty] private float _friction = 0.5f;
    [ObservableProperty] private float _restitution = 0.1f;
    [ObservableProperty] private bool _isSensor = false;

    public CircleCollider2DComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is CircleCollider2DData ccd)
        {
            CenterX = ccd.Center.X;
            CenterY = ccd.Center.Y;
            Radius = ccd.Radius;
            Density = ccd.Density;
            Friction = ccd.Friction;
            Restitution = ccd.Restitution;
            IsSensor = ccd.IsSensor;
        }
        _isUpdating = false;
    }

    public CircleCollider2DComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityCircleCollider2D(_entityId, new Vector2(CenterX, CenterY), Radius, Density, Friction, Restitution, IsSensor);
    }

    partial void OnCenterXChanged(float value) => NotifyChange();
    partial void OnCenterYChanged(float value) => NotifyChange();
    partial void OnRadiusChanged(float value) => NotifyChange();
    partial void OnDensityChanged(float value) => NotifyChange();
    partial void OnFrictionChanged(float value) => NotifyChange();
    partial void OnRestitutionChanged(float value) => NotifyChange();
    partial void OnIsSensorChanged(bool value) => NotifyChange();
}

// -------------------------------------------------------------
// 16. Character Controller
// -------------------------------------------------------------
public partial class CharacterControllerComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private float _radius = 0.5f;
    [ObservableProperty] private float _height = 2f;
    [ObservableProperty] private float _stepHeight = 0.4f;
    [ObservableProperty] private float _slopeAngle = 45f;
    [ObservableProperty] private float _mass = 80f;
    [ObservableProperty] private float _friction = 0.2f;

    public CharacterControllerComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is CharacterControllerData ccd)
        {
            Radius = ccd.Radius;
            Height = ccd.Height;
            StepHeight = ccd.MaxStepHeight;
            SlopeAngle = ccd.MaxSlopeAngle;
            Mass = ccd.Mass;
            Friction = ccd.Friction;
        }
        _isUpdating = false;
    }

    public CharacterControllerComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityCharacterController(_entityId, Radius, Height, StepHeight, SlopeAngle, Mass, Friction);
    }

    partial void OnRadiusChanged(float value) => NotifyChange();
    partial void OnHeightChanged(float value) => NotifyChange();
    partial void OnStepHeightChanged(float value) => NotifyChange();
    partial void OnSlopeAngleChanged(float value) => NotifyChange();
    partial void OnMassChanged(float value) => NotifyChange();
    partial void OnFrictionChanged(float value) => NotifyChange();
}

// -------------------------------------------------------------
// 17. Audio Source
// -------------------------------------------------------------
public partial class AudioSourceComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private float _volume = 1f;
    [ObservableProperty] private float _pitch = 1f;
    [ObservableProperty] private float _pan = 0f;
    [ObservableProperty] private bool _playOnStart = false;
    [ObservableProperty] private bool _loop = false;

    public AudioSourceComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is AudioSourceData asd)
        {
            Volume = asd.Volume;
            Pitch = asd.Pitch;
            Pan = asd.Pan;
            PlayOnStart = asd.PlayOnStart;
            Loop = asd.Loop;
        }
        _isUpdating = false;
    }

    public AudioSourceComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityAudioSource(_entityId, Volume, Pitch, Pan, PlayOnStart, Loop);
    }

    partial void OnVolumeChanged(float value) => NotifyChange();
    partial void OnPitchChanged(float value) => NotifyChange();
    partial void OnPanChanged(float value) => NotifyChange();
    partial void OnPlayOnStartChanged(bool value) => NotifyChange();
    partial void OnLoopChanged(bool value) => NotifyChange();
}

// -------------------------------------------------------------
// 18. Text
// -------------------------------------------------------------
public partial class TextComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private string _text = "Empty";
    [ObservableProperty] private Color _colorValue = Colors.White;
    [ObservableProperty] private float _kerning = 0f;
    [ObservableProperty] private float _lineSpacing = -0.025f;
    [ObservableProperty] private bool _screenSpace = false;

    public TextComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is TextData td)
        {
            Text = td.Text;
            ColorValue = Vector4ToColor(td.Color);
            Kerning = td.Kerning;
            LineSpacing = td.LineSpacing;
            ScreenSpace = td.ScreenSpace;
        }
        _isUpdating = false;
    }

    public TextComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityText(_entityId, Text, ColorToVector4(ColorValue), Kerning, LineSpacing, ScreenSpace);
    }

    partial void OnTextChanged(string value) => NotifyChange();
    partial void OnColorValueChanged(Color value) => NotifyChange();
    partial void OnKerningChanged(float value) => NotifyChange();
    partial void OnLineSpacingChanged(float value) => NotifyChange();
    partial void OnScreenSpaceChanged(bool value) => NotifyChange();
}

// -------------------------------------------------------------
// 19. World Environment
// -------------------------------------------------------------
public partial class WorldEnvironmentComponentVM : ComponentEditorViewModel
{
    private bool _isUpdating;

    [ObservableProperty] private float _exposure = 1.1f;
    [ObservableProperty] private float _gamma = 2.2f;
    [ObservableProperty] private float _ambient = 0.5f;
    [ObservableProperty] private float _fogDensity = 0f;
    [ObservableProperty] private Color _fogColor = Color.FromArgb(255, 128, 153, 179);
    [ObservableProperty] private float _fogStart = 10f;
    [ObservableProperty] private float _fogEnd = 100f;

    public WorldEnvironmentComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        _isUpdating = true;
        if (model.Data is WorldEnvironmentData wed)
        {
            Exposure = wed.Exposure;
            Gamma = wed.Gamma;
            Ambient = wed.Ambient;
            FogDensity = wed.FogDensity;
            FogColor = Vector4ToColor(wed.FogColor);
            FogStart = wed.FogStart;
            FogEnd = wed.FogEnd;
        }
        _isUpdating = false;
    }

    public WorldEnvironmentComponentVM() { }

    private void NotifyChange()
    {
        if (_isUpdating) return;
        _sceneService?.SetEntityWorldEnvironment(_entityId, Exposure, Gamma, Ambient, FogDensity, ColorToVector4(FogColor), FogStart, FogEnd);
    }

    partial void OnExposureChanged(float value) => NotifyChange();
    partial void OnGammaChanged(float value) => NotifyChange();
    partial void OnAmbientChanged(float value) => NotifyChange();
    partial void OnFogDensityChanged(float value) => NotifyChange();
    partial void OnFogColorChanged(Color value) => NotifyChange();
    partial void OnFogStartChanged(float value) => NotifyChange();
    partial void OnFogEndChanged(float value) => NotifyChange();
}

// -------------------------------------------------------------
// 20. Script
// -------------------------------------------------------------
public partial class ScriptComponentVM : ComponentEditorViewModel
{
    [ObservableProperty] private string _className = string.Empty;

    public ScriptComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService)
    {
        if (model.Data is ScriptData sd)
        {
            ClassName = sd.ClassName;
        }
    }

    public ScriptComponentVM() { }
}

// -------------------------------------------------------------
// 21. Generic
// -------------------------------------------------------------
public partial class GenericComponentVM : ComponentEditorViewModel
{
    public GenericComponentVM(ComponentModel model, Guid entityId, ISceneService? sceneService)
        : base(model, entityId, sceneService) { }
    public GenericComponentVM() { }
}

