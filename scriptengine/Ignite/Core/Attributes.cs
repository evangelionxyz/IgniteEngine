// Copyright (c) 2026 Evangelion Manuhutu

using System;
using System.Linq.Expressions;

namespace Ignite;

/// <summary>
/// Serialize field to disk
/// </summary>
[AttributeUsage(AttributeTargets.Field, Inherited = true, AllowMultiple = true)]
public sealed class SerializeField : Attribute
{
}

/// <summary>
/// Editor UI - Use Slider
/// </summary>
[AttributeUsage(AttributeTargets.Field, Inherited = true, AllowMultiple = false)]
public sealed class UISlider : Attribute
{
    public float MinValue { get; set; }
    public float MaxValue { get; set; }

    public UISlider(float minValue = 0.0f, float maxValue = 1.0f)
    {
        MinValue = minValue;
        MaxValue = maxValue;
    }
}

/// <sumarry>
/// Engine UI - marks a ScriptableObject subclass so the content browser
/// can generate "Create > Scriptable Object > <menuName>" context menu items.
/// </sumarry>
[AttributeUsage(AttributeTargets.Class, Inherited = false, AllowMultiple = false)]
public sealed class CreateAssetMenu : Attribute
{
    /// <summary>Default file name (without extension) used when creating the asset.</summary>
    public string FileName { get; set; } = "";

    /// <summary>
    /// Forward-slash-separated menu path shown under "Create > Scriptable Object".
    /// Example: "Weapons/Sword Config"
    /// </summary>
    public string MenuName { get; set; } = "";
}
