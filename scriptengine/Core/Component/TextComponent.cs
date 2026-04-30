// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
namespace Ignite;

public class TextComponent : IComponent
{
    public string TextString
    {
        get
        {
            ComponentInternalCalls.TextComponent_GetText(Entity.ID, out string result);
            return result;
        }
        set => ComponentInternalCalls.TextComponent_SetText(Entity.ID, value);
    }

    public Vector4 Color
    {
        get
        {
            ComponentInternalCalls.TextComponent_GetColor(Entity.ID, out Vector4 result);
            return result;
        }
        set => ComponentInternalCalls.TextComponent_SetColor(Entity.ID, value);
    }

    public float Kerning
    {
        get
        {
            ComponentInternalCalls.TextComponent_GetKerning(Entity.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.TextComponent_SetKerning(Entity.ID, value);
    }

    public float LineSpacing
    {
        get
        {
            ComponentInternalCalls.TextComponent_GetLineSpacing(Entity.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.TextComponent_SetLineSpacing(Entity.ID, value);
    }
}
