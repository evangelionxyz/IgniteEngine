// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_LAYER_HPP
#define IGN_LAYER_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "input/event.hpp"

#include <string>
#include "SDL3/SDL_events.h"

namespace nvrhi
{
    class IFramebuffer;
}

namespace ignite
{
    class Event;

    class IGN_API Layer
    {
    public:
        virtual ~Layer() = default;
        Layer() = default;
        Layer(const std::string &name)
            : m_Name(name)
        {
        }

        virtual void OnAttach() { }
        virtual void OnDetach() { }
        virtual void OnUpdate(float deltaTime) { }
        virtual void OnRender(nvrhi::IFramebuffer *framebuffer) { }
        virtual void OnEvent(Event &e) { }
        virtual void OnGuiRender() { }
        std::string GetName() { return m_Name; }

    protected:
        std::string m_Name;
    };
}

#endif
