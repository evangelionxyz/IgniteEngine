using System;
using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;

namespace IgniteEditor.Controls;

public partial class NumericDragBox : UserControl
{
    public static readonly StyledProperty<float> ValueProperty =
        AvaloniaProperty.Register<NumericDragBox, float>(
            nameof(Value), 
            defaultValue: 0f, 
            defaultBindingMode: BindingMode.TwoWay);

    public static readonly StyledProperty<float> DefaultValueProperty =
        AvaloniaProperty.Register<NumericDragBox, float>(
            nameof(DefaultValue), 
            defaultValue: 0f);

    public static readonly StyledProperty<string?> BadgeProperty =
        AvaloniaProperty.Register<NumericDragBox, string?>(
            nameof(Badge), 
            defaultValue: null);

    public static readonly StyledProperty<IBrush?> BadgeBrushProperty =
        AvaloniaProperty.Register<NumericDragBox, IBrush?>(
            nameof(BadgeBrush), 
            defaultValue: null);

    public static readonly StyledProperty<float> StepProperty =
        AvaloniaProperty.Register<NumericDragBox, float>(
            nameof(Step), 
            defaultValue: 0.1f);

    public static readonly StyledProperty<float> MinimumProperty =
        AvaloniaProperty.Register<NumericDragBox, float>(
            nameof(Minimum), 
            defaultValue: float.MinValue);

    public static readonly StyledProperty<float> MaximumProperty =
        AvaloniaProperty.Register<NumericDragBox, float>(
            nameof(Maximum), 
            defaultValue: float.MaxValue);

    public static readonly StyledProperty<bool> IsIntegerProperty =
        AvaloniaProperty.Register<NumericDragBox, bool>(
            nameof(IsInteger), 
            defaultValue: false);

    public static readonly StyledProperty<string> FormatProperty =
        AvaloniaProperty.Register<NumericDragBox, string>(
            nameof(Format), 
            defaultValue: "0.###");

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

    public string? Badge
    {
        get => GetValue(BadgeProperty);
        set => SetValue(BadgeProperty, value);
    }

    public IBrush? BadgeBrush
    {
        get => GetValue(BadgeBrushProperty);
        set => SetValue(BadgeBrushProperty, value);
    }

    public float Step
    {
        get => GetValue(StepProperty);
        set => SetValue(StepProperty, value);
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

    public bool IsInteger
    {
        get => GetValue(IsIntegerProperty);
        set => SetValue(IsIntegerProperty, value);
    }

    public string Format
    {
        get => GetValue(FormatProperty);
        set => SetValue(FormatProperty, value);
    }

    private Point _startDragPoint;
    private float _startDragValue;
    private bool _isDragging;
    private bool _isPressed;
    private bool _isUpdatingText;

    static NumericDragBox()
    {
        ValueProperty.Changed.AddClassHandler<NumericDragBox>((x, e) => x.OnValueChanged(e));
        BadgeProperty.Changed.AddClassHandler<NumericDragBox>((x, e) => x.OnBadgeChanged(e));
        BadgeBrushProperty.Changed.AddClassHandler<NumericDragBox>((x, e) => x.OnBadgeBrushChanged(e));
    }

    public NumericDragBox()
    {
        InitializeComponent();
        UpdateBadge();
        UpdateTextBox();
    }

    private void OnValueChanged(AvaloniaPropertyChangedEventArgs e)
    {
        if (!_isUpdatingText)
        {
            UpdateTextBox();
        }
    }

    private void OnBadgeChanged(AvaloniaPropertyChangedEventArgs e)
    {
        UpdateBadge();
    }

    private void OnBadgeBrushChanged(AvaloniaPropertyChangedEventArgs e)
    {
        UpdateBadge();
    }

    private void UpdateBadge()
    {
        if (BadgeTextBlock == null || BadgeBorder == null) return;

        if (string.IsNullOrEmpty(Badge))
        {
            BadgeBorder.Width = 12;
            BadgeBorder.Background = new SolidColorBrush(Color.Parse("#2C2C2C"));
            BadgeTextBlock.Text = "↔";
            BadgeTextBlock.FontSize = 9;
            BadgeTextBlock.Foreground = new SolidColorBrush(Color.Parse("#707070"));
        }
        else
        {
            BadgeBorder.Width = 16;
            BadgeBorder.Background = BadgeBrush ?? new SolidColorBrush(Color.Parse("#404040"));
            BadgeTextBlock.Text = Badge;
            BadgeTextBlock.FontSize = 10;
            BadgeTextBlock.Foreground = new SolidColorBrush(Color.Parse("#FFFFFF"));
        }
    }

    private void UpdateTextBox()
    {
        if (ValueTextBox == null) return;

        _isUpdatingText = true;
        try
        {
            ValueTextBox.Text = FormatValue(Value);
        }
        finally
        {
            _isUpdatingText = false;
        }
    }

    private string FormatValue(float val)
    {
        if (IsInteger)
        {
            return ((int)MathF.Round(val)).ToString(CultureInfo.InvariantCulture);
        }
        return val.ToString(Format, CultureInfo.InvariantCulture);
    }

    private void OnDragHandlePointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
        {
            _isPressed = true;
            _isDragging = false;
            _startDragPoint = e.GetPosition(this);
            _startDragValue = Value;
            e.Pointer.Capture(BadgeBorder);
            e.Handled = true;
        }
    }

    private void OnDragHandlePointerMoved(object? sender, PointerEventArgs e)
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
            if (IsInteger)
                newValue = MathF.Round(newValue);

            newValue = Math.Clamp(newValue, Minimum, Maximum);
            Value = newValue;
            UpdateTextBox();
            e.Handled = true;
        }
    }

    private void OnDragHandlePointerReleased(object? sender, PointerReleasedEventArgs e)
    {
        if (_isPressed)
        {
            _isPressed = false;
            e.Pointer.Capture(null);
            e.Handled = true;

            if (!_isDragging)
            {
                // Clicked without dragging: focus text box and select all for direct editing
                ValueTextBox.Focus();
                ValueTextBox.SelectAll();
            }
            _isDragging = false;
        }
    }

    private void OnDragHandlePointerCaptureLost(object? sender, PointerCaptureLostEventArgs e)
    {
        _isPressed = false;
        _isDragging = false;
    }

    private void OnTextBoxLostFocus(object? sender, RoutedEventArgs e)
    {
        CommitTextValue();
    }

    private void OnTextBoxKeyDown(object? sender, KeyEventArgs e)
    {
        if (e.Key == Key.Enter)
        {
            CommitTextValue();
            ValueTextBox.SelectAll();
            e.Handled = true;
        }
        else if (e.Key == Key.Escape)
        {
            UpdateTextBox();
            TopLevel.GetTopLevel(this)?.FocusManager?.Focus(null);
            e.Handled = true;
        }
    }

    private void CommitTextValue()
    {
        if (float.TryParse(ValueTextBox.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out float parsed))
        {
            if (IsInteger)
                parsed = MathF.Round(parsed);

            parsed = Math.Clamp(parsed, Minimum, Maximum);
            Value = parsed;
        }
        UpdateTextBox();
    }
}
