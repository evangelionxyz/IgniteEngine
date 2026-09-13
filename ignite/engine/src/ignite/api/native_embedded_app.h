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

        uint64_t PickEntity(float mouseX, float mouseY, uint32_t viewportWidth, uint32_t viewportHeight, bool isDoubleClick, bool isShiftDown);
        void SetSelectedEntity(uint64_t uuid);
        void SelectEntity(uint64_t uuid, bool multiSelect);
        void DeselectEntity(uint64_t uuid);
        bool IsEntitySelected(uint64_t uuid) const;
        void ClearSelectedEntities();
        void SetSelectedEntities(const uint64_t *uuids, uint32_t count);
        uint32_t GetSelectedEntities(uint64_t *outUuids, uint32_t maxCount) const;
        uint32_t GetSelectedEntityCount() const;
        uint64_t GetSelectedEntity() const;
        void SetEntitySelectedCallback(void (*callback)(uint64_t uuid, bool isMultiSelect));

        AvaloniaLayer *GetAvaloniaLayer() { return m_AvaloniaLayer; }

    private:
        AvaloniaLayer *m_AvaloniaLayer = nullptr;
    };
}

#endif
