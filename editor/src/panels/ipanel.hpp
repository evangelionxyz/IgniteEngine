// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IPANEL_HPP
#define IPANEL_HPP

#include "ignite/core/logger.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/layer.hpp"
#include "states.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <string>

namespace ignite
{
    class IPanel : public Layer
    {
    public:
        IPanel() = default;
        explicit IPanel(const char *windowTitle)
            : m_WindowTitle(windowTitle), Layer(windowTitle)
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
        bool m_IsOpen = true;
        bool m_IsFocused = false;
        bool m_IsHovered = false;
    };
}

#endif
