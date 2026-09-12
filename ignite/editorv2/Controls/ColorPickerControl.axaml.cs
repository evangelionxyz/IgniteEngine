using System;
using System.Globalization;
using System.Numerics;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;

namespace IgniteEditor.Controls;

public partial class ColorPickerControl : UserControl
{
    public static readonly StyledProperty<Color> SelectedColorProperty =
        AvaloniaProperty.Register<ColorPickerControl, Color>(
            nameof(SelectedColor),
            Colors.White,
            defaultBindingMode: BindingMode.TwoWay);

    public static readonly StyledProperty<Vector4> VectorColorProperty =
        AvaloniaProperty.Register<ColorPickerControl, Vector4>(
            nameof(VectorColor),
            Vector4.One,
            defaultBindingMode: BindingMode.TwoWay);

    public static readonly StyledProperty<Color> OriginalColorProperty =
        AvaloniaProperty.Register<ColorPickerControl, Color>(
            nameof(OriginalColor),
            Colors.White);

    public static readonly StyledProperty<IBrush> CurrentHueBrushProperty =
        AvaloniaProperty.Register<ColorPickerControl, IBrush>(
            nameof(CurrentHueBrush),
            new SolidColorBrush(Colors.Red));

    public static readonly StyledProperty<IBrush> NewColorBrushProperty =
        AvaloniaProperty.Register<ColorPickerControl, IBrush>(
            nameof(NewColorBrush),
            new SolidColorBrush(Colors.White));

    public static readonly StyledProperty<IBrush> OriginalColorBrushProperty =
        AvaloniaProperty.Register<ColorPickerControl, IBrush>(
            nameof(OriginalColorBrush),
            new SolidColorBrush(Colors.White));

    public static readonly StyledProperty<IBrush> AlphaSliderBrushProperty =
        AvaloniaProperty.Register<ColorPickerControl, IBrush>(
            nameof(AlphaSliderBrush),
            new SolidColorBrush(Colors.White));

    public static readonly StyledProperty<string> InputHProperty =
        AvaloniaProperty.Register<ColorPickerControl, string>(nameof(InputH), "0");

    public static readonly StyledProperty<string> InputSProperty =
        AvaloniaProperty.Register<ColorPickerControl, string>(nameof(InputS), "0");

    public static readonly StyledProperty<string> InputVProperty =
        AvaloniaProperty.Register<ColorPickerControl, string>(nameof(InputV), "100");

    public static readonly StyledProperty<string> InputLProperty =
        AvaloniaProperty.Register<ColorPickerControl, string>(nameof(InputL), "100");

    public static readonly StyledProperty<string> InputRProperty =
        AvaloniaProperty.Register<ColorPickerControl, string>(nameof(InputR), "255");

    public static readonly StyledProperty<string> InputGProperty =
        AvaloniaProperty.Register<ColorPickerControl, string>(nameof(InputG), "255");

    public static readonly StyledProperty<string> InputBProperty =
        AvaloniaProperty.Register<ColorPickerControl, string>(nameof(InputB), "255");

    public static readonly StyledProperty<string> InputAProperty =
        AvaloniaProperty.Register<ColorPickerControl, string>(nameof(InputA), "255");

    public static readonly StyledProperty<string> InputHexProperty =
        AvaloniaProperty.Register<ColorPickerControl, string>(nameof(InputHex), "FFFFFFFF");

    public event EventHandler<Color>? ColorChanged;
    public event EventHandler<Vector4>? VectorColorChanged;

    private float _h = 0f; // 0 - 360
    private float _s = 0f; // 0 - 1
    private float _v = 1f; // 0 - 1
    private float _a = 1f; // 0 - 1

    private bool _isInternalUpdate;
    private bool _isDraggingSv;
    private bool _isDraggingHue;
    private bool _isDraggingAlpha;

    static ColorPickerControl()
    {
        SelectedColorProperty.Changed.AddClassHandler<ColorPickerControl>((x, _) => x.OnSelectedColorChanged(x.SelectedColor));
        VectorColorProperty.Changed.AddClassHandler<ColorPickerControl>((x, _) => x.OnVectorColorChanged(x.VectorColor));
    }

    public ColorPickerControl()
    {
        InitializeComponent();
        UpdateAllFromHsv();
    }

