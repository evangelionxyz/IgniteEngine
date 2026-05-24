// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef INPUT_HPP
#define INPUT_HPP

#include <glm/vec2.hpp>

#include "key_codes.hpp"
#include "mouse_codes.hpp"

#include "ignite/graphics/window.hpp"

namespace ignite
{
    enum class CursorMode
    {
        Normal,
        Hidden,
        Disabled,
        Captured
    };

    class Input
    {
    public:
		Input(Window* window);

        static bool IsKeyPressed(KeyCode keycode);
        static bool IsModifierPressed(KeyModCode modcode);
        static bool IsMouseButtonPressed(MouseCode button);

        static glm::ivec2 GetMousePosition();
        
        static glm::vec2 GetGameplayMousePosition();
        static void SetGameplayMousePosition(float x, float y, bool enabled);
        static bool IsGameplayMousePositionEnabled();
        
        static void SetMouseToCenter();
        static void SetCursorMode(CursorMode mode);
        static void SetKey(SDL_Keycode key, bool pressed);
		static void SetModifier(SDL_Keymod mod, bool pressed);
        static void SetMouseButton(MouseCode button, bool pressed);
		static void SetMousePosition(i32 x, i32 y);

    private:
		static std::unordered_map<KeyModCode, bool> modifierState;
        static std::unordered_map<KeyCode, bool> keyState;
		static std::unordered_map<MouseCode, bool> mouseButtonState;

        static glm::ivec2 mousePosition;
        static glm::vec2 gameplayMousePosition;
        static bool gameplayMousePositionEnabled;
		static CursorMode cursorMode;

        static Window *window;
    };
}

#endif