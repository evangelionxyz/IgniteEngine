// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef QT_MAIN_WINDOW_HPP
#define QT_MAIN_WINDOW_HPP

#include <QMainWindow>
#include <functional>
#include <string>
#include <vector>

class QWidget;
class QCloseEvent;
class QDockWidget;
class QLabel;
class QPlainTextEdit;
class QTreeWidget;

namespace ignite
{
    class MainWindow : public QMainWindow
    {
    public:
        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow();

        WId GetViewportHandle() const;
        QWidget *GetViewportWidget() const { return m_Viewport; }
        void SetCloseRequestedCallback(std::function<void()> callback);

    protected:
        void closeEvent(QCloseEvent *event) override;

    private:
        QWidget *m_Viewport = nullptr;
        std::function<void()> m_CloseRequestedCallback;
    };
}

#endif
