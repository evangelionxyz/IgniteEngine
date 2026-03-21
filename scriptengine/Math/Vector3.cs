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

    public Vector3 Normalized()
    {
        float length = Length();
        if (length > 0.0f)
        {
            return new Vector3(X / length, Y / length, Z / length);
        }
        return Zero;
    }

    public float Length()
    {
        return (float)Math.Sqrt(X * X + Y * Y + Z * Z);
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
}
