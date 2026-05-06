#include "qt_sdl_event_bridge.hpp"

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QWidget>
#include <QWheelEvent>

namespace ignite
{
    namespace
    {
        SDL_Keycode ToAsciiKey(Qt::Key key)
        {
            if (key >= Qt::Key_A && key <= Qt::Key_Z)
            {
                return static_cast<SDL_Keycode>('a' + (key - Qt::Key_A));
            }

            if (key >= Qt::Key_0 && key <= Qt::Key_9)
            {
                return static_cast<SDL_Keycode>('0' + (key - Qt::Key_0));
            }

            if (key >= Qt::Key_Space && key <= Qt::Key_AsciiTilde)
            {
                return static_cast<SDL_Keycode>(key);
            }

            return SDLK_UNKNOWN;
        }
    }

    QtSdlEventBridge::QtSdlEventBridge(QWidget *viewportWidget)
        : m_ViewportWidget(viewportWidget)
    {
        if (m_ViewportWidget)
        {
            m_ViewportWidget->installEventFilter(this);
            m_ViewportWidget->setAttribute(Qt::WA_KeyCompression, false);
            m_ViewportWidget->setFocusPolicy(Qt::StrongFocus);
            m_ViewportWidget->setMouseTracking(true);
        }
    }

    void QtSdlEventBridge::AttachWindow(SDL_Window *window)
    {
        m_Window = window;
        m_WindowId = window ? SDL_GetWindowID(window) : 0;
    }

    void QtSdlEventBridge::SyncWindowState() const
    {
        if (!m_ViewportWidget || !m_WindowId)
        {
            return;
        }

        const QSize logicalSize = m_ViewportWidget->size();
        const QSize pixelSize(
            static_cast<int>(logicalSize.width() * m_ViewportWidget->devicePixelRatioF()),
            static_cast<int>(logicalSize.height() * m_ViewportWidget->devicePixelRatioF()));

        PushWindowEvent(SDL_EVENT_WINDOW_RESIZED, logicalSize.width(), logicalSize.height());
        PushWindowEvent(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, pixelSize.width(), pixelSize.height());
        PushWindowEvent(SDL_EVENT_WINDOW_FOCUS_GAINED);
    }

