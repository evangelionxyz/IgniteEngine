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

#include "joystick_event.hpp"

#include "ignite/graphics/window.hpp"

namespace ignite {

    Window *JoystickManager::window = nullptr;

    void JoystickManager::Init(Window *window)
    {
        JoystickManager::window = window;
    }

    void JoystickManager::ConnectJoystick(int id)
    {
        Ref<Joystick> j = CreateRef<Joystick>(id);
        connectedJoystick.push_back(std::move(j));

        JoystickConnectionEvent event(id, joystick::state::CONNECTED);
        window->m_Callback(event);
    }

    void JoystickManager::DisconnectJoystick(int id)
    {
        disconnectedIDs.push_back(id);
        
        JoystickConnectionEvent event(id, joystick::state::DISCONNECTED);
        window->m_Callback(event);
    }

    const std::list<Ref<Joystick>> &JoystickManager::GetConnectedJoystick()
    {
        return connectedJoystick;
    }

    const std::list<int> &JoystickManager::GetDisconnectedJoystickIDs()
    {
        return disconnectedIDs;
    }

    std::list<Ref<Joystick>> JoystickManager::connectedJoystick;
    std::list<int> JoystickManager::disconnectedIDs;

}
