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

#include "mesh_factory.hpp"

namespace ignite {

    std::array<VertexMesh, 24> MeshFactory::CubeVertices =
    {
        // Front face
        VertexMesh{{-0.5f, -0.5f,  0.5f}, { 0.f,  0.f,  1.f}, {0.f, 0.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{ 0.5f, -0.5f,  0.5f}, { 0.f,  0.f,  1.f}, {1.f, 0.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{ 0.5f,  0.5f,  0.5f}, { 0.f,  0.f,  1.f}, {1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{-0.5f,  0.5f,  0.5f}, { 0.f,  0.f,  1.f}, {0.f, 1.f}, {1.f, 1.f, 1.f, 1.f}},

        // Back face
        VertexMesh{{ 0.5f, -0.5f, -0.5f}, { 0.f,  0.f, -1.f}, {0.f, 0.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{-0.5f, -0.5f, -0.5f}, { 0.f,  0.f, -1.f}, {1.f, 0.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{-0.5f,  0.5f, -0.5f}, { 0.f,  0.f, -1.f}, {1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{ 0.5f,  0.5f, -0.5f}, { 0.f,  0.f, -1.f}, {0.f, 1.f}, {1.f, 1.f, 1.f, 1.f}},

        // Left face
        VertexMesh{{-0.5f, -0.5f, -0.5f}, {-1.f,  0.f,  0.f}, {0.f, 0.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{-0.5f, -0.5f,  0.5f}, {-1.f,  0.f,  0.f}, {1.f, 0.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{-0.5f,  0.5f,  0.5f}, {-1.f,  0.f,  0.f}, {1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{-0.5f,  0.5f, -0.5f}, {-1.f,  0.f,  0.f}, {0.f, 1.f}, {1.f, 1.f, 1.f, 1.f}},

        // Right face
        VertexMesh{{ 0.5f, -0.5f,  0.5f}, { 1.f,  0.f,  0.f}, {0.f, 0.f},  {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{ 0.5f, -0.5f, -0.5f}, { 1.f,  0.f,  0.f}, {1.f, 0.f},  {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{ 0.5f,  0.5f, -0.5f}, { 1.f,  0.f,  0.f}, {1.f, 1.f},  {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{ 0.5f,  0.5f,  0.5f}, { 1.f,  0.f,  0.f}, {0.f, 1.f},  {1.f, 1.f, 1.f, 1.f}},

        // Top face
        VertexMesh{{-0.5f,  0.5f,  0.5f}, { 0.f,  1.f,  0.f}, {0.f, 0.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{ 0.5f,  0.5f,  0.5f}, { 0.f,  1.f,  0.f}, {1.f, 0.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{ 0.5f,  0.5f, -0.5f}, { 0.f,  1.f,  0.f}, {1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{-0.5f,  0.5f, -0.5f}, { 0.f,  1.f,  0.f}, {0.f, 1.f}, {1.f, 1.f, 1.f, 1.f}},

        // Bottom face
        VertexMesh{{-0.5f, -0.5f, -0.5f}, { 0.f, -1.f,  0.f}, {0.f, 0.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{ 0.5f, -0.5f, -0.5f}, { 0.f, -1.f,  0.f}, {1.f, 0.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{ 0.5f, -0.5f,  0.5f}, { 0.f, -1.f,  0.f}, {1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}},
        VertexMesh{{-0.5f, -0.5f,  0.5f}, { 0.f, -1.f,  0.f}, {0.f, 1.f}, {1.f, 1.f, 1.f, 1.f}},
    };

    std::array<uint32_t, 36> MeshFactory::CubeIndices = {
        // Front face
        0, 1, 2,  2, 3, 0,
        // Back face
        4, 5, 6,  6, 7, 4,
        // Left face
        8, 9,10, 10,11, 8,
        // Right face
        12,13,14, 14,15,12,
        // Top face
        16,17,18, 18,19,16,
        // Bottom face
        20,21,22, 22,23,20
    };
}
