// Copyright (c) 2026 Evangelion Manuhutu

#include "animation_montage.hpp"

#include "ignite/core/logger.hpp"

#pragma warning(push)
#pragma warning(disable : 4275 4251)
#include <yaml-cpp/yaml.h>
#pragma warning(pop)

#include <fstream>

namespace ignite
{
    void AnimationMontage::AddNotif(const std::string &name, float startTime, float endTime)
    {
        if (!m_Notifies.contains(name))
        {
            m_Notifies[name] = AnimNotif { startTime, endTime };
        }
    }

    void AnimationMontage::SetNotif(const std::string &name, const AnimNotif &notif)
    {
        if (m_Notifies.contains(name))
        {
            m_Notifies[name] = notif;
        }
    }

    void AnimationMontage::RemoveNotif(const std::string &name)
    {
        if (m_Notifies.contains(name))
        {
            m_Notifies.erase(name);
        }
    }

    AnimNotif AnimationMontage::GetAnimNotif(const std::string &name)
    {
        auto it = m_Notifies.find(name);
        if (it != m_Notifies.end())
        {
            return it->second;
        }
        return AnimNotif {};
    }

    bool AnimationMontage::Serialize(const std::filesystem::path &filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "AnimationMontage" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Name" << YAML::Value << name;
        out << YAML::Key << "AnimationHandle" << YAML::Value << static_cast<uint64_t>(m_AnimationHandle);
        out << YAML::Key << "SkeletonHandle" << YAML::Value << static_cast<uint64_t>(m_SkeletonHandle);
        out << YAML::Key << "Notifies" << YAML::Value << YAML::BeginSeq;

        for (const auto &[notifyName, notify] : m_Notifies)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << notifyName;
            out << YAML::Key << "StartTime" << YAML::Value << notify.startTime;
            out << YAML::Key << "EndTime" << YAML::Value << notify.endTime;
            out << YAML::EndMap;
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;
        out << YAML::EndMap;

        std::ofstream file(filepath);
        if (!file.is_open())
        {
            LOG_ERROR("[AnimationMontage] Failed to open file for writing: {}", filepath.string());
            return false;
        }

        file << out.c_str();
        SetDirtyFlag(false);
        return true;
    }

    Ref<AnimationMontage> AnimationMontage::Deserialize(const std::filesystem::path &filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            LOG_ERROR("[AnimationMontage] File does not exist: {}", filepath.string());
            return nullptr;
        }

        YAML::Node root;
        try
        {
            root = YAML::LoadFile(filepath.string());
        }
        catch (const YAML::Exception &e)
        {
            LOG_ERROR("[AnimationMontage] YAML parse error: {}", e.what());
            return nullptr;
        }

        YAML::Node node = root["AnimationMontage"];
        if (!node)
        {
            return nullptr;
        }

        Ref<AnimationMontage> montage = CreateRef<AnimationMontage>();
        if (auto n = node["Name"]) montage->name = n.as<std::string>();
        if (auto n = node["AnimationHandle"]) montage->m_AnimationHandle = AssetHandle(n.as<uint64_t>());
        if (auto n = node["SkeletonHandle"]) montage->m_SkeletonHandle = AssetHandle(n.as<uint64_t>());

        if (YAML::Node notifiesNode = node["Notifies"]; notifiesNode && notifiesNode.IsSequence())
        {
            for (const auto &notifyNode : notifiesNode)
            {
                std::string notifyName;
                AnimNotif notify;

                if (auto n = notifyNode["Name"]) notifyName = n.as<std::string>();
                if (auto n = notifyNode["StartTime"]) notify.startTime = n.as<float>();
                if (auto n = notifyNode["EndTime"]) notify.endTime = n.as<float>();

                if (!notifyName.empty())
                {
                    montage->m_Notifies[notifyName] = notify;
                }
            }
        }

        montage->SetDirtyFlag(false);
        montage->SetReadyFlag(true);
        return montage;
    }

    bool BlendSpace::Serialize(const std::filesystem::path &filepath)
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

    Ref<BlendSpace> BlendSpace::Deserialize(const std::filesystem::path &filepath)
    {
        if (!std::filesystem::exists(filepath))
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

    bool LocomotionController::Serialize(const std::filesystem::path &filepath)
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

    Ref<LocomotionController> LocomotionController::Deserialize(const std::filesystem::path &filepath)
    {
        if (!std::filesystem::exists(filepath))
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