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

#include "layer.hpp"
#include <list>

namespace ignite
{
    class LayerStack
    {
    public:
        LayerStack() = default;

        void PushLayer(Layer *layer)
        {
            m_Layers.emplace(m_Layers.end(), layer);
        }

        void PopLayer(Layer *layer)
        {
            m_Layers.remove(layer);
            delete layer;
        }

        std::list<Layer*>::const_iterator begin() { return m_Layers.begin(); }
        std::list<Layer*>::const_iterator end() { return m_Layers.end(); }
        std::list<Layer*>::const_reverse_iterator rbegin() { return m_Layers.rbegin(); }
        std::list<Layer*>::const_reverse_iterator rend() { return m_Layers.rend(); }

    private:
        std::list<Layer *> m_Layers;
    };
}
