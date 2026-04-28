// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core;
namespace Ignite;

public class Text : IComponent
{
    public string TextString
    {
        get
        {
            InternalCalls.TextComponent_GetText(Entity.ID, out string result);
            return result;
        }
        set => InternalCalls.TextComponent_SetText(Entity.ID, value);
    }

    public Vector4 Color
    {
        get
        {
            InternalCalls.TextComponent_GetColor(Entity.ID, out Vector4 result);
            return result;
        }
        set => InternalCalls.TextComponent_SetColor(Entity.ID, value);
    }

    public float Kerning
    {
        get
        {
            InternalCalls.TextComponent_GetKerning(Entity.ID, out float result);
            return result;
        }
        set => InternalCalls.TextComponent_SetKerning(Entity.ID, value);
    }

    public float LineSpacing
    {
        get
        {
            InternalCalls.TextComponent_GetLineSpacing(Entity.ID, out float result);
            return result;
        }
        set => InternalCalls.TextComponent_SetLineSpacing(Entity.ID, value);
    }
}
