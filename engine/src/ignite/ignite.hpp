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

#ifdef PLATFORM_WINDOWS
#   include "core/device/device_manager_dx12.hpp"
#endif

#include "core/uuid.hpp"
#include "core/vfs/vfs.hpp"
#include "core/application.hpp"
#include "core/base.hpp"
#include "core/device/device_manager.hpp"
#include "core/device/device_manager_vk.hpp"
#include "core/input/event.hpp"
#include "core/input/app_event.hpp"
#include "core/input/key_event.hpp"
#include "core/input/mouse_event.hpp"
#include "core/input/key_codes.hpp"
#include "core/input/mouse_codes.hpp"
#include "core/layer.hpp"
#include "core/layer_stack.hpp"
#include "core/logger.hpp"
#include "core/string_utils.hpp"
#include "core/types.hpp"
#include "core/time.hpp"
#include "core/input/input.hpp"

#include "asset/asset.hpp"

#include "imgui/imgui_nvrhi.hpp"
#include "imgui/imgui_layer.hpp"

#include "graphics/renderer.hpp"
#include "graphics/shader.hpp"
#include "graphics/renderer/renderer_2d.hpp"
#include "graphics/renderer/scene_renderer.hpp"
#include "graphics/renderer/iscene_renderer.hpp"
#include "graphics/window.hpp"
#include "graphics/objects/mesh.hpp"

#include "math/math.hpp"
#include "math/aabb.hpp"
#include "math/obb.hpp"

#include "physics/2d/physics_2d.hpp"
#include "physics/2d/physics_2d_component.hpp"
#include "project/project.hpp"

#include "scene/component.hpp"
#include "scene/icamera.hpp"
#include "scene/entity.hpp"
#include "scene/scene.hpp"
#include "scene/scene_manager.hpp"
