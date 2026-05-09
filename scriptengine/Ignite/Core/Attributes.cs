// Copyright (c) 2026 Evangelion Manuhutu

using System;

namespace Ignite;

// Serialize field to disk
[AttributeUsage(AttributeTargets.Field, Inherited = true, AllowMultiple = true)]
public sealed class SerializeFieldAttribute : Attribute
{
}

// Engine UI
[AttributeUsage(AttributeTargets.Class, Inherited = true, AllowMultiple = false)]
public sealed class CreateAssetMenu : Attribute
{
}
