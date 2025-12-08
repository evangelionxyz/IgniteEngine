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
#include "key_codes.hpp"
#include "event.hpp"

#include <sstream>

namespace ignite
{
    class KeyEvent : public Event
    {
    public:
        [[nodiscard]] KeyCode GetKeyCode() const { return m_KeyCode; }
        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

    protected:
        explicit KeyEvent(const KeyCode keycode)
            : m_KeyCode(keycode) {}

        KeyCode m_KeyCode;
    };

    class KeyPressedEvent final : public KeyEvent
    {
    public:
        KeyPressedEvent(const KeyCode keycode, const uint16_t repeatCount)
            : KeyEvent(keycode), m_RepeatCount(repeatCount) {}

        [[nodiscard]] uint16_t GetRepeatCount() const { return m_RepeatCount; }

        [[nodiscard]] std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << m_KeyCode << " (" << m_RepeatCount << " repeats)";
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyPressed)
    private:
        uint16_t m_RepeatCount;
    };

    class KeyReleasedEvent final : public KeyEvent
    {
    public:
        explicit KeyReleasedEvent(const KeyCode keycode)
            : KeyEvent(keycode) {}

        [[nodiscard]] std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << m_KeyCode;
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyReleased)
    };

    class KeyTypedEvent final : public KeyEvent
    {
    public:
        explicit KeyTypedEvent(const KeyCode keycode)
            : KeyEvent(keycode) {}

        [[nodiscard]] std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << m_KeyCode;
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyTyped)
    };
}
