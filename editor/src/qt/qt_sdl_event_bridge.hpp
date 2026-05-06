//Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef QT_SDL_EVENT_BRIDGE_HPP
#define QT_SDL_EVENT_BRIDGE_HPP

#include <QObject>
#include <QPoint>
#include <QPointF>

#include <SDL3/SDL.h>

class QWidget;
class QEvent;
class QKeyEvent;

namespace ignite
{
    class QtSdlEventBridge final : public QObject
    {
    public:
        explicit QtSdlEventBridge(QWidget *viewportWidget);

        void AttachWindow(SDL_Window *window);
        void SyncWindowState() const;

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;

    private:
        void PushWindowEvent(SDL_EventType type, int data1 = 0, int data2 = 0) const;
        void PushMouseMotionEvent(const QPointF &position) const;
        void PushMouseButtonEvent(SDL_EventType type, Qt::MouseButton button, const QPointF &position) const;
        void PushMouseWheelEvent(const QPoint &delta) const;
        void PushKeyEvent(SDL_EventType type, QKeyEvent *event) const;
        void PushTextInputEvent(QKeyEvent *event) const;

        SDL_Keycode ToSdlKey(Qt::Key key) const;
        SDL_Keymod ToSdlModifiers(Qt::KeyboardModifiers modifiers) const;
        Uint8 ToSdlMouseButton(Qt::MouseButton button) const;

        QWidget *m_ViewportWidget = nullptr;
        SDL_Window *m_Window = nullptr;
        SDL_WindowID m_WindowId = 0;
    };
}

#endif
