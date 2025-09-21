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

#include "ignite/core/types.hpp"
#include <nvrhi/nvrhi.h>

namespace ignite
{
    class CommandList
    {
    public:
        CommandList(uint32_t count = 0);
        ~CommandList();

        void Begin();
        void Submit();

        nvrhi::CommandListHandle GetActiveHandle() const;
        nvrhi::CommandListHandle GetHandle(uint32_t index) const;

        static Ref<CommandList> Create(uint32_t count = 0);

    private:
        std::vector<nvrhi::CommandListHandle> m_CommandLists;
        nvrhi::IDevice* m_Device;
    };
}