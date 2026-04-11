// Copyright (c) 2026 Evangelion Manuhutu

#include "animator_controller.hpp"

#pragma warning(push)
#pragma warning(disable : 4275 4251)
#include <yaml-cpp/yaml.h>
#pragma warning(pop)

namespace ignite
{

    bool AnimatorController::Serialize(const std::filesystem::path &filepath)
    {
        return false;
    }

    Ref<AnimatorController> AnimatorController::Deserialize(const std::filesystem::path &filepath)
    {
        return nullptr;
    }

    Ref<AnimatorController> AnimatorController::Create()
    {
        return CreateRef<AnimatorController>();
    }

}