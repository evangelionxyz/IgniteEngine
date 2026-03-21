namespace Ignite;

public enum CursorMode
{
    Normal = 0,
    Hidden = 1,
    Disabled = 2,
    Captured = 3
}

public enum MouseCode : byte
{
    Left = 1,
    Middle = 2,
    Right = 3
}

public enum KeyCode : uint
{
    W = 0x00000077u,
    A = 0x00000061u,
    S = 0x00000073u,
    D = 0x00000064u,
    Q = 0x00000071u,
    E = 0x00000065u,
    Space = 0x00000020u,
    Escape = 0x0000001bu,
    LeftShift = 0x400000e1u,
    RightShift = 0x400000e5u,
    LeftControl = 0x400000e0u,
    RightControl = 0x400000e4u,
    Up = 0x40000052u,
    Down = 0x40000051u,
    Left = 0x40000050u,
    Right = 0x4000004fu
}

public static class Input
{
    public static bool IsKeyPressed(KeyCode keyCode) => InternalCalls.Input_IsKeyPressed((uint)keyCode);
    public static bool IsKeyPressed(uint keyCode) => InternalCalls.Input_IsKeyPressed(keyCode);

    public static bool IsModifierPressed(ushort modCode) => InternalCalls.Input_IsModifierPressed(modCode);

    public static bool IsMouseButtonPressed(MouseCode button) => InternalCalls.Input_IsMouseButtonPressed((byte)button);

    public static Vector2 MousePosition
    {
        get
        {
            InternalCalls.Input_GetMousePosition(out Vector2 result);
            return result;
        }
    }

    public static void SetMouseToCenter() => InternalCalls.Input_SetMouseToCenter();
    public static void SetCursorMode(CursorMode mode) => InternalCalls.Input_SetCursorMode((int)mode);
}
