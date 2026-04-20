/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

﻿using System;

namespace Ignite;

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

    public float Length()
    {
        return (float)Math.Sqrt(X * X + Y * Y);
    }

    public Vector2 Normalized()
    {
        float length = Length();
        if (length > 0.0f)
        {
            return new Vector2(X / length, Y / length);
        }
        return Zero;
    }

    public static float Dot(Vector2 a, Vector2 b) => a.X * b.X + a.Y * b.Y;

    public static float Distance(Vector2 a, Vector2 b) => (a - b).Length();

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
