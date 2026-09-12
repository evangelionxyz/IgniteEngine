namespace IgniteEditor.ViewModels;

/// <summary>
/// Mirrors C++ EditorCamera::NavigationMode.
/// </summary>
public enum CameraNavigationMode
{
    Orbit = 0,
    Fly   = 1,
    Mode2D = 2
}

/// <summary>
/// Enum mirroring the C++ GizmoOperation in states.hpp
/// </summary>
public enum GizmoOperation
{
    None = -1,
    Translate = 0,
    Rotate = 1,
    Scale = 2,
    BoundSizing2D = 3
}

/// <summary>
/// Represents the current editor play state.
/// </summary>
public enum PlayModeState
{
    Stopped,
    Playing,
    Paused,
    Simulating
}

