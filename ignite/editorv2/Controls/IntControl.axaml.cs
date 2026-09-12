using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Input;
using Avalonia.Interactivity;

namespace IgniteEditor.Controls;

public partial class IntControl : UserControl
{
    public static readonly StyledProperty<string> LabelProperty =
        AvaloniaProperty.Register<IntControl, string>(nameof(Label), defaultValue: "Int");

    public static readonly StyledProperty<int> ValueProperty =
        AvaloniaProperty.Register<IntControl, int>(nameof(Value), defaultValue: 0, defaultBindingMode: BindingMode.TwoWay);

    public static readonly StyledProperty<int> DefaultValueProperty =
        AvaloniaProperty.Register<IntControl, int>(nameof(DefaultValue), defaultValue: 0);

    public static readonly StyledProperty<int> MinimumProperty =
        AvaloniaProperty.Register<IntControl, int>(nameof(Minimum), defaultValue: int.MinValue);

    public static readonly StyledProperty<int> MaximumProperty =
        AvaloniaProperty.Register<IntControl, int>(nameof(Maximum), defaultValue: int.MaxValue);

    public static readonly StyledProperty<float> StepProperty =
        AvaloniaProperty.Register<IntControl, float>(nameof(Step), defaultValue: 1f);

    public static readonly StyledProperty<bool> HasChangedProperty =
        AvaloniaProperty.Register<IntControl, bool>(nameof(HasChanged), defaultValue: false);

    public static readonly StyledProperty<float> FloatValueProperty =
        AvaloniaProperty.Register<IntControl, float>(nameof(FloatValue), defaultValue: 0f, defaultBindingMode: BindingMode.TwoWay);

    public static readonly StyledProperty<float> FloatDefaultValueProperty =
        AvaloniaProperty.Register<IntControl, float>(nameof(FloatDefaultValue), defaultValue: 0f);

    public static readonly StyledProperty<float> FloatMinimumProperty =
        AvaloniaProperty.Register<IntControl, float>(nameof(FloatMinimum), defaultValue: float.MinValue);

    public static readonly StyledProperty<float> FloatMaximumProperty =
        AvaloniaProperty.Register<IntControl, float>(nameof(FloatMaximum), defaultValue: float.MaxValue);

    public string Label
    {
        get => GetValue(LabelProperty);
        set => SetValue(LabelProperty, value);
    }

    public int Value
    {
        get => GetValue(ValueProperty);
        set => SetValue(ValueProperty, value);
    }

    public int DefaultValue
    {
        get => GetValue(DefaultValueProperty);
        set => SetValue(DefaultValueProperty, value);
    }

    public int Minimum
    {
        get => GetValue(MinimumProperty);
        set => SetValue(MinimumProperty, value);
    }

    public int Maximum
    {
        get => GetValue(MaximumProperty);
        set => SetValue(MaximumProperty, value);
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

    public float FloatValue
    {
        get => GetValue(FloatValueProperty);
        set => SetValue(FloatValueProperty, value);
    }

    public float FloatDefaultValue
    {
        get => GetValue(FloatDefaultValueProperty);
        set => SetValue(FloatDefaultValueProperty, value);
    }

    public float FloatMinimum
    {
        get => GetValue(FloatMinimumProperty);
        set => SetValue(FloatMinimumProperty, value);
    }

    public float FloatMaximum
    {
        get => GetValue(FloatMaximumProperty);
        set => SetValue(FloatMaximumProperty, value);
    }

    private bool _isSyncing;

    static IntControl()
    {
        ValueProperty.Changed.AddClassHandler<IntControl>((x, _) => x.OnIntValueChanged());
        DefaultValueProperty.Changed.AddClassHandler<IntControl>((x, _) => x.OnIntDefaultChanged());
        MinimumProperty.Changed.AddClassHandler<IntControl>((x, _) => x.OnIntMinMaxChanged());
        MaximumProperty.Changed.AddClassHandler<IntControl>((x, _) => x.OnIntMinMaxChanged());
        FloatValueProperty.Changed.AddClassHandler<IntControl>((x, _) => x.OnFloatValueChanged());
    }

    public IntControl()
    {
        InitializeComponent();
        SyncFromInt();
        UpdateHasChanged();
    }

    private void OnIntValueChanged()
    {
        if (_isSyncing) return;
        _isSyncing = true;
        try
        {
            FloatValue = Value;
            UpdateHasChanged();
        }
        finally
        {
            _isSyncing = false;
        }
    }

    private void OnFloatValueChanged()
    {
        if (_isSyncing) return;
        _isSyncing = true;
        try
        {
            Value = (int)MathF.Round(FloatValue);
            UpdateHasChanged();
        }
        finally
        {
            _isSyncing = false;
        }
    }

    private void OnIntDefaultChanged()
    {
        FloatDefaultValue = DefaultValue;
        UpdateHasChanged();
    }

    private void OnIntMinMaxChanged()
    {
        FloatMinimum = Minimum;
        FloatMaximum = Maximum;
    }

    private void SyncFromInt()
    {
        FloatValue = Value;
        FloatDefaultValue = DefaultValue;
        FloatMinimum = Minimum;
        FloatMaximum = Maximum;
    }

    private void UpdateHasChanged()
    {
        HasChanged = Value != DefaultValue;
    }

    private void OnResetClicked(object? sender, RoutedEventArgs e)
    {
        Value = DefaultValue;
    }

    private Point _startDragPoint;
    private int _startDragValue;
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
                speed = Math.Max(1f, speed * 0.5f);
            else if ((e.KeyModifiers & KeyModifiers.Control) != 0)
                speed *= 10.0f;

            int newValue = _startDragValue + (int)MathF.Round((float)deltaX * speed);
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
