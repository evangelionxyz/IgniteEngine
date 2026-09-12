using System;
using System.Globalization;
using Avalonia.Data.Converters;
using Avalonia.Media;
using IgniteEditor.Models;

namespace IgniteEditor.Converters;

public class LogLevelToBrushConverter : IValueConverter
{
    public static readonly LogLevelToBrushConverter Instance = new();

    private static readonly IBrush TraceBrush = new SolidColorBrush(Color.Parse("#CCCCCC"));
    private static readonly IBrush DebugBrush = new SolidColorBrush(Color.Parse("#33CCCC"));
    private static readonly IBrush InfoBrush = new SolidColorBrush(Color.Parse("#33CC33"));
    private static readonly IBrush WarningBrush = new SolidColorBrush(Color.Parse("#CCCC33"));
    private static readonly IBrush ErrorBrush = new SolidColorBrush(Color.Parse("#CC3333"));
    private static readonly IBrush CriticalBrush = new SolidColorBrush(Color.Parse("#FF0000"));

    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is LogLevel level)
        {
            return level switch
            {
                LogLevel.Trace => TraceBrush,
                LogLevel.Debug => DebugBrush,
                LogLevel.Info => InfoBrush,
                LogLevel.Warning => WarningBrush,
                LogLevel.Error => ErrorBrush,
                LogLevel.Critical => CriticalBrush,
                _ => TraceBrush
            };
        }
        return TraceBrush;
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => throw new NotImplementedException();
}
