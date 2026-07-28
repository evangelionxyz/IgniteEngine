// Copyright (c) 2026 Evangelion Manuhutu

namespace Ignite;

public static partial class Mathf
{
    public struct Vector3
    {
        public float X, Y, Z;

        public Vector3(float scalar)
        {
            X = scalar;
            Y = scalar;
            Z = scalar;
        }

        public Vector3(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }

        public Vector3(Vector3 vector)
        {
            X = vector.X;
            Y = vector.Y;
            Z = vector.Z;
        }

        public Vector3(Vector4 vector)
        {
            X = vector.X;
            Y = vector.Y;
            Z = vector.Z;
        }

        public Vector3(Vector2 vector, float z)
        {
            X = vector.X;
            Y = vector.Y;
            Z = z;
        }

        public Vector2 XY
        {
            get => new Vector2(X, Y);
            set
            {
                X = value.X;
                Y = value.Y;
            }
        }

        public static Vector3 Zero => new Vector3(0.0f);

        public float Length() => Sqrt(X * X + Y * Y + Z * Z);
        public float LengthSquared() => X * X + Y * Y + Z * Z;

        public float Magnitude() => Length();
        public float SqrMagnitude() => LengthSquared();

        public float magnitude => Length();
        public float sqrMagnitude => LengthSquared();
        public float length => Length();
        public float lengthSquared => LengthSquared();

        public Vector3 Normalized()
        {
            float len = Length();
            if (len > 0.0f)
            {
                return new Vector3(X / len, Y / len, Z / len);
            }
            return Zero;
        }

        public Vector3 normalized => Normalized();

        public void Normalize()
        {
            float len = Length();
            if (len > 0.0f)
            {
                X /= len;
                Y /= len;
                Z /= len;
            }
            else
            {
                X = Y = Z = 0.0f;
            }
        }

        public static float Dot(Vector3 a, Vector3 b) => a.X * b.X + a.Y * b.Y + a.Z * b.Z;

        public static Vector3 Cross(Vector3 a, Vector3 b)
        {
            return new Vector3(
                a.Y * b.Z - a.Z * b.Y,
                a.Z * b.X - a.X * b.Z,
                a.X * b.Y - a.Y * b.X
            );
        }

        public static float Distance(Vector3 a, Vector3 b) => (a - b).Length();
        public static float DistanceSquared(Vector3 a, Vector3 b) => (a - b).LengthSquared();

        public static Vector3 ClampMagnitude(Vector3 vector, float maxLength)
        {
            float sqrLen = vector.LengthSquared();
            if (sqrLen > maxLength * maxLength)
            {
                float len = Sqrt(sqrLen);
                return new Vector3(vector.X / len * maxLength, vector.Y / len * maxLength, vector.Z / len * maxLength);
            }
            return vector;
        }

        public static Vector3 Lerp(Vector3 a, Vector3 b, float t)
        {
            return new Vector3(Mathf.Lerp(a.X, b.X, t), Mathf.Lerp(a.Y, b.Y, t), Mathf.Lerp(a.Z, b.Z, t));
        }

        public static Vector3 operator *(Vector3 a, Vector3 b)
        {
            return new Vector3(a.X * b.X, a.Y * b.Y, a.Z * b.Z);
        }

        public static Vector3 operator *(Vector3 vector, float scalar)
        {
            return new Vector3(vector.X * scalar, vector.Y * scalar, vector.Z * scalar);
        }
        public static Vector3 operator *(float scalar, Vector3 vector)
        {
            return new Vector3(vector.X * scalar, vector.Y * scalar, vector.Z * scalar);
        }

        public static Vector3 operator /(Vector3 a, Vector3 b)
        {
            return new Vector3(a.X / b.X, a.Y / b.Y, a.Z / b.Z);
        }

        public static Vector3 operator /(Vector3 vector, float scalar)
        {
            return new Vector3(vector.X / scalar, vector.Y / scalar, vector.Z / scalar);
        }

        public static Vector3 operator /(float scalar, Vector3 vector)
        {
            return new Vector3(vector.X / scalar, vector.Y / scalar, vector.Z / scalar);
        }

        public static Vector3 operator +(Vector3 vectorA, Vector3 vectorB)
        {
            return new Vector3(vectorA.X + vectorB.X, vectorA.Y + vectorB.Y, vectorA.Z + vectorB.Z);
        }

        public static Vector3 operator -(Vector3 vectorA)
        {
            return new Vector3(-vectorA.X, -vectorA.Y, -vectorA.Z);
        }

        public static Vector3 operator -(Vector3 vectorA, Vector3 vectorB)
        {
            return new Vector3(vectorA.X - vectorB.X, vectorA.Y - vectorB.Y, vectorA.Z - vectorB.Z);
        }

        public static Vector3 Min(Vector3 a, Vector3 b)
        {
            return new Vector3(Mathf.Min(a.X, b.X), Mathf.Min(a.Y, b.Y), Mathf.Min(a.Z, b.Z));
        }

        public static Vector3 Max(Vector3 a, Vector3 b)
        {
            return new Vector3(Mathf.Max(a.X, b.X), Mathf.Max(a.Y, b.Y), Mathf.Max(a.Z, b.Z));
        }
    }

}