    public Color SelectedColor
    {
        get => GetValue(SelectedColorProperty);
        set => SetValue(SelectedColorProperty, value);
    }

    public Vector4 VectorColor
    {
        get => GetValue(VectorColorProperty);
        set => SetValue(VectorColorProperty, value);
    }

    public Color OriginalColor
    {
        get => GetValue(OriginalColorProperty);
        set => SetValue(OriginalColorProperty, value);
    }

    public IBrush CurrentHueBrush
    {
        get => GetValue(CurrentHueBrushProperty);
        set => SetValue(CurrentHueBrushProperty, value);
    }

    public IBrush NewColorBrush
    {
        get => GetValue(NewColorBrushProperty);
        set => SetValue(NewColorBrushProperty, value);
    }

    public IBrush OriginalColorBrush
    {
        get => GetValue(OriginalColorBrushProperty);
        set => SetValue(OriginalColorBrushProperty, value);
    }

    public IBrush AlphaSliderBrush
    {
        get => GetValue(AlphaSliderBrushProperty);
        set => SetValue(AlphaSliderBrushProperty, value);
    }

    public string InputH { get => GetValue(InputHProperty); set => SetValue(InputHProperty, value); }
    public string InputS { get => GetValue(InputSProperty); set => SetValue(InputSProperty, value); }
    public string InputV { get => GetValue(InputVProperty); set => SetValue(InputVProperty, value); }
    public string InputL { get => GetValue(InputLProperty); set => SetValue(InputLProperty, value); }
    public string InputR { get => GetValue(InputRProperty); set => SetValue(InputRProperty, value); }
    public string InputG { get => GetValue(InputGProperty); set => SetValue(InputGProperty, value); }
    public string InputB { get => GetValue(InputBProperty); set => SetValue(InputBProperty, value); }
    public string InputA { get => GetValue(InputAProperty); set => SetValue(InputAProperty, value); }
    public string InputHex { get => GetValue(InputHexProperty); set => SetValue(InputHexProperty, value); }

    private void OnSelectedColorChanged(Color color)
    {
        if (_isInternalUpdate) return;
        _isInternalUpdate = true;
        var (h, s, v) = RgbToHsv(color.R / 255f, color.G / 255f, color.B / 255f);
        _h = h;
        _s = s;
        _v = v;
        _a = color.A / 255f;
        UpdateAllFromHsv(triggerEvents: false);
        _isInternalUpdate = false;
    }

    private void OnVectorColorChanged(Vector4 vCol)
    {
        if (_isInternalUpdate) return;
        _isInternalUpdate = true;
        var (h, s, v) = RgbToHsv(vCol.X, vCol.Y, vCol.Z);
        _h = h;
        _s = s;
        _v = v;
        _a = Math.Clamp(vCol.W, 0f, 1f);
        UpdateAllFromHsv(triggerEvents: false);
        _isInternalUpdate = false;
    }

