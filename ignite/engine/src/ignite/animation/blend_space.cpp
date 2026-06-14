// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include "blend_space.hpp"

#include "ignite/core/logger.hpp"
#include <fstream>

#pragma warning(push)
#pragma warning(disable : 4275 4251)
#include <yaml-cpp/yaml.h>
#pragma warning(pop)

namespace ignite
{
    bool BlendSpace::Serialize(const ignite::Path &filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "BlendSpace" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Name" << YAML::Value << name;
        out << YAML::Key << "SkeletonHandle" << YAML::Value << static_cast<uint64_t>(skeletonHandle);
        out << YAML::Key << "AxisXName" << YAML::Value << axisXName;
        out << YAML::Key << "AxisYName" << YAML::Value << axisYName;
        out << YAML::Key << "AxisMin" << YAML::Value << YAML::Flow << YAML::BeginSeq << axisMin.x << axisMin.y << YAML::EndSeq;
        out << YAML::Key << "AxisMax" << YAML::Value << YAML::Flow << YAML::BeginSeq << axisMax.x << axisMax.y << YAML::EndSeq;

        out << YAML::Key << "Samples" << YAML::Value << YAML::BeginSeq;
        for (const BlendSpaceSample &sample : samples)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "AnimationHandle" << YAML::Value << static_cast<uint64_t>(sample.animationHandle);
            out << YAML::Key << "Position" << YAML::Value << YAML::Flow << YAML::BeginSeq << sample.position.x << sample.position.y << YAML::EndSeq;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::EndMap;
        out << YAML::EndMap;

        std::ofstream file(filepath);
        if (!file.is_open())
        {
            LOG_ERROR("[BlendSpace] Failed to open file for writing: {}", filepath.string());
            return false;
        }

        file << out.c_str();
        SetDirtyFlag(false);
        return true;
    }

    Ref<BlendSpace> BlendSpace::Deserialize(const ignite::Path &filepath)
    {
        if (!ignite::Path::exists(filepath))
        {
            return nullptr;
        }

        YAML::Node root = YAML::LoadFile(filepath.string());
        YAML::Node node = root["BlendSpace"];
        if (!node)
        {
            return nullptr;
        }

        Ref<BlendSpace> blendSpace = CreateRef<BlendSpace>();
        if (auto n = node["Name"]) blendSpace->name = n.as<std::string>();
        if (auto n = node["SkeletonHandle"]) blendSpace->skeletonHandle = AssetHandle(n.as<uint64_t>());
        if (auto n = node["AxisXName"]) blendSpace->axisXName = n.as<std::string>();
        if (auto n = node["AxisYName"]) blendSpace->axisYName = n.as<std::string>();
        if (auto n = node["AxisMin"]; n && n.IsSequence() && n.size() == 2)
        {
            blendSpace->axisMin = glm::vec2(n[0].as<float>(), n[1].as<float>());
        }

        if (auto n = node["AxisMax"]; n && n.IsSequence() && n.size() == 2)
        {
            blendSpace->axisMax = glm::vec2(n[0].as<float>(), n[1].as<float>());
        }

        if (YAML::Node samplesNode = node["Samples"]; samplesNode && samplesNode.IsSequence())
        {
            for (const auto &sampleNode : samplesNode)
            {
                BlendSpaceSample sample;
                if (auto n = sampleNode["AnimationHandle"]) sample.animationHandle = AssetHandle(n.as<uint64_t>());
                if (auto n = sampleNode["Position"]; n && n.IsSequence() && n.size() == 2)
                {
                    sample.position = glm::vec2(n[0].as<float>(), n[1].as<float>());
                }

                blendSpace->samples.push_back(sample);
            }
        }

        blendSpace->SetDirtyFlag(false);
        blendSpace->SetReadyFlag(true);
        return blendSpace;
    }
}