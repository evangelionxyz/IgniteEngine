// Copyright (c) 2026 Evangelion Manuhutu

namespace Ignite;

public static partial class Mathf
{
    public struct Vector2
    {
        public float X, Y;

        public Vector2(float scalar)
        {
            X = scalar;
            Y = scalar;
        }

        public Vector2(float x, float y)
        {
            X = x;
            Y = y;
        }

        public Vector2(Vector3 vector)
        {
            X = vector.X;
            Y = vector.Y;
        }

        public Vector2(Vector4 vector)
        {
            X = vector.X;
            Y = vector.Y;
        }

        public static Vector2 Zero => new Vector2(0.0f);

        public float Length() => Sqrt(X * X + Y * Y);
        public float LengthSquared() => X * X + Y * Y;

        public float Magnitude() => Length();
        public float SqrMagnitude() => LengthSquared();

        public float magnitude => Length();
        public float sqrMagnitude => LengthSquared();
        public float length => Length();
        public float lengthSquared => LengthSquared();

        public Vector2 Normalized()
        {
            float len = Length();
            if (len > 0.0f)
            {
                return new Vector2(X / len, Y / len);
            }
            return Zero;
        }

        public Vector2 normalized => Normalized();

        public void Normalize()
        {
            float len = Length();
            if (len > 0.0f)
            {
                X /= len;
                Y /= len;
            }
            else
            {
                X = Y = 0.0f;
            }
        }

        public static float Dot(Vector2 a, Vector2 b) => a.X * b.X + a.Y * b.Y;

        public static float Distance(Vector2 a, Vector2 b) => (a - b).Length();
        public static float DistanceSquared(Vector2 a, Vector2 b) => (a - b).LengthSquared();

        public static Vector2 ClampMagnitude(Vector2 vector, float maxLength)
        {
            float sqrLen = vector.LengthSquared();
            if (sqrLen > maxLength * maxLength)
            {
                float len = Sqrt(sqrLen);
                return new Vector2(vector.X / len * maxLength, vector.Y / len * maxLength);
            }
            return vector;
        }

        public static Vector2 Lerp(Vector2 a, Vector2 b, float t)
        {
            return new Vector2(Mathf.Lerp(a.X, b.X, t), Mathf.Lerp(a.Y, b.Y, t));
        }

        public static Vector2 operator *(Vector2 vector, float scalar)
        {
            return new Vector2(vector.X * scalar, vector.Y * scalar);
        }

        public static Vector2 operator *(float scalar, Vector2 vector)
        {
            return new Vector2(vector.X * scalar, vector.Y * scalar);
        }

        public static Vector2 operator /(Vector2 vector, float scalar)
        {
            return new Vector2(vector.X / scalar, vector.Y / scalar);
        }

        public static Vector2 operator +(Vector2 vectorA, Vector2 vectorB)
        {
            return new Vector2(vectorA.X + vectorB.X, vectorA.Y + vectorB.Y);
        }

        public static Vector2 operator -(Vector2 vectorA, Vector2 vectorB)
        {
            return new Vector2(vectorA.X - vectorB.X, vectorA.Y - vectorB.Y);
        }

        public static Vector2 operator -(Vector2 vector)
        {
            return new Vector2(-vector.X, -vector.Y);
        }
    }
}

