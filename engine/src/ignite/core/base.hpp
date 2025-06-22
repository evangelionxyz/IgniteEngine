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

#ifdef _WIN32
    #ifndef PLATFORM_WINDOWS
        #define PLATFORM_WINDOWS
    #endif
#elif __linux__ || __GNUG__
    #ifndef PLATFORM_LINUX
        #define PLATFORM_LINUX
    #endif
#endif

#ifdef _DEBUG
    #define ENABLE_ASSERTS
    #ifdef _WIN32
        #define DEBUGBREAK() __debugbreak();
    #elif __linux__
        #define DEBUGBREAK() __builtin_trap();
    #endif
#else
#define DEBUGBREAK()
#endif

#ifndef RELEASE_BUILD
    #define ENABLE_VERIFY
#endif

#define ENGINE_VERSION "Alpha-0.1"

#define EXPAND_MACRO(x)
#define STRINGIFY_MACRO(x) #x

#define BIT(x)(1 << x)
#define BIND_EVENT_FN(fn) [](auto&&... args) -> decltype(auto) { return fn(std::forward<decltype(args)>(args)...); }
#define BIND_CLASS_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }
