#pragma once

// C++ Standard Library
#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "ignite/core/path.hpp"
#include <optional>
#include <queue>
#include <atomic>

// Ignite Engine Core
#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/logger.hpp"

// Windows Header (minimal)
#ifdef PLATFORM_WINDOWS
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#endif
