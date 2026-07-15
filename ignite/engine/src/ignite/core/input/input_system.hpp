// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_INPUT_SYSTEM_HPP
#define IGN_INPUT_SYSTEM_HPP

#include "ignite/core/types.hpp"

#include <glm/vec2.hpp>
#include <unordered_map>
#include "input_layer.hpp"
#include "key_codes.hpp"
#include "mouse_codes.hpp"
#include "ignite/core/subsystem.hpp"
#include "SDL3/SDL_events.h"

namespace ignite
{
	class Scene;
	class Window;

	enum class CursorMode
	{
		Normal,
		Hidden,
		Disabled,
		Captured
	};

	class IGN_API InputSystem : public Subsystem
	{
	public:
		virtual void Init() override;
		virtual void Shutdown() override;

		virtual void ProcessEvent(SDL_Event *event) = 0;

		static bool IsKeyPressed(KeyCode keycode);
		static bool IsModifierPressed(KeyModCode modcode);
		static bool IsMouseButtonPressed(MouseCode button);
		static glm::ivec2 GetMousePosition();
		static glm::vec2 GetMouseDelta();

		static bool IsActionPressed(const std::string &actionName);
		static void SetInputMapping(Ref<class InputMapping> mapping);

		static glm::vec2 GetGameplayMousePosition();
		static void SetGameplayMousePosition(float x, float y, bool enabled);
		static bool IsGameplayMousePositionEnabled();

		static void SetActiveSystem(InputSystem* system);
		static InputSystem* GetActiveSystem() { return s_ActiveSystem; }

		static void SetWindow(Window* win);
		static Window* GetWindow();

		static void SetMouseToCenter();
		static void SetCursorMode(CursorMode mode);

		// Instance implementations
		bool IsKeyPressedImpl(KeyCode keycode) const;
		bool IsModifierPressedImpl(KeyModCode modcode) const;
		bool IsMouseButtonPressedImpl(MouseCode button) const;
		glm::ivec2 GetMousePositionImpl() const { return m_MousePosition; }

		bool IsActionPressedImpl(const std::string &actionName) const;
		void SetInputMappingImpl(Ref<class InputMapping> mapping);

		void SetKey(SDL_Keycode key, bool pressed);
		void SetModifier(SDL_Keymod mod, bool pressed);
		void SetMouseButton(MouseCode button, bool pressed);
		void SetJoystickButton(uint8_t button, bool pressed);
		void SetMousePosition(int x, int y);
		void AddMouseDelta(float dx, float dy);
		void ResetMouseDelta();

		CursorMode GetCursorMode() const { return m_CursorMode; }
		void SetCursorModeImpl(CursorMode mode);

	protected:
		void UpdateActionStateOnKey(KeyCode key, bool pressed);
		void UpdateActionStateOnMouseButton(MouseCode button, bool pressed);
		void UpdateActionStateOnJoystickButton(uint8_t button, bool pressed);

		std::unordered_map<KeyCode, bool> m_KeyState;
		std::unordered_map<KeyModCode, bool> m_ModifierState;
		std::unordered_map<MouseCode, bool> m_MouseButtonState;
		std::unordered_map<uint8_t, bool> m_JoystickButtonState;
		glm::ivec2 m_MousePosition{ 0 };
		glm::vec2 m_MouseDelta{ 0.0f };
		CursorMode m_CursorMode = CursorMode::Normal;

		Ref<class InputMapping> m_InputMapping;
		std::unordered_map<std::string, int> m_ActionPressCounts;

	private:
		static InputSystem* s_ActiveSystem;
		static Window* s_Window;
	};

	class IGN_API EditorInputSystem : public InputSystem
	{
	public:
		virtual void ProcessEvent(SDL_Event* event) override;
	};

	class IGN_API GameInputSystem : public InputSystem
	{
	public:
		virtual void ProcessEvent(SDL_Event* event) override;

		void SetGameplayMousePosition(float x, float y, bool enabled)
		{
			m_GameplayMousePosition = glm::vec2(x, y);
			m_GameplayMousePositionEnabled = enabled;
		}

		glm::vec2 GetGameplayMousePosition() const { return m_GameplayMousePosition; }
		bool IsGameplayMousePositionEnabled() const { return m_GameplayMousePositionEnabled; }

	private:
		glm::vec2 m_GameplayMousePosition{ 0.0f };
		bool m_GameplayMousePositionEnabled = false;
	};

	class IGN_API SceneInput : public Subsystem
	{
	public:
		static void SetSceneContext(Scene* scene);
		static void HandleMouseMotion(float mouseX, float mouseY);
		static void CancelWorldHover();
	};
}

#endif

