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

#include "ignite/core/command.hpp"

namespace ignite
{
    class EntityManagerCommand : public ICommand
    {
    public:
        EntityManagerCommand(const CommandFunc &createFunc, const CommandFunc &destroyFunc, CommandState state)
        {
            m_CreateFunc = createFunc;
            m_DestroyFunc = destroyFunc;
            m_State = CommandState_Create;
        }

        virtual void Execute() override
        {
            switch (m_State)
            {
            case CommandState_Create:
            {
                m_CreateFunc();
                break;
            }
            case CommandState_Destroy:
            {
                m_DestroyFunc();
                break;
            }
            }
        }

        virtual void Undo() override
        {
            switch (m_State)
            {
            case CommandState_Create:
            {
                m_DestroyFunc();
                break;
            }
            case CommandState_Destroy:
            {
                m_CreateFunc();
                break;
            }
            }
        }

    private:
        CommandFunc m_CreateFunc;
        CommandFunc m_DestroyFunc;
        CommandState m_State;
    };
}
