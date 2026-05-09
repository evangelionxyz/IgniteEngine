// Copyright (c) 2026 Evangelion Manuhutu

namespace Ignite;

public static partial class Mathf
{
    public const float PI = 3.14159265358979323846f;
    public const float Deg2Rad = PI / 180f;
    public const float Rad2Deg = 180f / PI;
    public const float Epsilon = 1e-5f;

    public static float Sin(float x) => (float)System.Math.Sin(x);
    public static float Cos(float x) => (float)System.Math.Cos(x);
    public static float Tan(float x) => (float)System.Math.Tan(x);

    public static float Asin(float x) => (float)System.Math.Asin(x);
    public static float Acos(float x) => (float)System.Math.Acos(x);
    public static float Atan(float x) => (float)System.Math.Atan(x);
    public static float Atan2(float y, float x) => (float)System.Math.Atan2(y, x);

    public static float Sqrt(float x) => (float)System.Math.Sqrt(x);
    public static float Pow(float x, float y) => (float)System.Math.Pow(x, y);

    public static float Abs(float x) => System.Math.Abs(x);

    public static float Min(float a, float b) => (a < b) ? a : b;
    public static float Max(float a, float b) => (a > b) ? a : b;

    public static float Clamp(float value, float min, float max)
    {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

    public static float Lerp(float a, float b, float t)
    {
        return a + (b - a) * Clamp(t, 0f, 1f);
    }

    public static bool Approximately(float a, float b)
    {
        return Abs(b - a) < Max(1e-6f * Max(Abs(a), Abs(b)), Epsilon * 8);
    }

    public static float InverseLerp(float a, float b, float value)
    {
        if (a != b)
            return Clamp((value - a) / (b - a), 0f, 1f);
        return 0f;
    }

    public static float DeltaAngle(float current, float target)
    {
        float delta = target - current;
        while (delta > 180) delta -= 360;
        while (delta < -180) delta += 360;
        return delta;
    }

    public static float SmoothStep(float edge0, float edge1, float x)
    {
        x = Clamp((x - edge0) / (edge1 - edge0), 0f, 1f);
        return x * x * (3 - 2 * x);
    }

    public static int FloorToInt(float f) => (int)System.Math.Floor(f);
    public static int CeilToInt(float f) => (int)System.Math.Ceiling(f);
    public static int RoundToInt(float f) => (int)System.Math.Round(f);

    public static bool RayPlaneIntersection(Ray ray, Vector3 planeNormal, Vector3 planePoint, out float distance)
    {
        float denom = Vector3.Dot(planeNormal, ray.Direction);
        if (Abs(denom) > Epsilon)
        {
            Vector3 diff = planePoint - ray.Origin;
            distance = Vector3.Dot(diff, planeNormal) / denom;
            return distance >= 0f;
        }
        distance = 0f;
        return false;
    }

    public static bool RayQuadIntersection(Ray ray, Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3, out float distance)
    {
        Vector3 edge1 = v1 - v0;
        Vector3 edge2 = v3 - v0;
        Vector3 normal = Vector3.Cross(edge1, edge2).Normalized();

        if (RayPlaneIntersection(ray, normal, v0, out distance))
        {
            Vector3 hitPoint = ray.Origin + ray.Direction * distance;

            Vector3 n0 = Vector3.Cross(v1 - v0, hitPoint - v0);
            Vector3 n1 = Vector3.Cross(v2 - v1, hitPoint - v1);
            Vector3 n2 = Vector3.Cross(v3 - v2, hitPoint - v2);
            Vector3 n3 = Vector3.Cross(v0 - v3, hitPoint - v3);

            if (Vector3.Dot(normal, n0) >= 0 &&
                Vector3.Dot(normal, n1) >= 0 &&
                Vector3.Dot(normal, n2) >= 0 &&
                Vector3.Dot(normal, n3) >= 0)
            {
                return true;
            }
        }
        return false;
    }

}
