/* MIT License
* 
* Copyright (c) 2026 Evangelion Manuhutu
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

#include "ignite/core/base.hpp"
#include "component.hpp"

#include "scene.hpp"
#include "ignite/core/types.hpp"
#include <entt/entt.hpp>

namespace ignite
{
    class IGN_API Entity
    {
    public:
        Entity();
        Entity(entt::entity e, Scene *scene);
        Entity(const Entity &other);

        ~Entity();

        template<typename T, typename... Args>
        T &AddComponent(Args &&... args)
        {
            T &comp = m_Scene->registry->get_or_emplace<T>(m_Handle, std::forward<Args>(args)...);

            if (std::is_base_of_v<IComponent, T>)
            {
                m_Scene->OnComponentAdded<T>(*this, comp);
            }

            return comp;
        }

        template<typename T, typename... Args>
        T &AddOrReplaceComponent(Args &&... args)
        {
            T &comp = m_Scene->registry->emplace_or_replace<T>(m_Handle, std::forward<Args>(args)...);

            if (std::is_base_of_v<IComponent, T>)
            {
                m_Scene->OnComponentAdded<T>(*this, comp);
            }
            return comp;
        }

        template<typename T>
        T &GetComponent()
        {
            return m_Scene->registry->get<T>(m_Handle);
        }

        template<typename T>
        bool HasComponent() const
        {
            return m_Scene->registry->all_of<T>(m_Handle);
        }

        template<typename T>
        void RemoveComponent() const
        {
            T &comp = m_Scene->registry->get<T>(m_Handle);
            m_Scene->registry->remove<T>(m_Handle);
        }

        bool IsValid() const
        {
            if (!m_Scene)
                return false;

            return m_Scene->registry->valid(m_Handle);
        }

        operator entt::entity() const { return m_Handle; }
        operator i32() const { return static_cast<i32>(m_Handle); }
        operator u32() const { return static_cast<u32>(m_Handle); }
        operator u64() const { return static_cast<u64>(m_Handle); }

        operator bool() const { return IsValid(); }

        bool operator==(const Entity &other) const { return other.m_Handle == m_Handle && other.m_Scene == m_Scene; }
        bool operator!=(const Entity &other) const { return !(*this == other); }

        UUID GetUUID() { return GetComponent<IDComponent>().uuid; }
        UUID GetUUID() const { return const_cast<Entity *>(this)->GetComponent<IDComponent>().uuid; }
        UUID GetParentUUID() { return GetComponent<IDComponent>().parent; }
        UUID GetParentUUID() const { return const_cast<Entity *>(this)->GetComponent<IDComponent>().parent; }
        TransformComponent &GetTransform() { return GetComponent<TransformComponent>(); }
        const TransformComponent &GetTransform() const { return const_cast<Entity *>(this)->GetComponent<TransformComponent>(); }
        const std::string &GetName() { return GetComponent<IDComponent>().name; }
        const std::string &GetName() const { return const_cast<Entity *>(this)->GetComponent<IDComponent>().name; }

    private:
        entt::entity m_Handle;
        Scene *m_Scene;
    };
}
