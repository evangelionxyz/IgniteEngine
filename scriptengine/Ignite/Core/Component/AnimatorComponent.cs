// Copyright (c) 2026 Evangelion Manuhutu

using System;
using Ignite.Core.Component;

namespace Ignite;

public class AnimatorComponent : IComponent
{
    public void SetFloat(string paramName, float value)
    {
        if (Entity == null || string.IsNullOrEmpty(paramName)) return;
        ComponentInternalCalls.AnimatorComponent_SetFloat(Entity.ID, paramName, value);
    }

    public float GetFloat(string paramName)
    {
        if (Entity == null || string.IsNullOrEmpty(paramName)) return 0.0f;
        ComponentInternalCalls.AnimatorComponent_GetFloat(Entity.ID, paramName, out float result);
        return result;
    }

    public void SetInt(string paramName, int value)
    {
        if (Entity == null || string.IsNullOrEmpty(paramName)) return;
        ComponentInternalCalls.AnimatorComponent_SetInt(Entity.ID, paramName, value);
    }

    public int GetInt(string paramName)
    {
        if (Entity == null || string.IsNullOrEmpty(paramName)) return 0;
        ComponentInternalCalls.AnimatorComponent_GetInt(Entity.ID, paramName, out int result);
        return result;
    }

    public void SetBool(string paramName, bool value)
    {
        if (Entity == null || string.IsNullOrEmpty(paramName)) return;
        ComponentInternalCalls.AnimatorComponent_SetBool(Entity.ID, paramName, value);
    }

    public bool GetBool(string paramName)
    {
        if (Entity == null || string.IsNullOrEmpty(paramName)) return false;
        ComponentInternalCalls.AnimatorComponent_GetBool(Entity.ID, paramName, out bool result);
        return result;
    }

    public void SetString(string paramName, string value)
    {
        if (Entity == null || string.IsNullOrEmpty(paramName)) return;
        ComponentInternalCalls.AnimatorComponent_SetString(Entity.ID, paramName, value ?? string.Empty);
    }

    public string GetString(string paramName)
    {
        if (Entity == null || string.IsNullOrEmpty(paramName)) return string.Empty;
        return ComponentInternalCalls.AnimatorComponent_GetString(Entity.ID, paramName);
    }

    public void SetState(string stateName)
    {
        if (Entity == null || string.IsNullOrEmpty(stateName)) return;
        ComponentInternalCalls.AnimatorComponent_SetState(Entity.ID, stateName);
    }

    public string CurrentStateName
    {
        get
        {
            if (Entity == null) return string.Empty;
            return ComponentInternalCalls.AnimatorComponent_GetCurrentStateName(Entity.ID);
        }
    }
}
