using System;
using System.Globalization;
using Avalonia.Data.Converters;
using IgniteEditor.ViewModels;

namespace IgniteEditor.Converters;

/// <summary>
/// Converts <see cref="CameraNavigationMode"/> enum values to display strings
/// matching the old editor's Orbit / Flying / 2D dropdown labels.
/// </summary>
public class CameraNavigationModeConverter : IValueConverter
{
    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        return value switch
        {
            CameraNavigationMode.Orbit  => "Orbit",
            CameraNavigationMode.Fly    => "Flying",
            CameraNavigationMode.Mode2D => "2D",
            _                           => value?.ToString()
        };
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => throw new NotImplementedException();
}
