/* MIT License
* 
* Copyright (c) 2026 Evangelion Manuhutu
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include <functional>
#include <stack>
#include <deque>

#include "base.hpp"
#include "types.hpp"

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
