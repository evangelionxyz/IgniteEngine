using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;

namespace IgniteEditor.Controls;

public partial class ColorPickerPropertyControl : UserControl
{
    public static readonly StyledProperty<string> LabelProperty =
        AvaloniaProperty.Register<ColorPickerPropertyControl, string>(
            nameof(Label),
            defaultValue: "Color");

    public static readonly StyledProperty<Color> ValueProperty =
        AvaloniaProperty.Register<ColorPickerPropertyControl, Color>(
            nameof(Value),
            defaultValue: Colors.White,
            defaultBindingMode: BindingMode.TwoWay);

    public static readonly StyledProperty<Color> DefaultValueProperty =
        AvaloniaProperty.Register<ColorPickerPropertyControl, Color>(
            nameof(DefaultValue),
            defaultValue: Colors.White);

    public static readonly StyledProperty<IBrush> SwatchBrushProperty =
        AvaloniaProperty.Register<ColorPickerPropertyControl, IBrush>(
            nameof(SwatchBrush),
            new SolidColorBrush(Colors.White));

    public static readonly StyledProperty<string> HexStringProperty =
        AvaloniaProperty.Register<ColorPickerPropertyControl, string>(
            nameof(HexString),
            "#FFFFFFFF");

    public static readonly StyledProperty<bool> HasChangedProperty =
        AvaloniaProperty.Register<ColorPickerPropertyControl, bool>(
            nameof(HasChanged),
            defaultValue: false);

    private bool _isUpdating;

    static ColorPickerPropertyControl()
    {
        ValueProperty.Changed.AddClassHandler<ColorPickerPropertyControl>((x, _) => x.OnColorValueChanged(x.Value));
    }

    public ColorPickerPropertyControl()
    {
        InitializeComponent();
        OnColorValueChanged(Value);
    }

    public string Label
    {
        get => GetValue(LabelProperty);
        set => SetValue(LabelProperty, value);
    }

    public Color Value
    {
        get => GetValue(ValueProperty);
        set => SetValue(ValueProperty, value);
    }

    public Color DefaultValue
    {
        get => GetValue(DefaultValueProperty);
        set => SetValue(DefaultValueProperty, value);
    }

    public IBrush SwatchBrush
    {
        get => GetValue(SwatchBrushProperty);
        set => SetValue(SwatchBrushProperty, value);
    }

    public string HexString
    {
        get => GetValue(HexStringProperty);
        set => SetValue(HexStringProperty, value);
    }

    public bool HasChanged
    {
        get => GetValue(HasChangedProperty);
        set => SetValue(HasChangedProperty, value);
    }

    private void OnColorValueChanged(Color c)
    {
        if (_isUpdating) return;
        _isUpdating = true;
        SwatchBrush = new SolidColorBrush(c);
        HexString = $"#{c.R:X2}{c.G:X2}{c.B:X2}{c.A:X2}";
        HasChanged = c != DefaultValue;
        _isUpdating = false;
    }

    private void OnPickerColorChanged(object? sender, Color c)
    {
        if (_isUpdating) return;
        Value = c;
    }

    private void OnHexLostFocus(object? sender, RoutedEventArgs e)
    {
        ApplyHexInput();
    }

    private void OnHexKeyDown(object? sender, KeyEventArgs e)
    {
        if (e.Key == Key.Enter)
        {
            ApplyHexInput();
        }
    }

    private void ApplyHexInput()
    {
        if (_isUpdating) return;
        if (Color.TryParse(HexString, out var parsed))
        {
            Value = parsed;
        }
        else
        {
            HexString = $"#{Value.R:X2}{Value.G:X2}{Value.B:X2}{Value.A:X2}";
        }
    }

    private void OnResetClicked(object? sender, RoutedEventArgs e)
    {
        Value = DefaultValue;
    }
}
