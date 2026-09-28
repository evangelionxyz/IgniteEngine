// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_EVENT_HPP
#define IGN_EVENT_HPP

#include "ignite/core/base.hpp"
#include <sstream>

namespace ignite
{
    enum class EventSource
    {
        None = 0,
        Editor,
        Runtime,
        Viewport,
    };

    enum class EventType
    {
        None = 0,
        WindowClose,
        WindowResize,
        WindowFocus,
        WindowLostFocus,
        WindowMoved,
        WindowDrop,
        WindowMaximized,
        WindowMinimized,
        WindowRestored,
        WindowDPIScaleChanged,
        FramebufferResize,
        AppTick,
        AppUpdate,
        AppRender,
        Joystick,
        KeyPressed,
        KeyReleased,
        KeyTyped,
        MouseButtonPressed,
        MouseButtonReleased,
        MouseMoved,
        MouseScrolled,
    };

    enum EventCategory
    {
        EventCategoryApplication = BIT(0),
        EventCategoryInput = BIT(1),
        EventCategoryKeyboard = BIT(2),
        EventCategoryMouse = BIT(3),
        EventCategoryMouseButton = BIT(4),
        EventCategoryJoystick = BIT(5),
    };

#define EVENT_CLASS_TYPE(type)\
static EventType GetStaticType() { return EventType::type; }\
virtual EventType GetEventType() const override { return GetStaticType(); }\
virtual const char* GetName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) virtual int GetCategoryFlags() const override { return category; }

    class Event
    {
    public:
        virtual ~Event() = default;

        bool Handled = false;

        virtual EventType GetEventType() const = 0;
        virtual EventSource GetSource() const { return m_Source; }
        virtual void SetSource(EventSource source) { m_Source = source; }
        virtual const char *GetName() const = 0;
        virtual int GetCategoryFlags() const = 0;
        virtual std::string ToString() const { return GetName(); }

        bool IsInCategory(const EventCategory category) const
        {
            return (GetCategoryFlags() & category) != 0;
        }
    protected:
        EventSource m_Source = EventSource::Viewport;

        friend class EventDispatcher;
    };

    class EventDispatcher
    {
    public:
        EventDispatcher(Event &event)
            : m_Event(event)
        {
        }

        // Dispatches to func for matching event type regardless of source
        template<typename T, typename F>
        bool Dispatch(const F &func)
        {
            if (m_Event.GetEventType() == T::GetStaticType())
            {
                m_Event.Handled |= func(static_cast<T &>(m_Event));
                return true;
            }
            return false;
        }

        // Dispatches to func only if event type matches AND matches the filtered source
        template<typename T, typename F>
        bool Dispatch(const F &func, EventSource filterSource)
        {
            if (m_Event.GetEventType() == T::GetStaticType())
            {
                if (filterSource != EventSource::None && m_Event.GetSource() != filterSource)
                {
                    return false;
                }
                m_Event.Handled |= func(static_cast<T &>(m_Event));
                return true;
            }
            return false;
        }
    private:
        Event &m_Event;
    };

    inline std::ostream &operator<<(std::ostream &os, const Event &e)
    {
        return os << e.ToString();
    }
}

#endif