    private void UpdateAllFromHsv(bool triggerEvents = true)
    {
        var (r, g, b) = HsvToRgb(_h, _s, _v);
        byte rB = (byte)(Math.Clamp(r, 0f, 1f) * 255f);
        byte gB = (byte)(Math.Clamp(g, 0f, 1f) * 255f);
        byte bB = (byte)(Math.Clamp(b, 0f, 1f) * 255f);
        byte aB = (byte)(Math.Clamp(_a, 0f, 1f) * 255f);

        Color newCol = Color.FromArgb(aB, rB, gB, bB);
        Vector4 newVec = new Vector4(r, g, b, _a);

        // Pure hue for SV background
        var (pureR, pureG, pureB) = HsvToRgb(_h, 1f, 1f);
        CurrentHueBrush = new SolidColorBrush(Color.FromRgb(
            (byte)(pureR * 255f), (byte)(pureG * 255f), (byte)(pureB * 255f)));

        NewColorBrush = new SolidColorBrush(newCol);
        OriginalColorBrush = new SolidColorBrush(OriginalColor);

        // Alpha slider gradient brush
        AlphaSliderBrush = new LinearGradientBrush
        {
            StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
            EndPoint = new RelativePoint(0, 1, RelativeUnit.Relative),
            GradientStops =
            {
                new GradientStop(Color.FromRgb(rB, gB, bB), 0.0),
                new GradientStop(Color.FromArgb(0, rB, gB, bB), 1.0)
            }
        };

        // Textual properties
        InputH = Math.Round(_h).ToString(CultureInfo.InvariantCulture);
        InputS = Math.Round(_s * 100f).ToString(CultureInfo.InvariantCulture);
        InputV = Math.Round(_v * 100f).ToString(CultureInfo.InvariantCulture);

        var (_, hslS, hslL) = RgbToHsl(r, g, b);
        InputL = Math.Round(hslL * 100f).ToString(CultureInfo.InvariantCulture);

        InputR = rB.ToString();
        InputG = gB.ToString();
        InputB = bB.ToString();
        InputA = aB.ToString();
        InputHex = $"{rB:X2}{gB:X2}{bB:X2}{aB:X2}";

        // Position SV Cursor (Width=184, Height=170)
        Canvas.SetLeft(SvCursor, _s * 184f);
        Canvas.SetTop(SvCursor, (1f - _v) * 170f);

        // Position Hue Indicator (Height=170, Hue 0..360)
        Canvas.SetTop(HueIndicator, (_h / 360f) * 170f);

        // Position Alpha Indicator (Height=170, Alpha 1..0 from top to bottom)
        Canvas.SetTop(AlphaIndicator, (1f - _a) * 170f);

        if (triggerEvents)
        {
            _isInternalUpdate = true;
            SelectedColor = newCol;
            VectorColor = newVec;
            _isInternalUpdate = false;

            ColorChanged?.Invoke(this, newCol);
            VectorColorChanged?.Invoke(this, newVec);
        }
    }

