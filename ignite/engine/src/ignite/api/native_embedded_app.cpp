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
}
