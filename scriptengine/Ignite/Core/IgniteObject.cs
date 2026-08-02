using System;

namespace Ignite;

/// <summary>Base Class for all of Ignite Game Object</summary>
public abstract class IgniteObject : IEquatable<IgniteObject>
{
    /// <summary>The AssetHandle UUID that uniquely identifies this asset.</summary>
    public ulong ID { get; protected set; }

    protected IgniteObject() { }
    protected IgniteObject(ulong id) { ID = id; }

    internal void SetID(ulong id) { ID = id; }

    // Lifecycle callbacks - override in subclasses
    public virtual void OnCreate() { }
    public virtual void OnUpdate(float deltaTime) { }
    public virtual void OnDestroy() { }
    public virtual void OnFixedUpdate() { }
    public virtual void OnHotReload() { }

    // -------------------------------------------------------------------------
    // Hot-reload helpers: called as instance methods via MochiSharp InvokeMethod
    // so the dispatch goes through GCHandle.FromIntPtr (correct) rather than
    // through InvokeStaticMethod with a raw void* handle (broken marshaling).
    // -------------------------------------------------------------------------

    /// <summary>
    /// Returns the IDs of all items in a List&lt;Entity&gt; field as a
    /// pipe-separated string of ulong values, or an empty string if the
    /// field is null / empty.
    /// </summary>
    internal string GetEntityListFieldIds(string fieldName)
    {
        if (string.IsNullOrEmpty(fieldName))
            return string.Empty;

        // Walk the inheritance chain so fields declared on a subclass are found.
        var type = GetType();
        System.Reflection.FieldInfo? field = null;
        while (type != null && field == null)
        {
            field = type.GetField(fieldName,
                System.Reflection.BindingFlags.Instance |
                System.Reflection.BindingFlags.Public |
                System.Reflection.BindingFlags.NonPublic |
                System.Reflection.BindingFlags.DeclaredOnly);
            type = type.BaseType;
        }

        if (field == null)
            return string.Empty;

        if (field.GetValue(this) is not System.Collections.IEnumerable list)
            return string.Empty;

        var sb = new System.Text.StringBuilder();
        foreach (var item in list)
        {
            if (item is IgniteObject obj)
            {
                if (sb.Length > 0) sb.Append('|');
                sb.Append(obj.ID);
            }
        }
        return sb.ToString();
    }

    /// <summary>
    /// Rebuilds a List&lt;Entity&gt; field on this instance from a pipe-separated
    /// string of ulong entity IDs.
    /// </summary>
    internal void SetEntityListField(string fieldName, string ids)
    {
        if (string.IsNullOrEmpty(fieldName))
            return;

        var type = GetType();
        System.Reflection.FieldInfo? field = null;
        while (type != null && field == null)
        {
            field = type.GetField(fieldName,
                System.Reflection.BindingFlags.Instance |
                System.Reflection.BindingFlags.Public |
                System.Reflection.BindingFlags.NonPublic |
                System.Reflection.BindingFlags.DeclaredOnly);
            type = type.BaseType;
        }

        if (field == null)
            return;

        // Determine the declared element type so we can reconstruct the list correctly.
        // For List<Entity> the element type is Entity; for List<SomeScript> it is that subclass.
        var fieldType = field.FieldType;
        Type elemType = typeof(Entity);
        if (fieldType.IsGenericType)
            elemType = fieldType.GetGenericArguments()[0];

        var listType = typeof(System.Collections.Generic.List<>).MakeGenericType(elemType);
        var list = (System.Collections.IList)Activator.CreateInstance(listType)!;

        if (!string.IsNullOrEmpty(ids))
        {
            foreach (var part in ids.Split('|', StringSplitOptions.RemoveEmptyEntries))
            {
                if (ulong.TryParse(part, out ulong id) && id != 0)
                    list.Add(new Entity(id));
            }
        }

        field.SetValue(this, list);
    }


    // Equatable Funcs
    public override bool Equals(object? obj) => obj is IgniteObject so && Equals(so);
    public override int GetHashCode() => ID.GetHashCode();
    public override string ToString() => $"{GetType().Name} [{ID}]";

    public bool Equals(IgniteObject? other)
    {
        if (other is null)
            return false;

        if (ReferenceEquals(this, other))
            return true;

        return ID == other.ID;
    }

    public static bool operator ==(IgniteObject? left, IgniteObject? right)
    {
        if (ReferenceEquals(left, right))
            return true;

        if (left is null || right is null)
            return false;

        return left.ID == right.ID;
    }

    public static bool operator !=(IgniteObject? left, IgniteObject? right) => !(left == right);
    public static implicit operator ulong(IgniteObject? so) => so?.ID ?? 0UL;
}
