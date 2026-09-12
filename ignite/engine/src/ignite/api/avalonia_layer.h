// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_AVALONIA_LAYER_H
#define IGN_AVALONIA_LAYER_H

#include "ignite/core/layer.hpp"
#include "ignite/scene/editor_camera.hpp"

namespace ignite
{
    class Scene;
    class SceneRenderer;

    class AvaloniaLayer : public Layer
    {
    public:
        AvaloniaLayer(uint32_t width, uint32_t height);

        void OnAttach() override;
        void OnDetach() override;
        void Resize(uint32_t width, uint32_t height);
        void OnUpdate(float deltaTime) override;
        void OnRender(nvrhi::IFramebuffer *mainFramebuffer) override;
        void OnGuiRender() override;
        void Play();
        void Simulate();
        void Stop();
        void Pause();
        void StepFrame(int frames);
        int GetState();

        Ref<Scene> GetCurrentScene();
        Ref<Scene> GetEditorScene();

        void OnEvent(Event &e) override;
        void SetNavigationMode(int mode);
        int GetNavigationMode() const;
        EditorCamera &GetEditorCamera() { return m_EditorCamera; }
    private:
        Ref<SceneRenderer> m_SceneRenderer;

        Ref<Scene> m_FallbackScene;
        Ref<Scene> m_RuntimeScene;

        EditorCamera m_EditorCamera;
        uint32_t m_Width = 1280;
        uint32_t m_Height = 720;
    };
}

#endif
