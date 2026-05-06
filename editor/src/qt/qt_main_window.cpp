// Copyright (c) 2026 Evangelion Manuhutu

#include "qt_main_window.h"

#include "ignite/core/logger.hpp"
#include "editor_layer.hpp"
#include "panels/qt_scene_hierarchy_widget.hpp"

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
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <QUiLoader>
#include <QSplitter>

namespace ignite
{
    MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent)
    {
        setWindowTitle("Ignite Editor");
        resize(1640, 940);

        auto *splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setChildrenCollapsible(false);
        splitter->setHandleWidth(6);
        setCentralWidget(splitter);

        m_SceneHierarchy = new QtSceneHierarchyWidget(splitter);
        m_SceneHierarchy->setMinimumWidth(260);
        splitter->addWidget(m_SceneHierarchy);

        m_Viewport = new QWidget(splitter);
        m_Viewport->setObjectName("IgniteViewportHost");
        m_Viewport->setAttribute(Qt::WA_NativeWindow);
        m_Viewport->setAttribute(Qt::WA_PaintOnScreen);
        m_Viewport->setAttribute(Qt::WA_NoSystemBackground);
        m_Viewport->setAutoFillBackground(false);
        m_Viewport->setFocusPolicy(Qt::StrongFocus);
        m_Viewport->setMouseTracking(true);
        splitter->addWidget(m_Viewport);

        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);

        auto *viewportRefreshTimer = new QTimer(this);
        viewportRefreshTimer->setTimerType(Qt::PreciseTimer);
        viewportRefreshTimer->setInterval(16);
        QObject::connect(viewportRefreshTimer, &QTimer::timeout, this, [this]()
        {
            if (m_Viewport && m_Viewport->isVisible())
            {
                m_Viewport->update();
            }
        });
        viewportRefreshTimer->start();
    }

    MainWindow::~MainWindow()
    {
    }

    WId MainWindow::GetViewportHandle() const
    {
        return m_Viewport ? m_Viewport->winId() : 0;
    }

    void MainWindow::RefreshSceneHierarchy()
    {
        if (m_SceneHierarchy)
        {
            m_SceneHierarchy->RefreshHierarchy();
        }
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
