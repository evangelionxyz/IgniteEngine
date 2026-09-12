// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_NATIVE_EMBEDDED_APP_H
#define IGN_NATIVE_EMBEDDED_APP_H

#include "ignite/core/application.hpp"

namespace ignite
{
    class AvaloniaLayer;

    class NativeEmbeddedApp final : public Application
    {
    public:
        explicit NativeEmbeddedApp(const ApplicationCreateInfo &createInfo);
        void ResizeEmbedded(uint32_t width, uint32_t height);
        void SetCameraNavigationMode(int mode);
        int GetCameraNavigationMode() const;

        void Play();
        void Simulate();
        void Stop();
        void Pause();
        void StepFrame(int frames);
        int GetSceneState() const;

        AvaloniaLayer *GetAvaloniaLayer() { return m_AvaloniaLayer; }

    private:
        AvaloniaLayer *m_AvaloniaLayer = nullptr;
    };
}

#endif
