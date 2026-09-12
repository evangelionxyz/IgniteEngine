using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Interactivity;

namespace IgniteEditor.Controls;

public partial class Vector2Control : UserControl
{
    public static readonly StyledProperty<string> LabelProperty =
        AvaloniaProperty.Register<Vector2Control, string>(nameof(Label), defaultValue: "Vector2");

    public static readonly StyledProperty<float> XProperty =
        AvaloniaProperty.Register<Vector2Control, float>(nameof(X), defaultValue: 0f, defaultBindingMode: BindingMode.TwoWay);

    public static readonly StyledProperty<float> YProperty =
        AvaloniaProperty.Register<Vector2Control, float>(nameof(Y), defaultValue: 0f, defaultBindingMode: BindingMode.TwoWay);

    public static readonly StyledProperty<float> DefaultXProperty =
        AvaloniaProperty.Register<Vector2Control, float>(nameof(DefaultX), defaultValue: 0f);

    public static readonly StyledProperty<float> DefaultYProperty =
        AvaloniaProperty.Register<Vector2Control, float>(nameof(DefaultY), defaultValue: 0f);

    public static readonly StyledProperty<float> StepProperty =
        AvaloniaProperty.Register<Vector2Control, float>(nameof(Step), defaultValue: 0.1f);

    public static readonly StyledProperty<bool> HasChangedProperty =
        AvaloniaProperty.Register<Vector2Control, bool>(nameof(HasChanged), defaultValue: false);

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

    static Vector2Control()
    {
        XProperty.Changed.AddClassHandler<Vector2Control>((x, _) => x.UpdateHasChanged());
        YProperty.Changed.AddClassHandler<Vector2Control>((x, _) => x.UpdateHasChanged());
        DefaultXProperty.Changed.AddClassHandler<Vector2Control>((x, _) => x.UpdateHasChanged());
        DefaultYProperty.Changed.AddClassHandler<Vector2Control>((x, _) => x.UpdateHasChanged());
    }

    public Vector2Control()
    {
        InitializeComponent();
        UpdateHasChanged();
    }

    private void UpdateHasChanged()
    {
        const float eps = 0.0001f;
        HasChanged = MathF.Abs(X - DefaultX) > eps ||
                     MathF.Abs(Y - DefaultY) > eps;
    }

    private void OnResetClicked(object? sender, RoutedEventArgs e)
    {
        X = DefaultX;
        Y = DefaultY;
    }
}
