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

#include <array>
#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <vector>
#include "ignite_rs/core_utils.h"

namespace ignite::stringutils
{
    inline bool EndsWith(std::string_view const &value, std::string_view const &ending)
    {
        std::string v(value);
        std::string e(ending);
        return ignite_rs_string_ends_with(v.c_str(), e.c_str());
    }

    static std::string ToLower(const std::string &str)
    {
        std::string result(str.size(), '\0');
        size_t len = ignite_rs_string_to_lower(str.c_str(), result.data(), result.size() + 1);
        result.resize(len);
        return result;
    }

    static void ReplaceWith(std::string &outStr, const std::string &targetKey, const std::string &replaceKey)
    {
        while (outStr.find(targetKey) != std::string::npos)
        {
            size_t targetPos = outStr.find(targetKey);
            outStr.replace(targetPos, targetKey.size(), replaceKey);
        }
    }

    inline std::string Trim(const std::string &s)
    {
        std::string result(s.size(), '\0');
        size_t len = ignite_rs_string_trim(s.c_str(), result.data(), result.size() + 1);
        result.resize(len);
        return result;
    }

    inline std::vector<std::string> SplitString(const std::string &str, char delimiter)
    {
        std::vector<std::string> result;
        size_t start = 0;
        size_t end = str.find(delimiter);
        while (end != std::string::npos)
        {
            result.push_back(str.substr(start, end - start));
            start = end + 1;
            end = str.find(delimiter, start);
        }
        result.push_back(str.substr(start));
        return result;
    }
}
