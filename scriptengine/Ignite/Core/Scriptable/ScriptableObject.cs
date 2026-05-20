// Copyright (c) 2026 Evangelion Manuhutu

using System;
using Ignite.Core;

namespace Ignite;

/// <summary>
/// Base class for all Scriptable Objects.
/// Subclasses are discovered by the engine via reflection and can be
/// decorated with [CreateAssetMenu] to appear in the content browser
/// context menu.
///
/// Usage example:
/// <code>
/// [CreateAssetMenu(FileName = "WeaponData", MenuName = "Game/Weapon Data")]
/// public class WeaponData : ScriptableObject
/// {
///     [SerializeField] public float Damage = 10f;
///     [SerializeField] public string WeaponName = "Sword";
/// }
/// </code>
/// </summary>
public abstract class ScriptableObject : IgniteObject
{
    protected ScriptableObject() { }

    /// <summary>Constructor used by the MochiSharp host to reconstruct a reference from an AssetHandle ID.</summary>
    protected ScriptableObject(ulong id): base(id) { }

    // ---- Runtime field access via native bridge ----
    /// <summary>
    /// Read a float field value from the native .ixso asset by field name.
    /// Falls back to reflection on the managed instance if the native bridge is unavailable.
    /// </summary>
    protected float GetField(string fieldName, float defaultValue = 0f)
    {
        if (!CoreInternalCalls.HasScriptableObjectBridge) return defaultValue;
        return CoreInternalCalls.ScriptableObject_GetFieldFloat(ID, fieldName);
    }

    protected int GetFieldInt(string fieldName, int defaultValue = 0)
    {
        if (!CoreInternalCalls.HasScriptableObjectBridge) return defaultValue;
        return CoreInternalCalls.ScriptableObject_GetFieldInt(ID, fieldName);
    }

    protected bool GetFieldBool(string fieldName, bool defaultValue = false)
    {
        if (!CoreInternalCalls.HasScriptableObjectBridge) return defaultValue;
        return CoreInternalCalls.ScriptableObject_GetFieldBool(ID, fieldName);
    }

    protected string GetFieldString(string fieldName, string defaultValue = "")
    {
        if (!CoreInternalCalls.HasScriptableObjectBridge) return defaultValue;
        return CoreInternalCalls.ScriptableObject_GetFieldString(ID, fieldName);
    }
}
