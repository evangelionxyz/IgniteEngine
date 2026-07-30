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
