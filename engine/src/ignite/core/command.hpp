/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
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
        
        static CommandManager *GetInstance();

        static void AddCommand(Scope<ICommand> command)
        {
            GetInstance()->m_UndoStack.push(std::move(command));
            GetInstance()->m_RedoStack = std::stack<Scope<ICommand>>();
        }

        static void ExecuteCommand(Scope<ICommand> command)
        {
            command->Execute();
            GetInstance()->m_UndoStack.push(std::move(command));
            GetInstance()->m_RedoStack = std::stack<Scope<ICommand>>();
        }

        void Undo()
        {
            if (m_UndoStack.empty())
            {
                return;
            }

            // store undo stack and pop it
            auto command = std::move(m_UndoStack.top());
            m_UndoStack.pop();
            command->Undo(); // execute undo command

            // push to redo
            m_RedoStack.push(std::move(command));
        }

        void Redo()
        {
            if (m_RedoStack.empty())
            {
                return;
            }

            // store redo stack and pop it
            auto command = std::move(m_RedoStack.top());
            m_RedoStack.pop();
            command->Execute(); // execute redo command

            // push to undo
            m_UndoStack.push(std::move(command));
        }

    private:
        std::stack<Scope<ICommand>> m_UndoStack;
        std::stack<Scope<ICommand>> m_RedoStack;
    };
}