    // ---- Pointer Event Handlers for 2D SV Box ----
    private void OnSvPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        _isDraggingSv = true;
        e.Pointer.Capture(SvGrid);
        UpdateSvFromPointer(e.GetPosition(SvGrid));
    }

    private void OnSvPointerMoved(object? sender, PointerEventArgs e)
    {
        if (_isDraggingSv)
        {
            UpdateSvFromPointer(e.GetPosition(SvGrid));
        }
    }

    private void OnSvPointerReleased(object? sender, PointerReleasedEventArgs e)
    {
        _isDraggingSv = false;
        e.Pointer.Capture(null);
    }

    private void OnSvPointerCaptureLost(object? sender, PointerCaptureLostEventArgs e)
    {
        _isDraggingSv = false;
    }

    private void UpdateSvFromPointer(Point pt)
    {
        double w = SvGrid.Bounds.Width > 0 ? SvGrid.Bounds.Width : 184.0;
        double h = SvGrid.Bounds.Height > 0 ? SvGrid.Bounds.Height : 170.0;
        _s = Math.Clamp((float)(pt.X / w), 0f, 1f);
        _v = Math.Clamp(1f - (float)(pt.Y / h), 0f, 1f);
        UpdateAllFromHsv();
    }

    // ---- Pointer Event Handlers for Rainbow Hue Bar ----
    private void OnHuePointerPressed(object? sender, PointerPressedEventArgs e)
    {
        _isDraggingHue = true;
        e.Pointer.Capture(HueGrid);
        UpdateHueFromPointer(e.GetPosition(HueGrid));
    }

    private void OnHuePointerMoved(object? sender, PointerEventArgs e)
    {
        if (_isDraggingHue)
        {
            UpdateHueFromPointer(e.GetPosition(HueGrid));
        }
    }

    private void OnHuePointerReleased(object? sender, PointerReleasedEventArgs e)
    {
        _isDraggingHue = false;
        e.Pointer.Capture(null);
    }

    private void OnHuePointerCaptureLost(object? sender, PointerCaptureLostEventArgs e)
    {
        _isDraggingHue = false;
    }

    private void UpdateHueFromPointer(Point pt)
    {
        double h = HueGrid.Bounds.Height > 0 ? HueGrid.Bounds.Height : 170.0;
        _h = Math.Clamp((float)(pt.Y / h) * 360f, 0f, 360f);
        UpdateAllFromHsv();
    }

    // ---- Pointer Event Handlers for Alpha Bar ----
    private void OnAlphaPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        _isDraggingAlpha = true;
        e.Pointer.Capture(AlphaGrid);
        UpdateAlphaFromPointer(e.GetPosition(AlphaGrid));
    }

    private void OnAlphaPointerMoved(object? sender, PointerEventArgs e)
    {
        if (_isDraggingAlpha)
        {
            UpdateAlphaFromPointer(e.GetPosition(AlphaGrid));
        }
    }

    private void OnAlphaPointerReleased(object? sender, PointerReleasedEventArgs e)
    {
        _isDraggingAlpha = false;
        e.Pointer.Capture(null);
    }

    private void OnAlphaPointerCaptureLost(object? sender, PointerCaptureLostEventArgs e)
    {
        _isDraggingAlpha = false;
    }

    private void UpdateAlphaFromPointer(Point pt)
    {
        double h = AlphaGrid.Bounds.Height > 0 ? AlphaGrid.Bounds.Height : 170.0;
        _a = Math.Clamp(1f - (float)(pt.Y / h), 0f, 1f);
        UpdateAllFromHsv();
    }

    // ---- Numeric Inputs Handlers ----
    private void OnInputChanged(object? sender, RoutedEventArgs e)
    {
        ParseNumericInputs();
    }

    private void OnInputKeyDown(object? sender, KeyEventArgs e)
    {
        if (e.Key == Key.Enter)
        {
            ParseNumericInputs();
        }
    }

    private void ParseNumericInputs()
    {
        if (float.TryParse(InputH, NumberStyles.Float, CultureInfo.InvariantCulture, out var h))
            _h = Math.Clamp(h, 0f, 360f);
        if (float.TryParse(InputS, NumberStyles.Float, CultureInfo.InvariantCulture, out var s))
            _s = Math.Clamp(s / 100f, 0f, 1f);
        if (float.TryParse(InputV, NumberStyles.Float, CultureInfo.InvariantCulture, out var v))
            _v = Math.Clamp(v / 100f, 0f, 1f);

        if (float.TryParse(InputR, NumberStyles.Float, CultureInfo.InvariantCulture, out var r) &&
            float.TryParse(InputG, NumberStyles.Float, CultureInfo.InvariantCulture, out var g) &&
            float.TryParse(InputB, NumberStyles.Float, CultureInfo.InvariantCulture, out var b))
        {
            var (nh, ns, nv) = RgbToHsv(r / 255f, g / 255f, b / 255f);
            _h = nh;
            _s = ns;
            _v = nv;
        }

        if (float.TryParse(InputA, NumberStyles.Float, CultureInfo.InvariantCulture, out var a))
            _a = Math.Clamp(a / 255f, 0f, 1f);

        UpdateAllFromHsv();
    }

    private void OnHexInputChanged(object? sender, RoutedEventArgs e)
    {
        ParseHexInput();
    }

    private void OnHexKeyDown(object? sender, KeyEventArgs e)
    {
        if (e.Key == Key.Enter)
        {
            ParseHexInput();
        }
    }

    private void ParseHexInput()
    {
        string hex = InputHex.Trim().TrimStart('#');
        if (hex.Length == 6)
        {
            if (uint.TryParse(hex, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var val))
            {
                byte r = (byte)((val >> 16) & 0xFF);
                byte g = (byte)((val >> 8) & 0xFF);
                byte b = (byte)(val & 0xFF);
                var (h, s, v) = RgbToHsv(r / 255f, g / 255f, b / 255f);
                _h = h; _s = s; _v = v;
                UpdateAllFromHsv();
            }
        }
        else if (hex.Length == 8)
        {
            if (uint.TryParse(hex, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var val))
            {
                byte r = (byte)((val >> 24) & 0xFF);
                byte g = (byte)((val >> 16) & 0xFF);
                byte b = (byte)((val >> 8) & 0xFF);
                byte a = (byte)(val & 0xFF);
                var (h, s, v) = RgbToHsv(r / 255f, g / 255f, b / 255f);
                _h = h; _s = s; _v = v;
                _a = a / 255f;
                UpdateAllFromHsv();
            }
        }
    }

    private void OnRevertToOriginalPressed(object? sender, PointerPressedEventArgs e)
    {
        var (h, s, v) = RgbToHsv(OriginalColor.R / 255f, OriginalColor.G / 255f, OriginalColor.B / 255f);
        _h = h;
        _s = s;
        _v = v;
        _a = OriginalColor.A / 255f;
        UpdateAllFromHsv();
    }

    private void OnPaletteClick(object? sender, RoutedEventArgs e)
    {
        if (sender is Button btn && btn.Tag is string hexStr && Color.TryParse(hexStr, out var c))
        {
            var (h, s, v) = RgbToHsv(c.R / 255f, c.G / 255f, c.B / 255f);
            _h = h;
            _s = s;
            _v = v;
            _a = c.A / 255f;
            UpdateAllFromHsv();
        }
    }

    // ---- Pure Math Color Conversions ----
    public static (float H, float S, float V) RgbToHsv(float r, float g, float b)
    {
        r = Math.Clamp(r, 0f, 1f);
        g = Math.Clamp(g, 0f, 1f);
        b = Math.Clamp(b, 0f, 1f);

        float max = Math.Max(r, Math.Max(g, b));
        float min = Math.Min(r, Math.Min(g, b));
        float delta = max - min;
        float h = 0f;

        if (delta > 1e-5f)
        {
            if (Math.Abs(max - r) < 1e-5f)
                h = (g - b) / delta % 6f;
            else if (Math.Abs(max - g) < 1e-5f)
                h = (b - r) / delta + 2f;
            else
                h = (r - g) / delta + 4f;

            h *= 60f;
            if (h < 0f) h += 360f;
        }

        float s = max > 1e-5f ? delta / max : 0f;
        float v = max;
        return (h, s, v);
    }

    public static (float R, float G, float B) HsvToRgb(float h, float s, float v)
    {
        h = (h % 360f + 360f) % 360f;
        s = Math.Clamp(s, 0f, 1f);
        v = Math.Clamp(v, 0f, 1f);

        float c = v * s;
        float x = c * (1f - Math.Abs((h / 60f) % 2f - 1f));
        float m = v - c;

        float r = 0, g = 0, b = 0;
        if (h < 60f) { r = c; g = x; b = 0; }
        else if (h < 120f) { r = x; g = c; b = 0; }
        else if (h < 180f) { r = 0; g = c; b = x; }
        else if (h < 240f) { r = 0; g = x; b = c; }
        else if (h < 300f) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }

        return (r + m, g + m, b + m);
    }

    public static (float H, float S, float L) RgbToHsl(float r, float g, float b)
    {
        r = Math.Clamp(r, 0f, 1f);
        g = Math.Clamp(g, 0f, 1f);
        b = Math.Clamp(b, 0f, 1f);

        float max = Math.Max(r, Math.Max(g, b));
        float min = Math.Min(r, Math.Min(g, b));
        float delta = max - min;
        float l = (max + min) / 2f;
        float h = 0f;

        if (delta > 1e-5f)
        {
            if (Math.Abs(max - r) < 1e-5f)
                h = (g - b) / delta % 6f;
            else if (Math.Abs(max - g) < 1e-5f)
                h = (b - r) / delta + 2f;
            else
                h = (r - g) / delta + 4f;

            h *= 60f;
            if (h < 0f) h += 360f;
        }

        float s = 0f;
        if (delta > 1e-5f && l > 1e-5f && l < (1f - 1e-5f))
        {
            s = delta / (1f - Math.Abs(2f * l - 1f));
        }

        return (h, Math.Clamp(s, 0f, 1f), Math.Clamp(l, 0f, 1f));
    }

    public static (float R, float G, float B) HslToRgb(float h, float s, float l)
    {
        h = (h % 360f + 360f) % 360f;
        s = Math.Clamp(s, 0f, 1f);
        l = Math.Clamp(l, 0f, 1f);

        float c = (1f - Math.Abs(2f * l - 1f)) * s;
        float x = c * (1f - Math.Abs((h / 60f) % 2f - 1f));
        float m = l - c / 2f;

        float r = 0, g = 0, b = 0;
        if (h < 60f) { r = c; g = x; b = 0; }
        else if (h < 120f) { r = x; g = c; b = 0; }
        else if (h < 180f) { r = 0; g = c; b = x; }
        else if (h < 240f) { r = 0; g = x; b = c; }
        else if (h < 300f) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }

        return (r + m, g + m, b + m);
    }
}
