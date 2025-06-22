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
#include "types.hpp"

namespace ignite
{
class UUID
{
public:
    UUID();
    explicit UUID(u64 uuid);
    UUID(const UUID &uuid) = default;
    operator u64() const { return m_UUID; }
private:
    u64 m_UUID;
};
}

template<>
struct std::hash<ignite::UUID>
{
    std::size_t operator() (const ignite::UUID& uuid) const noexcept
    {
        return hash<u64>()(uuid);
    }
};

// template<>
// struct formatter<UUID>
// {
//     template<typename ParseContext>
//     constexpr auto parse(ParseContext &ctx)
//     {
//         return ctx.begin();
//     }

//     template<typename FormatContext>
//     auto format(const UUID &uuid, FormatContext &ctx) const
//     {
//         return fmt::format_to(ctx.out(), "{}", u64(uuid));
//     }
// };

