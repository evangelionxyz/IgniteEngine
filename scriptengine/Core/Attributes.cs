// Copyright (c) 2026 Evangelion Manuhutu

using System;

namespace Ignite;

[AttributeUsage(AttributeTargets.Field, Inherited = true, AllowMultiple = true)]
public sealed class SerializeFieldAttribute : Attribute
{
}
