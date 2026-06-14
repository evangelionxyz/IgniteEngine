// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_PLATFORM_UTILS_HPP
#define IGN_PLATFORM_UTILS_HPP

#include <string>
#include <vector>

#include "ignite/core/base.hpp"
#include "ignite/core/path.hpp"

namespace ignite
{
    struct IGN_API FileDialogs
    {
        static std::vector<std::string> OpenFiles(const char *filter);
        static std::string OpenFile(const char *filter);
        static std::string SelectFolder();
        static std::string SaveFile(const char *filter);
    };
}

#endif
