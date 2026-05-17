// Copyright (c) 2026 Evangelion Manuhutu

using System;
using System.Runtime.InteropServices;

namespace Ignite.Core;
public static class NativeObject
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2
    {
        public float X;
        public float Y;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3
    {
        public float X;
        public float Y;
        public float Z;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector4
    {
        public float X;
        public float Y;
        public float Z;
        public float W;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Quaternion
    {
        public float X;
        public float Y;
        public float Z;
        public float W;
    }

    public static Vector2 ToNative(Mathf.Vector2 value) => new() { X = value.X, Y = value.Y };
    public static Vector3 ToNative(Mathf.Vector3 value) => new() { X = value.X, Y = value.Y, Z = value.Z };
    public static Vector4 ToNative(Mathf.Vector4 value) => new() { X = value.X, Y = value.Y, Z = value.Z, W = value.W };

    public static Mathf.Vector2 ToManaged(Vector2 value) => new(value.X, value.Y);
    public static Mathf.Vector3 ToManaged(Vector3 value) => new(value.X, value.Y, value.Z);
    public static Mathf.Vector4 ToManaged(Vector4 value) => new(value.X, value.Y, value.Z, value.W);

    public static Quaternion ToNative(Mathf.Quaternion value) => new() { X = value.X, Y = value.Y, Z = value.Z, W = value.W };
    public static Mathf.Quaternion ToManaged(Quaternion value) => new(value.X, value.Y, value.Z, value.W);

    public static IntPtr StringToUtf8(string value) => Marshal.StringToCoTaskMemUTF8(value ?? string.Empty);
    public static string? Utf8ToString(IntPtr ptr) => ptr == IntPtr.Zero ? null : Marshal.PtrToStringUTF8(ptr);
}
