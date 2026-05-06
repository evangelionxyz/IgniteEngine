// Copyright (c) 2026 Evangelion Manuhutu

#include "qt_main_window.h"

#include "ignite/core/logger.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <QUiLoader>

namespace ignite
{
    MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent)
    {
        setWindowTitle("Ignite Editor");
        resize(1640, 940);

        auto central = new QWidget(this);
        central->setContentsMargins(0, 0, 0, 0);
        setCentralWidget(central);

        auto layout = new QVBoxLayout(central);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_Viewport = new QWidget(central);
        m_Viewport->setObjectName("IgniteViewportHost");
        m_Viewport->setAttribute(Qt::WA_NativeWindow);
        m_Viewport->setAttribute(Qt::WA_PaintOnScreen);
        m_Viewport->setFocusPolicy(Qt::StrongFocus);
        m_Viewport->setMouseTracking(true);

        layout->addWidget(m_Viewport);
    }

    MainWindow::~MainWindow()
    {
    }

    WId MainWindow::GetViewportHandle() const
    {
        return m_Viewport ? m_Viewport->winId() : 0;
    }

    void MainWindow::SetCloseRequestedCallback(std::function<void()> callback)
    {
        m_CloseRequestedCallback = std::move(callback);
    }

    void MainWindow::closeEvent(QCloseEvent *event)
    {
        if (m_CloseRequestedCallback)
        {
            m_CloseRequestedCallback();
            event->ignore();
            return;
        }

        QMainWindow::closeEvent(event);
    }
}
