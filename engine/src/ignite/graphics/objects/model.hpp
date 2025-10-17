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

#pragma once

#include "mesh.hpp"

#include <string>
#include <memory>
#include <glm/glm.hpp>

namespace ignite
{
    class Model
    {
    public:

        Model(const std::string& filename);
        ~Model();

        void PlayAnimation(const std::string& name);
        void StopAnimation();
        
        void Update(float deltaTime);
        void UpdateBindingSet(Scene* scene);

        void SetTransform(const glm::mat4& transform);

        static Ref<Model> Create(const std::string& filename);

        MeshScene& GetScene() { return m_Scene; }
        glm::mat4& GetTransform() { return m_Transform; }

    private:
        MeshScene m_Scene;
        std::string m_CurrentAnimation;
        glm::mat4 m_Transform = glm::mat4(1.0f);
    };
}