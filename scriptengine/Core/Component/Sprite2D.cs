namespace Ignite;

public class Sprite2D : IComponent
{
    public Vector4 Color
    {
        get
        {
            InternalCalls.Sprite2DComponent_GetColor(Entity.ID, out Vector4 result);
            return result;
        }

        set
        {
            InternalCalls.Sprite2DComponent_SetColor(Entity.ID, value);
        }
    }

    public Vector2 TilingFactor
    {
        get
        {
            InternalCalls.Sprite2DComponent_GetTilingFactor(Entity.ID, out Vector2 result);
            return result;
        }

        set
        {
            InternalCalls.Sprite2DComponent_SetTilingFactor(Entity.ID, value);
        }
    }
}
