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
#ifndef LAYER_HPP
#define LAYER_HPP

#include <string>
#include "types.hpp"
#include "input/event.hpp"

#include "SDL3/SDL_events.h"

namespace nvrhi
{
    class IFramebuffer;
}

namespace ignite
{
    class Event;

    class Layer
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
        virtual void OnSDLEvent(SDL_Event *evt) { }
        virtual void OnGuiRender() { }
        std::string GetName() { return m_Name; }

    protected:
        std::string m_Name;
    };
}

#endif
