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

﻿namespace Ignite
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
