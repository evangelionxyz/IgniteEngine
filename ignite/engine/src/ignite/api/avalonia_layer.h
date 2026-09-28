// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_AVALONIA_LAYER_H
#define IGN_AVALONIA_LAYER_H

#include "ignite/core/layer.hpp"
#include "ignite/scene/editor_camera.hpp"
#include "ignite/scene/entity.hpp"

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

        UUID PickEntity(float mouseX, float mouseY, uint32_t viewportWidth, uint32_t viewportHeight, bool isDoubleClick = false, bool isShiftDown = false);
        Entity SetSelectedEntity(UUID uuid);
        Entity SetSelectedEntity(Entity entity);
        void SelectEntity(UUID uuid, bool multiSelect = false);
        void DeselectEntity(UUID uuid);
        bool IsEntitySelected(UUID uuid) const;
        void ClearSelectedEntities();
        void SetSelectedEntities(const uint64_t *uuids, uint32_t count);
        uint32_t GetSelectedEntities(uint64_t *outUuids, uint32_t maxCount) const;
        uint32_t GetSelectedEntityCount() const;

        void SetMultiSelect(bool multiSelect) { m_MultiSelect = multiSelect; }
        bool IsMultiSelect() const { return m_MultiSelect; }

        Entity GetSelectedEntity() { return m_SelectedEntities.empty() ? Entity{} : m_SelectedEntities.begin()->second; }
        UUID GetSelectedEntityUUID() { return m_SelectedEntities.empty() ? UUID(0) : m_SelectedEntities.begin()->first; }

        using EntitySelectedCallback = void (*)(uint64_t uuid, bool isMultiSelect);
        void SetEntitySelectedCallback(EntitySelectedCallback callback) { m_EntitySelectedCallback = callback; }

    private:

        void DrawGizmo();

    private:

        Ref<SceneRenderer> m_SceneRenderer;

        Ref<Scene> m_FallbackScene;
        Ref<Scene> m_RuntimeScene;

        std::unordered_map<UUID, Entity> m_SelectedEntities;

        EditorCamera m_EditorCamera;
        uint32_t m_Width = 1280;
        uint32_t m_Height = 720;

        EntitySelectedCallback m_EntitySelectedCallback = nullptr;
        std::chrono::steady_clock::time_point m_LastClickTime{};
        glm::ivec2 m_LastClickPos{ 0, 0 };
        bool m_MultiSelect = false;
    };
}

#endif
