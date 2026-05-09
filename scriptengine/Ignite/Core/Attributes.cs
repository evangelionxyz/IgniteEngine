// Copyright (c) 2026 Evangelion Manuhutu

using System;

namespace Ignite;

// Serialize field to disk
[AttributeUsage(AttributeTargets.Field, Inherited = true, AllowMultiple = true)]
public sealed class SerializeField : Attribute
{
}

// Engine UI - marks a ScriptableObject subclass so the content browser
// can generate "Create > Scriptable Object > <menuName>" context menu items.
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
