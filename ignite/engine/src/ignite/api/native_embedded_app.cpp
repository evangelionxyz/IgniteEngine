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
            m_AvaloniaLayer->SetNavigationMode(mode);
    }

    int NativeEmbeddedApp::GetCameraNavigationMode() const
    {
        if (m_AvaloniaLayer)
            return m_AvaloniaLayer->GetNavigationMode();
        return 0;
    }

    void NativeEmbeddedApp::Play() { if (m_AvaloniaLayer) m_AvaloniaLayer->Play(); }
    void NativeEmbeddedApp::Simulate() { if (m_AvaloniaLayer) m_AvaloniaLayer->Simulate(); }
    void NativeEmbeddedApp::Stop() { if (m_AvaloniaLayer) m_AvaloniaLayer->Stop(); }
    void NativeEmbeddedApp::Pause() { if (m_AvaloniaLayer) m_AvaloniaLayer->Pause(); }
    void NativeEmbeddedApp::StepFrame(int frames) { if (m_AvaloniaLayer) m_AvaloniaLayer->StepFrame(frames); }
    int NativeEmbeddedApp::GetSceneState() const { return m_AvaloniaLayer ? m_AvaloniaLayer->GetState() : static_cast<int>(ESceneState::Stop); }

    uint64_t NativeEmbeddedApp::PickEntity(float mouseX, float mouseY, uint32_t viewportWidth, uint32_t viewportHeight, bool isDoubleClick, bool isShiftDown)
    {
        return m_AvaloniaLayer ? static_cast<uint64_t>(m_AvaloniaLayer->PickEntity(mouseX, mouseY, viewportWidth, viewportHeight, isDoubleClick, isShiftDown)) : 0u;
    }

    void NativeEmbeddedApp::SetSelectedEntity(uint64_t uuid)
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->SelectEntity(UUID(uuid), false);
    }

    void NativeEmbeddedApp::SelectEntity(uint64_t uuid, bool multiSelect)
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->SelectEntity(UUID(uuid), multiSelect);
    }

    void NativeEmbeddedApp::DeselectEntity(uint64_t uuid)
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->DeselectEntity(UUID(uuid));
    }

    bool NativeEmbeddedApp::IsEntitySelected(uint64_t uuid) const
    {
        return m_AvaloniaLayer ? m_AvaloniaLayer->IsEntitySelected(UUID(uuid)) : false;
    }

    void NativeEmbeddedApp::ClearSelectedEntities()
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->ClearSelectedEntities();
    }

    void NativeEmbeddedApp::SetSelectedEntities(const uint64_t *uuids, uint32_t count)
    {
        if (m_AvaloniaLayer)
            m_AvaloniaLayer->SetSelectedEntities(uuids, count);
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
}
