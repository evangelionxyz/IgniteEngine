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

#include "physics_2d_component.hpp"
#include <box2d/box2d.h>
#include <entt/entt.hpp>
#include <ignite/core/types.hpp>

namespace ignite
{
    class Scene;
    class Physics2D
    {
    public:
        Physics2D() = default;
        explicit Physics2D(Scene *scene);
        ~Physics2D();

        void SimulationStart();
        void SimulationStop();

        void Instantiate(entt::entity e);
        void DestroyBody(entt::entity e);

        void Simulate(f32 deltaTime);
        void CreateBoxCollider(BoxCollider2DComponent *box, b2BodyId bodyId, b2Vec2 size);
        void ApplyForce(Rigidbody2DComponent *body, const glm::vec2 &force, const glm::vec2 &point, bool wake);

    private:
        Scene *m_Scene;
        b2WorldId m_WorldId{ b2_nullWorldId };
    };
}
