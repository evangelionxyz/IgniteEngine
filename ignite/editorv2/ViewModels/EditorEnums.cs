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
/// Represents the coordinate space mode for gizmos.
/// </summary>
public enum GizmoMode
{
    Local = 0,
    World = 1
}

/// <summary>
/// Extension methods for GizmoOperation.
/// </summary>
public static class GizmoOperationExtensions
{
    /// <summary>
    /// Cycles through the primary gizmo operations: Translate -> Rotate -> Scale -> Translate.
    /// </summary>
    public static GizmoOperation Cycle(this GizmoOperation op)
    {
        return op switch
        {
            GizmoOperation.Translate => GizmoOperation.Rotate,
            GizmoOperation.Rotate => GizmoOperation.Scale,
            GizmoOperation.Scale => GizmoOperation.Translate,
            _ => GizmoOperation.Translate
        };
    }
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


