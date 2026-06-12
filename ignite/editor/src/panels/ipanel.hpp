// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IPANEL_HPP
#define IPANEL_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/layer.hpp"
#include "states.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <string>

namespace ignite
{
    class EditorLayer;

    class IPanel : public Layer
    {
    public:
        IPanel() = default;
		explicit IPanel(const char *name, EditorLayer *editor)
            : m_WindowTitle(name), Layer(name), m_EditorLayer(editor)
        {
        }

        virtual ~IPanel() {};

        // to child class
        virtual bool IsOpen() { return m_IsOpen; }
        virtual bool IsFocused() { return m_IsFocused; }
        virtual bool IsHovered() { return m_IsHovered; }

        std::string &GetTitle() { return m_WindowTitle; }

    protected:
        std::string m_WindowTitle;
        EditorLayer *m_EditorLayer;

        bool m_IsOpen = true;
        bool m_IsFocused = false;
        bool m_IsHovered = false;
    };
}

#endif
