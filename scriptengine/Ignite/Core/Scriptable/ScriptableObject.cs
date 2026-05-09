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
public abstract class ScriptableObject : IEquatable<ScriptableObject>
{
    /// <summary>The AssetHandle UUID that uniquely identifies this asset.</summary>
    public ulong ID { get; protected set; }

    protected ScriptableObject() { }

    /// <summary>Constructor used by the MochiSharp host to reconstruct a reference from an AssetHandle ID.</summary>
    protected ScriptableObject(ulong id) { ID = id; }

    internal void SetID(ulong id) { ID = id; }

    // Lifecycle callbacks - override in subclasses
    public virtual void OnCreate() { }
    public virtual void OnUpdate(float deltaTime) { }
    public virtual void OnDestroy() { }

    public override bool Equals(object obj) => obj is ScriptableObject so && Equals(so);
    public override int GetHashCode() => ID.GetHashCode();
    public override string ToString() => $"{GetType().Name} [{ID}]";

    public bool Equals(ScriptableObject other)
    {
        if (other is null)
            return false;

        if (ReferenceEquals(this, other))
            return true;

        return ID == other.ID;
    }

    public static bool operator ==(ScriptableObject left, ScriptableObject right)
    {
        if (ReferenceEquals(left, right))
            return true;

        if (left is null || right is null)
            return false;

        return left.ID == right.ID;
    }

    public static bool operator !=(ScriptableObject left, ScriptableObject right) => !(left == right);
    public static implicit operator ulong(ScriptableObject so) => so?.ID ?? 0UL;

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
