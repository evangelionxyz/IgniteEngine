using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Input;
using Avalonia.Interactivity;

namespace IgniteEditor.Controls;

public partial class FloatControl : UserControl
{
    public static readonly StyledProperty<string> LabelProperty =
        AvaloniaProperty.Register<FloatControl, string>(nameof(Label), defaultValue: "Float");

    public static readonly StyledProperty<float> ValueProperty =
        AvaloniaProperty.Register<FloatControl, float>(nameof(Value), defaultValue: 0f, defaultBindingMode: BindingMode.TwoWay);

    public static readonly StyledProperty<float> DefaultValueProperty =
        AvaloniaProperty.Register<FloatControl, float>(nameof(DefaultValue), defaultValue: 0f);

    public static readonly StyledProperty<float> MinimumProperty =
        AvaloniaProperty.Register<FloatControl, float>(nameof(Minimum), defaultValue: float.MinValue);

    public static readonly StyledProperty<float> MaximumProperty =
        AvaloniaProperty.Register<FloatControl, float>(nameof(Maximum), defaultValue: float.MaxValue);

    public static readonly StyledProperty<float> StepProperty =
        AvaloniaProperty.Register<FloatControl, float>(nameof(Step), defaultValue: 0.1f);

    public static readonly StyledProperty<string> FormatProperty =
        AvaloniaProperty.Register<FloatControl, string>(nameof(Format), defaultValue: "0.###");

    public static readonly StyledProperty<bool> HasChangedProperty =
        AvaloniaProperty.Register<FloatControl, bool>(nameof(HasChanged), defaultValue: false);

    public string Label
    {
        get => GetValue(LabelProperty);
        set => SetValue(LabelProperty, value);
    }

    public float Value
    {
        get => GetValue(ValueProperty);
        set => SetValue(ValueProperty, value);
    }

    public float DefaultValue
    {
        get => GetValue(DefaultValueProperty);
        set => SetValue(DefaultValueProperty, value);
    }

    public float Minimum
    {
        get => GetValue(MinimumProperty);
        set => SetValue(MinimumProperty, value);
    }

    public float Maximum
    {
        get => GetValue(MaximumProperty);
        set => SetValue(MaximumProperty, value);
    }

    public float Step
    {
        get => GetValue(StepProperty);
        set => SetValue(StepProperty, value);
    }

    public string Format
    {
        get => GetValue(FormatProperty);
        set => SetValue(FormatProperty, value);
    }

    public bool HasChanged
    {
        get => GetValue(HasChangedProperty);
        set => SetValue(HasChangedProperty, value);
    }

    static FloatControl()
    {
        ValueProperty.Changed.AddClassHandler<FloatControl>((x, _) => x.UpdateHasChanged());
        DefaultValueProperty.Changed.AddClassHandler<FloatControl>((x, _) => x.UpdateHasChanged());
    }

    public FloatControl()
    {
        InitializeComponent();
        UpdateHasChanged();
    }

    private void UpdateHasChanged()
    {
        const float eps = 0.0001f;
        HasChanged = MathF.Abs(Value - DefaultValue) > eps;
    }

    private void OnResetClicked(object? sender, RoutedEventArgs e)
    {
        Value = DefaultValue;
    }

    private Point _startDragPoint;
    private float _startDragValue;
    private bool _isDragging;
    private bool _isPressed;

    private void OnLabelPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
        {
            _isPressed = true;
            _isDragging = false;
            _startDragPoint = e.GetPosition(this);
            _startDragValue = Value;
            e.Pointer.Capture((IInputElement)sender!);
            e.Handled = true;
        }
    }

    private void OnLabelPointerMoved(object? sender, PointerEventArgs e)
    {
        if (!_isPressed) return;

        var currentPoint = e.GetPosition(this);
        var deltaX = currentPoint.X - _startDragPoint.X;

        if (!_isDragging && Math.Abs(deltaX) > 2)
        {
            _isDragging = true;
        }

        if (_isDragging)
        {
            float speed = Step;
            if ((e.KeyModifiers & KeyModifiers.Shift) != 0)
                speed *= 0.1f;
            else if ((e.KeyModifiers & KeyModifiers.Control) != 0)
                speed *= 10.0f;

            float newValue = _startDragValue + (float)deltaX * speed;
            newValue = Math.Clamp(newValue, Minimum, Maximum);
            Value = newValue;
            e.Handled = true;
        }
    }

    private void OnLabelPointerReleased(object? sender, PointerReleasedEventArgs e)
    {
        if (_isPressed)
        {
            _isPressed = false;
            e.Pointer.Capture(null);
            _isDragging = false;
            e.Handled = true;
        }
    }

    private void OnLabelPointerCaptureLost(object? sender, PointerCaptureLostEventArgs e)
    {
        _isPressed = false;
        _isDragging = false;
    }
}
