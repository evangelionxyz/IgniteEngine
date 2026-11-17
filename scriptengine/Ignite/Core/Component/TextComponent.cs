// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public class TextComponent : IComponent
{
    public string TextString
    {
        get
        {
            ComponentInternalCalls.TextComponent_GetText(Entity!.ID, out string result);
            return result;
        }
        set => ComponentInternalCalls.TextComponent_SetText(Entity!.ID, value);
    }

    public Vector4 Color
    {
        get
        {
            ComponentInternalCalls.TextComponent_GetColor(Entity!.ID, out Vector4 result);
            return result;
        }
        set => ComponentInternalCalls.TextComponent_SetColor(Entity!.ID, value);
    }

    public float Kerning
    {
        get
        {
            ComponentInternalCalls.TextComponent_GetKerning(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.TextComponent_SetKerning(Entity!.ID, value);
    }

    public float LineSpacing
    {
        get
        {
            ComponentInternalCalls.TextComponent_GetLineSpacing(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.TextComponent_SetLineSpacing(Entity!.ID, value);
    }

    public float FontSize
    {
        get => ComponentInternalCalls.TextComponent_GetFontSize(Entity!.ID);
        set => ComponentInternalCalls.TextComponent_SetFontSize(Entity!.ID, value);
    }

    public bool ScreenSpace
    {
        get => ComponentInternalCalls.TextComponent_GetScreenSpace(Entity!.ID);
        set => ComponentInternalCalls.TextComponent_SetScreenSpace(Entity!.ID, value);
    }

    public bool Billboard
    {
        get => ComponentInternalCalls.TextComponent_GetBillboard(Entity!.ID);
        set => ComponentInternalCalls.TextComponent_SetBillboard(Entity!.ID, value);
    }

    public bool CenterAlign
    {
        get => ComponentInternalCalls.TextComponent_GetCenterAlign(Entity!.ID);
        set => ComponentInternalCalls.TextComponent_SetCenterAlign(Entity!.ID, value);
    }
}
