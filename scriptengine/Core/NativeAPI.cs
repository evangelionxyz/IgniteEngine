// Copyright (c) 2026 Evangelion Manuhutu

using System;
using System.Runtime.InteropServices;

namespace Ignite.Core;
public static class NativeAPI
{
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeVector2
    {
        public float X;
        public float Y;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeVector3
    {
        public float X;
        public float Y;
        public float Z;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeVector4
    {
        public float X;
        public float Y;
        public float Z;
        public float W;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeQuaternion
    {
        public float X;
        public float Y;
        public float Z;
        public float W;
    }

    public static NativeAPI.NativeVector2 ToNative(Vector2 value) => new NativeAPI.NativeVector2 { X = value.X, Y = value.Y };
    public static NativeAPI.NativeVector3 ToNative(Vector3 value) => new NativeAPI.NativeVector3 { X = value.X, Y = value.Y, Z = value.Z };
    public static NativeAPI.NativeVector4 ToNative(Vector4 value) => new NativeAPI.NativeVector4 { X = value.X, Y = value.Y, Z = value.Z, W = value.W };

    public static Vector2 ToManaged(NativeAPI.NativeVector2 value) => new Vector2(value.X, value.Y);
    public static Vector3 ToManaged(NativeAPI.NativeVector3 value) => new Vector3(value.X, value.Y, value.Z);
    public static Vector4 ToManaged(NativeAPI.NativeVector4 value) => new Vector4(value.X, value.Y, value.Z, value.W);

    public static NativeAPI.NativeQuaternion ToNative(Quaternion value) => new NativeAPI.NativeQuaternion { X = value.X, Y = value.Y, Z = value.Z, W = value.W };
    public static Quaternion ToManaged(NativeAPI.NativeQuaternion value) => new Quaternion(value.X, value.Y, value.Z, value.W);

    public static IntPtr StringToUtf8(string value) => Marshal.StringToCoTaskMemUTF8(value ?? string.Empty);
}
