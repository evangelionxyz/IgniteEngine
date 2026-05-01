using System;

namespace Ignite;

public abstract class ScriptableObject(ulong id) : IEquatable<ScriptableObject>
{
    public ulong ID { get; protected set; } = id;

    internal void SetID(ulong id) { ID = id; }

    public virtual void OnCreate() { }
    public virtual void OnUpdate(float deltaTime) { }
    public virtual void OnDestroy() { }

    public override bool Equals(object obj) => obj is ScriptableObject so && Equals(so);
    public override int GetHashCode() => ID.GetHashCode();
    public override string ToString() => $"{typeof(ScriptableObject)} {ID}";

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
    public static implicit operator ulong(ScriptableObject entity) => entity?.ID ?? 0UL;
}
