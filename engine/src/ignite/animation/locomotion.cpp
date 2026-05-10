// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "locomotion.hpp"

#include "ignite/core/logger.hpp"

#include <fstream>

#pragma warning(push)
#pragma warning(disable : 4275 4251)
#include <yaml-cpp/yaml.h>
#pragma warning(pop)

namespace ignite
{
    bool LocomotionController::Serialize(const ignite::Path &filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "LocomotionController" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Name" << YAML::Value << name;
        out << YAML::Key << "SkeletonHandle" << YAML::Value << static_cast<uint64_t>(skeletonHandle);
        out << YAML::Key << "DefaultState" << YAML::Value << defaultState;
        out << YAML::Key << "States" << YAML::Value << YAML::BeginSeq;

        for (const LocomotionState &state : states)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << state.name;
            out << YAML::Key << "UseBlendSpace" << YAML::Value << state.useBlendSpace;
            out << YAML::Key << "AssetHandle" << YAML::Value << static_cast<uint64_t>(state.assetHandle);
            out << YAML::EndMap;
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;
        out << YAML::EndMap;

        std::ofstream file(filepath);
        if (!file.is_open())
        {
            LOG_ERROR("[LocomotionController] Failed to open file for writing: {}", filepath.string());
            return false;
        }

        file << out.c_str();
        SetDirtyFlag(false);
        return true;
    }

    Ref<LocomotionController> LocomotionController::Deserialize(const ignite::Path &filepath)
    {
        if (!ignite::Path::exists(filepath))
        {
            return nullptr;
        }

        YAML::Node root = YAML::LoadFile(filepath.string());
        YAML::Node node = root["LocomotionController"];
        if (!node)
        {
            return nullptr;
        }

        Ref<LocomotionController> controller = CreateRef<LocomotionController>();
        if (auto n = node["Name"]) controller->name = n.as<std::string>();
        if (auto n = node["SkeletonHandle"]) controller->skeletonHandle = AssetHandle(n.as<uint64_t>());
        if (auto n = node["DefaultState"]) controller->defaultState = n.as<std::string>();

        if (YAML::Node statesNode = node["States"]; statesNode && statesNode.IsSequence())
        {
            for (const auto &stateNode : statesNode)
            {
                LocomotionState state;
                if (auto n = stateNode["Name"]) state.name = n.as<std::string>();
                if (auto n = stateNode["UseBlendSpace"]) state.useBlendSpace = n.as<bool>();
                if (auto n = stateNode["AssetHandle"]) state.assetHandle = AssetHandle(n.as<uint64_t>());
                controller->states.push_back(state);
            }
        }

        controller->SetDirtyFlag(false);
        controller->SetReadyFlag(true);
        return controller;
    }
}