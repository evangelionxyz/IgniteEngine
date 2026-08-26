// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_COMMAND_HPP
#define IGN_COMMAND_HPP

#include <functional>
#include <stack>
#include <deque>

#include "ignite/core/base.hpp"
#include "ignite/core/buffer.hpp"
#include "ignite/core/types.hpp"

namespace ignite
{
    using CommandFunc = std::function<void()>;

    enum CommandState
    {
        CommandState_Create,
        CommandState_Destroy,
        CommandState_Renaming
    };

    class ICommand
    {
    public:
        virtual ~ICommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
    };

    class CommandManager
    {
    public:
        CommandManager();

        static IGN_API CommandManager *GetInstance();

        // AddCommand — push a pre-executed command (already applied, just record for undo)
        static void AddCommand(Scope<ICommand> command)
        {
            auto *inst = GetInstance();
            inst->m_UndoStack.push_back(std::move(command));
            inst->m_RedoStack.clear();
            // Trim oldest entries when stack exceeds the limit
            while (static_cast<int>(inst->m_UndoStack.size()) > inst->m_MaxStackSize)
                inst->m_UndoStack.pop_front();
        }

        // ExecuteCommand — call Execute() immediately, then record for undo
        static void ExecuteCommand(Scope<ICommand> command)
        {
            command->Execute();
            AddCommand(std::move(command));
        }

        void Undo()
        {
            if (m_UndoStack.empty())
                return;

            auto command = std::move(m_UndoStack.back());
            m_UndoStack.pop_back();
            command->Undo();
            m_RedoStack.push_back(std::move(command));
        }

        void Redo()
        {
            if (m_RedoStack.empty())
                return;

            auto command = std::move(m_RedoStack.back());
            m_RedoStack.pop_back();
            command->Execute();
            m_UndoStack.push_back(std::move(command));
        }

        // Clear — must be called when a new scene is loaded/created to
        // prevent stale Entity/Scene pointers in captured lambdas from crashing.
        void Clear()
        {
            m_UndoStack.clear();
            m_RedoStack.clear();
        }

        bool CanUndo() const { return !m_UndoStack.empty(); }
        bool CanRedo() const { return !m_RedoStack.empty(); }

        void SetMaxStackSize(int size) { m_MaxStackSize = size; }

    private:
        // Using deque instead of stack so we can pop from the front (trim oldest)
        std::deque<Scope<ICommand>> m_UndoStack;
        std::deque<Scope<ICommand>> m_RedoStack;
        int m_MaxStackSize = 100;
    };
}

#endif