    bool QtSdlEventBridge::eventFilter(QObject *watched, QEvent *event)
    {
        if (watched != m_ViewportWidget || !m_WindowId)
        {
            return QObject::eventFilter(watched, event);
        }

        switch (event->type())
        {
        case QEvent::Resize:
        {
            auto *resizeEvent = static_cast<QResizeEvent *>(event);
            const QSize pixelSize(
                static_cast<int>(resizeEvent->size().width() * m_ViewportWidget->devicePixelRatioF()),
                static_cast<int>(resizeEvent->size().height() * m_ViewportWidget->devicePixelRatioF()));
            PushWindowEvent(SDL_EVENT_WINDOW_RESIZED, resizeEvent->size().width(), resizeEvent->size().height());
            PushWindowEvent(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, pixelSize.width(), pixelSize.height());
            break;
        }
        case QEvent::FocusIn:
            PushWindowEvent(SDL_EVENT_WINDOW_FOCUS_GAINED);
            break;
        case QEvent::FocusOut:
            PushWindowEvent(SDL_EVENT_WINDOW_FOCUS_LOST);
            break;
        case QEvent::Enter:
            PushWindowEvent(SDL_EVENT_WINDOW_MOUSE_ENTER);
            break;
        case QEvent::Leave:
            PushWindowEvent(SDL_EVENT_WINDOW_MOUSE_LEAVE);
            break;
        case QEvent::MouseMove:
            PushMouseMotionEvent(static_cast<QMouseEvent *>(event)->position());
            return true;
        case QEvent::MouseButtonPress:
            PushMouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_DOWN, static_cast<QMouseEvent *>(event)->button(), static_cast<QMouseEvent *>(event)->position());
            return true;
        case QEvent::MouseButtonRelease:
            PushMouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_UP, static_cast<QMouseEvent *>(event)->button(), static_cast<QMouseEvent *>(event)->position());
            return true;
        case QEvent::Wheel:
            PushMouseWheelEvent(static_cast<QWheelEvent *>(event)->angleDelta());
            return true;
        case QEvent::KeyPress:
            PushKeyEvent(SDL_EVENT_KEY_DOWN, static_cast<QKeyEvent *>(event));
            PushTextInputEvent(static_cast<QKeyEvent *>(event));
            return true;
        case QEvent::KeyRelease:
            PushKeyEvent(SDL_EVENT_KEY_UP, static_cast<QKeyEvent *>(event));
            return true;
        default:
            break;
        }

        return QObject::eventFilter(watched, event);
    }

    void QtSdlEventBridge::PushWindowEvent(SDL_EventType type, int data1, int data2) const
    {
        if (!m_WindowId)
        {
            return;
        }

        SDL_Event event{};
        event.type = type;
        event.window.type = type;
        event.window.windowID = m_WindowId;
        event.window.data1 = data1;
        event.window.data2 = data2;
        SDL_PushEvent(&event);
    }

    void QtSdlEventBridge::PushMouseMotionEvent(const QPointF &position) const
    {
        SDL_Event event{};
        event.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.windowID = m_WindowId;
        event.motion.x = static_cast<float>(position.x());
        event.motion.y = static_cast<float>(position.y());
        SDL_PushEvent(&event);
    }

    void QtSdlEventBridge::PushMouseButtonEvent(SDL_EventType type, Qt::MouseButton button, const QPointF &position) const
    {
        const Uint8 sdlButton = ToSdlMouseButton(button);
        if (sdlButton == 0)
        {
            return;
        }

        SDL_Event event{};
        event.type = type;
        event.button.type = type;
        event.button.windowID = m_WindowId;
        event.button.button = sdlButton;
        event.button.x = static_cast<float>(position.x());
        event.button.y = static_cast<float>(position.y());
        SDL_PushEvent(&event);
    }

    void QtSdlEventBridge::PushMouseWheelEvent(const QPoint &delta) const
    {
        SDL_Event event{};
        event.type = SDL_EVENT_MOUSE_WHEEL;
        event.wheel.type = SDL_EVENT_MOUSE_WHEEL;
        event.wheel.windowID = m_WindowId;
        event.wheel.x = static_cast<float>(delta.x()) / 120.0f;
        event.wheel.y = static_cast<float>(delta.y()) / 120.0f;
        SDL_PushEvent(&event);
    }

    void QtSdlEventBridge::PushKeyEvent(SDL_EventType type, QKeyEvent *event) const
    {
        SDL_Event sdlEvent{};
        sdlEvent.type = type;
        sdlEvent.key.type = type;
        sdlEvent.key.windowID = m_WindowId;
        sdlEvent.key.key = ToSdlKey(static_cast<Qt::Key>(event->key()));
        sdlEvent.key.mod = static_cast<SDL_Keymod>(ToSdlModifiers(event->modifiers()));
        sdlEvent.key.repeat = event->isAutoRepeat();
        SDL_PushEvent(&sdlEvent);
    }

    void QtSdlEventBridge::PushTextInputEvent(QKeyEvent *event) const
    {
        const QString text = event->text();
        if (text.isEmpty() || text.at(0).isNull() || event->modifiers().testFlag(Qt::ControlModifier))
        {
            return;
        }

        const QByteArray utf8 = text.toUtf8();
        SDL_Event sdlEvent{};
        sdlEvent.type = SDL_EVENT_TEXT_INPUT;
        sdlEvent.text.type = SDL_EVENT_TEXT_INPUT;
        sdlEvent.text.windowID = m_WindowId;
        sdlEvent.text.text = utf8.constData();
        SDL_PushEvent(&sdlEvent);
    }

    SDL_Keycode QtSdlEventBridge::ToSdlKey(Qt::Key key) const
    {
        switch (key)
        {
        case Qt::Key_Escape: return SDLK_ESCAPE;
        case Qt::Key_Tab: return SDLK_TAB;
        case Qt::Key_Backspace: return SDLK_BACKSPACE;
        case Qt::Key_Return:
        case Qt::Key_Enter: return SDLK_RETURN;
        case Qt::Key_Insert: return SDLK_INSERT;
        case Qt::Key_Delete: return SDLK_DELETE;
        case Qt::Key_Pause: return SDLK_PAUSE;
        case Qt::Key_Print: return SDLK_PRINTSCREEN;
        case Qt::Key_Clear: return SDLK_CLEAR;
        case Qt::Key_Home: return SDLK_HOME;
        case Qt::Key_End: return SDLK_END;
        case Qt::Key_Left: return SDLK_LEFT;
        case Qt::Key_Up: return SDLK_UP;
        case Qt::Key_Right: return SDLK_RIGHT;
        case Qt::Key_Down: return SDLK_DOWN;
        case Qt::Key_PageUp: return SDLK_PAGEUP;
        case Qt::Key_PageDown: return SDLK_PAGEDOWN;
        case Qt::Key_Shift: return SDLK_LSHIFT;
        case Qt::Key_Control: return SDLK_LCTRL;
        case Qt::Key_Alt: return SDLK_LALT;
        case Qt::Key_Meta: return SDLK_LGUI;
        case Qt::Key_F1: return SDLK_F1;
        case Qt::Key_F2: return SDLK_F2;
        case Qt::Key_F3: return SDLK_F3;
        case Qt::Key_F4: return SDLK_F4;
        case Qt::Key_F5: return SDLK_F5;
        case Qt::Key_F6: return SDLK_F6;
        case Qt::Key_F7: return SDLK_F7;
        case Qt::Key_F8: return SDLK_F8;
        case Qt::Key_F9: return SDLK_F9;
        case Qt::Key_F10: return SDLK_F10;
        case Qt::Key_F11: return SDLK_F11;
        case Qt::Key_F12: return SDLK_F12;
        default:
            return ToAsciiKey(key);
        }
    }

    SDL_Keymod QtSdlEventBridge::ToSdlModifiers(Qt::KeyboardModifiers modifiers) const
    {
        SDL_Keymod result = SDL_KMOD_NONE;

        if (modifiers.testFlag(Qt::ShiftModifier))
            result = static_cast<SDL_Keymod>(result | SDL_KMOD_SHIFT);
        if (modifiers.testFlag(Qt::ControlModifier))
            result = static_cast<SDL_Keymod>(result | SDL_KMOD_CTRL);
        if (modifiers.testFlag(Qt::AltModifier))
            result = static_cast<SDL_Keymod>(result | SDL_KMOD_ALT);
        if (modifiers.testFlag(Qt::MetaModifier))
            result = static_cast<SDL_Keymod>(result | SDL_KMOD_GUI);

        return result;
    }

    Uint8 QtSdlEventBridge::ToSdlMouseButton(Qt::MouseButton button) const
    {
        switch (button)
        {
        case Qt::LeftButton: return SDL_BUTTON_LEFT;
        case Qt::RightButton: return SDL_BUTTON_RIGHT;
        case Qt::MiddleButton: return SDL_BUTTON_MIDDLE;
        case Qt::BackButton: return SDL_BUTTON_X1;
        case Qt::ForwardButton: return SDL_BUTTON_X2;
        default: return 0;
        }
    }
}
