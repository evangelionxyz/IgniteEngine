// Copyright (c) 2026 Evangelion Manuhutu

#include <QApplication>
#include <QEventLoop>

#include <ignite/core/application.hpp>
#include <ignite/core/logger.hpp>

#include "editor_app.hpp"
#include "editor_layer.hpp"
#include "qt_sdl_event_bridge.hpp"
#include "qt_main_window.h"

int main(int argc, char **argv)
{
    QApplication qtApp(argc, argv);

    ignite::MainWindow mainWindow;
    mainWindow.show();

    ignite::Logger::Init();

    // Store Qt window handle and events process callback for Ignite Application creation
    ignite::ConfigureEditorHost(reinterpret_cast<void *>(mainWindow.GetViewportHandle()), [&qtApp]()
    {
        qtApp.processEvents(QEventLoop::AllEvents, 1);
    });

    // Create editor app
    ignite::Application *editorApp = ignite::CreateApplication({ argc, argv });

    ignite::Application::SubmitToMainThread([&mainWindow]()
    {
        if (ignite::EditorLayer *editorLayer = ignite::EditorLayer::GetInstance())
        {
            editorLayer->SetQtSceneHierarchyRefreshCallback([&mainWindow]()
            {
                mainWindow.RefreshSceneHierarchy();
            });
        }
        mainWindow.RefreshSceneHierarchy();
    });

    ignite::QtSdlEventBridge eventBridge(mainWindow.GetViewportWidget());
    mainWindow.SetCloseRequestedCallback([]()
    {
        ignite::Application::Shutdown();
    });

    eventBridge.AttachWindow(editorApp->GetWindow()->GetWindowHandle());
    eventBridge.SyncWindowState();

    if (mainWindow.GetViewportWidget())
    {
        mainWindow.GetViewportWidget()->setFocus();
    }

    editorApp->Run();

    mainWindow.SetCloseRequestedCallback({});
    mainWindow.close();

    delete editorApp;
    ignite::Logger::Shutdown();
    return 0;
}
