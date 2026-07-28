// Copyright (c) 2026 Evangelion Manuhutu

namespace Ignite;

public static partial class Mathf
{
    public struct Vector4
    {
        public float X, Y, Z, W;

        public Vector4(float scalar)
        {
            X = scalar;
            Y = scalar;
            Z = scalar;
            W = scalar;
        }

        public Vector4(float x, float y, float z, float w)
        {
            X = x;
            Y = y;
            Z = z;
            W = w;
        }

        public Vector4(Vector4 vector)
        {
            X = vector.X;
            Y = vector.Y;
            Z = vector.Z;
            W = vector.W;
        }

        public static Vector4 Zero => new Vector4(0.0f);

        public Vector4(Vector2 vector, float z, float w)
        {
            X = vector.X;
            Y = vector.Y;
            Z = z;
            W = w;
        }

        public float Length() => Sqrt(X * X + Y * Y + Z * Z + W * W);
        public float LengthSquared() => X * X + Y * Y + Z * Z + W * W;

        public float Magnitude() => Length();
        public float SqrMagnitude() => LengthSquared();

        public float magnitude => Length();
        public float sqrMagnitude => LengthSquared();
        public float length => Length();
        public float lengthSquared => LengthSquared();

        public Vector4 Normalized()
        {
            float len = Length();
            if (len > 0.0f)
            {
                return new Vector4(X / len, Y / len, Z / len, W / len);
            }
            return Zero;
        }

        public Vector4 normalized => Normalized();

        public void Normalize()
        {
            float len = Length();
            if (len > 0.0f)
            {
                X /= len;
                Y /= len;
                Z /= len;
                W /= len;
            }
            else
            {
                X = Y = Z = W = 0.0f;
            }
        }

        public static float Dot(Vector4 a, Vector4 b) => a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;

        public static float Distance(Vector4 a, Vector4 b) => (a - b).Length();
        public static float DistanceSquared(Vector4 a, Vector4 b) => (a - b).LengthSquared();

        public static Vector4 Lerp(Vector4 a, Vector4 b, float t)
        {
            return new Vector4(Mathf.Lerp(a.X, b.X, t), Mathf.Lerp(a.Y, b.Y, t), Mathf.Lerp(a.Z, b.Z, t), Mathf.Lerp(a.W, b.W, t));
        }

        public Vector4(Vector3 vector, float w)
        {
            X = vector.X;
            Y = vector.Y;
            Z = vector.Z;
            W = w;
        }

        public static Vector4 operator +(Vector4 vectorA, Vector4 vectorB)
        {
            return new Vector4(vectorA.X + vectorB.X, vectorA.Y + vectorB.Y, vectorA.Z + vectorB.Z, vectorA.W + vectorB.W);
        }

        public static Vector4 operator -(Vector4 vectorA, Vector4 vectorB)
        {
            return new Vector4(vectorA.X - vectorB.X, vectorA.Y - vectorB.Y, vectorA.Z - vectorB.Z, vectorA.W - vectorB.W);
        }

        public static Vector4 operator *(Vector4 vectorA, float scalar)
        {
            return new Vector4(vectorA.X * scalar, vectorA.Y * scalar, vectorA.Z * scalar, vectorA.W * scalar);
        }

        public static Vector4 operator /(Vector4 vectorA, float scalar)
        {
            return new Vector4(vectorA.X / scalar, vectorA.Y / scalar, vectorA.Z / scalar, vectorA.W / scalar);
        }
    }
}
