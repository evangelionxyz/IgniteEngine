using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Interactivity;

namespace IgniteEditor.Controls;

public partial class Vector4Control : UserControl
{
    public static readonly StyledProperty<string> LabelProperty =
        AvaloniaProperty.Register<Vector4Control, string>(nameof(Label), defaultValue: "Vector4");

    public static readonly StyledProperty<float> XProperty =
        AvaloniaProperty.Register<Vector4Control, float>(nameof(X), defaultValue: 0f, defaultBindingMode: BindingMode.TwoWay);

    public static readonly StyledProperty<float> YProperty =
        AvaloniaProperty.Register<Vector4Control, float>(nameof(Y), defaultValue: 0f, defaultBindingMode: BindingMode.TwoWay);

    public static readonly StyledProperty<float> ZProperty =
        AvaloniaProperty.Register<Vector4Control, float>(nameof(Z), defaultValue: 0f, defaultBindingMode: BindingMode.TwoWay);

    public static readonly StyledProperty<float> WProperty =
        AvaloniaProperty.Register<Vector4Control, float>(nameof(W), defaultValue: 0f, defaultBindingMode: BindingMode.TwoWay);

    public static readonly StyledProperty<float> DefaultXProperty =
        AvaloniaProperty.Register<Vector4Control, float>(nameof(DefaultX), defaultValue: 0f);

    public static readonly StyledProperty<float> DefaultYProperty =
        AvaloniaProperty.Register<Vector4Control, float>(nameof(DefaultY), defaultValue: 0f);

    public static readonly StyledProperty<float> DefaultZProperty =
        AvaloniaProperty.Register<Vector4Control, float>(nameof(DefaultZ), defaultValue: 0f);

    public static readonly StyledProperty<float> DefaultWProperty =
        AvaloniaProperty.Register<Vector4Control, float>(nameof(DefaultW), defaultValue: 0f);

    public static readonly StyledProperty<float> StepProperty =
        AvaloniaProperty.Register<Vector4Control, float>(nameof(Step), defaultValue: 0.1f);

    public static readonly StyledProperty<bool> HasChangedProperty =
        AvaloniaProperty.Register<Vector4Control, bool>(nameof(HasChanged), defaultValue: false);

    public string Label
    {
        get => GetValue(LabelProperty);
        set => SetValue(LabelProperty, value);
    }

    public float X
    {
        get => GetValue(XProperty);
        set => SetValue(XProperty, value);
    }

    public float Y
    {
        get => GetValue(YProperty);
        set => SetValue(YProperty, value);
    }

    public float Z
    {
        get => GetValue(ZProperty);
        set => SetValue(ZProperty, value);
    }

    public float W
    {
        get => GetValue(WProperty);
        set => SetValue(WProperty, value);
    }

    public float DefaultX
    {
        get => GetValue(DefaultXProperty);
        set => SetValue(DefaultXProperty, value);
    }

    public float DefaultY
    {
        get => GetValue(DefaultYProperty);
        set => SetValue(DefaultYProperty, value);
    }

    public float DefaultZ
    {
        get => GetValue(DefaultZProperty);
        set => SetValue(DefaultZProperty, value);
    }

    public float DefaultW
    {
        get => GetValue(DefaultWProperty);
        set => SetValue(DefaultWProperty, value);
    }

    public float Step
    {
        get => GetValue(StepProperty);
        set => SetValue(StepProperty, value);
    }

    public bool HasChanged
    {
        get => GetValue(HasChangedProperty);
        set => SetValue(HasChangedProperty, value);
    }

    static Vector4Control()
    {
        XProperty.Changed.AddClassHandler<Vector4Control>((x, _) => x.UpdateHasChanged());
        YProperty.Changed.AddClassHandler<Vector4Control>((x, _) => x.UpdateHasChanged());
        ZProperty.Changed.AddClassHandler<Vector4Control>((x, _) => x.UpdateHasChanged());
        WProperty.Changed.AddClassHandler<Vector4Control>((x, _) => x.UpdateHasChanged());
        DefaultXProperty.Changed.AddClassHandler<Vector4Control>((x, _) => x.UpdateHasChanged());
        DefaultYProperty.Changed.AddClassHandler<Vector4Control>((x, _) => x.UpdateHasChanged());
        DefaultZProperty.Changed.AddClassHandler<Vector4Control>((x, _) => x.UpdateHasChanged());
        DefaultWProperty.Changed.AddClassHandler<Vector4Control>((x, _) => x.UpdateHasChanged());
    }

    public Vector4Control()
    {
        InitializeComponent();
        UpdateHasChanged();
    }

    private void UpdateHasChanged()
    {
        const float eps = 0.0001f;
        HasChanged = MathF.Abs(X - DefaultX) > eps ||
                     MathF.Abs(Y - DefaultY) > eps ||
                     MathF.Abs(Z - DefaultZ) > eps ||
                     MathF.Abs(W - DefaultW) > eps;
    }

    private void OnResetClicked(object? sender, RoutedEventArgs e)
    {
        X = DefaultX;
        Y = DefaultY;
        Z = DefaultZ;
        W = DefaultW;
    }
}
