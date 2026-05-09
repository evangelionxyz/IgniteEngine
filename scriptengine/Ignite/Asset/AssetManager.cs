// Copyright (c) 2026 Evangelion Manuhutu

using System;
using Ignite.Core;

namespace Ignite;

public readonly struct AssetHandle : IEquatable<AssetHandle>
{
    public AssetHandle(ulong value)
    {
        Value = value;
    }

    public ulong Value { get; }
    public bool IsValid => Value != 0;

    public bool Equals(AssetHandle other) => Value == other.Value;
    public override bool Equals(object obj) => obj is AssetHandle other && Equals(other);
    public override int GetHashCode() => Value.GetHashCode();
    public override string ToString() => Value.ToString();

    public static bool operator ==(AssetHandle left, AssetHandle right) => left.Equals(right);
    public static bool operator !=(AssetHandle left, AssetHandle right) => !left.Equals(right);

    public static implicit operator ulong(AssetHandle handle) => handle.Value;
    public static explicit operator AssetHandle(ulong value) => new(value);
}

public static class AssetManager
{
    public static bool IsValid(AssetHandle handle) => CoreInternalCalls.AssetManager_IsAssetHandleValid(handle.Value);
    public static bool IsLoaded(AssetHandle handle) => CoreInternalCalls.AssetManager_IsAssetLoaded(handle.Value);

    public static AssetHandle LoadAsyncFromFile(string filename) => CoreInternalCalls.AssetManager_LoadAssetAsyncFromFile(filename);
    public static AssetHandle LoadImmedateFromFile(string filename) => CoreInternalCalls.AssetManager_LoadAssetImmedateFromFile(filename);

    public static void LoadAsync(AssetHandle handle)
    {
        if (!handle.IsValid) return;
        CoreInternalCalls.AssetManager_LoadAssetAsync(handle.Value);
    }

    public static void LoadImmediate(AssetHandle handle)
    {
        if (!handle.IsValid) return;
        CoreInternalCalls.AssetManager_LoadAssetImmediate(handle.Value);
    }
}
