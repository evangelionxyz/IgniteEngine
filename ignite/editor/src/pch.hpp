#pragma once

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <format>
#include <cmath>
#include <limits>
#include <array>
#include <filesystem>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <nvrhi/nvrhi.h>

#include "ignite/core/logger.hpp"
#include "ignite/core/buffer.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/core/platform_utils.hpp"
#include "ignite/core/profiler/profiler.hpp"

#include "ignite/core/input/event.hpp"
#include "ignite/core/input/key_event.hpp"
#include "ignite/core/input/mouse_event.hpp"
#include "ignite/core/input/app_event.hpp"
#include "ignite/core/input/mouse_codes.hpp"

#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_manager.hpp"

#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/scene.hpp"
