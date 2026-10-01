// Copyright (c) 2026 Evangelion Manuhutu

#include "native_embedded_app.h"
#include "ignite/scene/scene.hpp"

#include "avalonia_layer.h"

namespace ignite
{
    NativeEmbeddedApp::NativeEmbeddedApp(const ApplicationCreateInfo &createInfo)
        : Application(createInfo)
    {
        m_AvaloniaLayer = new AvaloniaLayer(createInfo.width, createInfo.height);
        PushLayer(m_AvaloniaLayer);
    }

    void NativeEmbeddedApp::ResizeEmbedded(uint32_t width, uint32_t height)
    {
        Application::Resize(width, height);
        if (m_AvaloniaLayer)
        {
            m_AvaloniaLayer->Resize(width, height);
        }
    }

    void NativeEmbeddedApp::SetCameraNavigationMode(int mode)
    {
        if (m_AvaloniaLayer)
        {
            m_AvaloniaLayer->SetNavigationMode(mode);
        }
    }

    int NativeEmbeddedApp::GetCameraNavigationMode() const
    {
        if (m_AvaloniaLayer)
        {
            return m_AvaloniaLayer->GetNavigationMode();
        }
        return 0;
    }

    void NativeEmbeddedApp::SetActiveScene(const Ref<Scene> &scene)
    {
        if (m_AvaloniaLayer)
        {
            m_AvaloniaLayer->SetActiveScene(scene);
        }
    }

    bool NativeEmbeddedApp::SceneNew()
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->SceneNew() : false;
    }

    bool NativeEmbeddedApp::SceneOpen(const std::filesystem::path &filepath)
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->SceneOpen(filepath) : false;
    }

    bool NativeEmbeddedApp::SceneSave()
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->SceneSave() : false;
    }

    bool NativeEmbeddedApp::SceneSave(const std::filesystem::path &filepath)
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->SceneSave(filepath) : false;
    }

    uint64_t NativeEmbeddedApp::GetCurrentSceneHandle() const
    {
        return m_AvaloniaLayer ? static_cast<uint64_t>(m_AvaloniaLayer->GetCurrentSceneHandle()) : 0;
    }

    uint64_t NativeEmbeddedApp::GetActiveSceneHandle() const
    {
        return m_AvaloniaLayer ? static_cast<uint64_t>(m_AvaloniaLayer->GetActiveSceneHandle()) : 0;
    }

    const std::filesystem::path &NativeEmbeddedApp::GetCurrentSceneFilePath() const
    {
        static const std::filesystem::path s_EmptyPath;
        return m_AvaloniaLayer ? m_AvaloniaLayer->GetCurrentSceneFilePath() : s_EmptyPath;
    }

    bool NativeEmbeddedApp::IsCurrentSceneSaved() const
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->IsCurrentSceneSaved() : false;
    }


    void NativeEmbeddedApp::ScenePlay()
    {
        if (m_AvaloniaLayer)
        {
            m_AvaloniaLayer->ScenePlay();
        }
    }

    void NativeEmbeddedApp::SceneSimulate()
    {
        if (m_AvaloniaLayer)
        {
            m_AvaloniaLayer->SceneSimulate();
        }
    }

    void NativeEmbeddedApp::SceneStop()
    {
        if (m_AvaloniaLayer)
        {
            m_AvaloniaLayer->SceneStop();
        }
    }

    void NativeEmbeddedApp::ScenePause()
    {
        if (m_AvaloniaLayer)
        {
            m_AvaloniaLayer->ScenePause();
        }
    }

    void NativeEmbeddedApp::SceneStepFrame(int frames)
    {
        if (m_AvaloniaLayer)
        {
            m_AvaloniaLayer->SceneStepFrame(frames);
        }
    }

    int NativeEmbeddedApp::SceneGetState() const
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->SceneGetState() : static_cast<int>(ESceneState::Stop);
    }

    bool NativeEmbeddedApp::ProjectSave()
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->ProjectSave() : false;
    }

    bool NativeEmbeddedApp::ProjectClose()
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->ProjectClose() : false;
    }

    uint64_t NativeEmbeddedApp::PickEntity(float mouseX, float mouseY, uint32_t viewportWidth, uint32_t viewportHeight, bool isDoubleClick, bool isShiftDown)
    {
        return m_AvaloniaLayer ? static_cast<uint64_t>(m_AvaloniaLayer->PickEntity(mouseX, mouseY, viewportWidth, viewportHeight, isDoubleClick, isShiftDown)) : 0u;
    }

    void NativeEmbeddedApp::SelectSingleEntityFromViewport(uint64_t uuid)
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->SelectSingleEntityFromViewport(UUID(uuid));
    }

    void NativeEmbeddedApp::SyncSingleSelection(uint64_t uuid)
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->SyncSingleSelection(UUID(uuid));
    }

    void NativeEmbeddedApp::AddEntityToSelectionFromViewport(uint64_t uuid)
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->AddEntityToSelectionFromViewport(UUID(uuid));
    }

    void NativeEmbeddedApp::DeselectEntityFromViewport(uint64_t uuid)
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->DeselectEntityFromViewport(UUID(uuid));
    }

    bool NativeEmbeddedApp::IsEntitySelected(uint64_t uuid) const
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->IsEntitySelected(UUID(uuid)) : false;
    }

    void NativeEmbeddedApp::ClearSelectionFromViewport()
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->ClearSelectionFromViewport();
    }

    void NativeEmbeddedApp::SyncClearSelection()
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->SyncClearSelection();
    }

    void NativeEmbeddedApp::SyncSelectedEntities(const uint64_t *uuids, uint32_t count)
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->SyncSelectedEntities(uuids, count);
    }

    uint32_t NativeEmbeddedApp::GetSelectedEntities(uint64_t *outUuids, uint32_t maxCount) const
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->GetSelectedEntities(outUuids, maxCount) : 0;
    }

    uint32_t NativeEmbeddedApp::GetSelectedEntityCount() const
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->GetSelectedEntityCount() : 0;
    }

    uint64_t NativeEmbeddedApp::GetSelectedEntity() const
    {
        return m_AvaloniaLayer ? static_cast<uint64_t>(m_AvaloniaLayer->GetSelectedEntityUUID()) : 0;
    }

    void NativeEmbeddedApp::SetEntitySelectedCallback(void (*callback)(uint64_t uuid, bool isMultiSelect))
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->SetEntitySelectedCallback(callback);
    }

    void NativeEmbeddedApp::SetGizmoOperation(int op)
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->SetGizmoOperation(op);
    }

    int NativeEmbeddedApp::GetGizmoOperation() const
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->GetGizmoOperation() : -1;
    }

    void NativeEmbeddedApp::CycleGizmoOperation()
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->CycleGizmoOperation();
    }

    void NativeEmbeddedApp::SetGizmoMode(int mode)
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->SetGizmoMode(mode);
    }

    int NativeEmbeddedApp::GetGizmoMode() const
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->GetGizmoMode() : 0;
    }

    void NativeEmbeddedApp::SetSnapEnabled(bool enabled)
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->SetSnapEnabled(enabled);
    }

    bool NativeEmbeddedApp::GetSnapEnabled() const
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->GetSnapEnabled() : false;
    }

    void NativeEmbeddedApp::SetSnapValue(int op, float value)
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->SetSnapValue(op, value);
    }

    float NativeEmbeddedApp::GetSnapValue(int op) const
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->GetSnapValue(op) : 0.0f;
    }

    void NativeEmbeddedApp::SetGizmoOperationChangedCallback(void (*callback)(int op))
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->SetGizmoOperationChangedCallback(callback);
    }
}
