// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "animation_montage.hpp"

#include "ignite/core/logger.hpp"
#include "ignite/asset/asset_manager.hpp"

#pragma warning(push)
#pragma warning(disable : 4275 4251)
#include <yaml-cpp/yaml.h>
#pragma warning(pop)

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

    void AnimationMontage::AddNotifyCallback(float timestep, AnimationTimelineEvent::Action actionType, const std::string &callbackName)
    {
        m_NotifyCallbacks.emplace_back(timestep, actionType, callbackName);
        SetDirtyFlag(true);
    }

    void AnimationMontage::RemoveNotifyCallback(size_t index)
    {
        if (index < m_NotifyCallbacks.size())
        {
            m_NotifyCallbacks.erase(m_NotifyCallbacks.begin() + static_cast<ptrdiff_t>(index));
            SetDirtyFlag(true);
        }
    }

    void AnimationMontage::SetAnimationHandle(AssetHandle animationHandle)
    {
        m_AnimationHandle = animationHandle;
    }

    void AnimationMontage::SetSkeletonHandle(AssetHandle skeletonHandle)
    {
        m_SkeletonHandle = skeletonHandle;
    }

    bool AnimationMontage::Serialize(const std::filesystem::path &filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "AnimationMontage" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Version" << YAML::Value << 2;
        out << YAML::Key << "Name" << YAML::Value << name;
        out << YAML::Key << "AnimationHandle" << YAML::Value << static_cast<uint64_t>(m_AnimationHandle);
        out << YAML::Key << "SkeletonHandle" << YAML::Value << static_cast<uint64_t>(m_SkeletonHandle);

        // Range-based notifies
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

        // Timestep-based notify callbacks (v2)
        out << YAML::Key << "NotifyCallbacks" << YAML::Value << YAML::BeginSeq;
        for (const auto &cb : m_NotifyCallbacks)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Timestep" << YAML::Value << cb.timestep;
            out << YAML::Key << "ActionType" << YAML::Value << static_cast<uint32_t>(cb.actionType);
            out << YAML::Key << "CallbackName" << YAML::Value << cb.callbackName;
            out << YAML::Key << "AudioHandle" << YAML::Value << static_cast<uint64_t>(cb.audioHandle);
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        // Body part mask (v2)
        out << YAML::Key << "MaskedJoints" << YAML::Value << YAML::BeginSeq;
        for (int32_t jointIdx : m_MaskedJoints)
        {
            out << jointIdx;
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
        int version = 1;
        if (auto n = node["Version"]) version = n.as<int>();
        if (auto n = node["Name"]) montage->name = n.as<std::string>();
        if (auto n = node["AnimationHandle"]) montage->m_AnimationHandle = AssetHandle(n.as<uint64_t>());
        if (auto n = node["SkeletonHandle"]) montage->m_SkeletonHandle = AssetHandle(n.as<uint64_t>());

        // Range-based notifies (all versions)
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

        // Timestep-based notify callbacks (v2+, backward compatible)
        if (version >= 2)
        {
            if (YAML::Node callbacksNode = node["NotifyCallbacks"]; callbacksNode && callbacksNode.IsSequence())
            {
                montage->m_NotifyCallbacks.reserve(callbacksNode.size());
                for (const auto &cbNode : callbacksNode)
                {
                    AnimNotifyCallback cb;
                    if (auto n = cbNode["Timestep"]) cb.timestep = n.as<float>();
                    if (auto n = cbNode["ActionType"]) cb.actionType = static_cast<AnimationTimelineEvent::Action>(n.as<uint32_t>());
                    if (auto n = cbNode["CallbackName"]) cb.callbackName = n.as<std::string>();
                    if (auto n = cbNode["AudioHandle"]) cb.audioHandle = AssetHandle(n.as<uint64_t>());
                    montage->m_NotifyCallbacks.push_back(std::move(cb));
                }
            }

            // Body part mask (v2+)
            if (YAML::Node maskedNode = node["MaskedJoints"]; maskedNode && maskedNode.IsSequence())
            {
                montage->m_MaskedJoints.reserve(maskedNode.size());
                for (const auto &jointNode : maskedNode)
                {
                    montage->m_MaskedJoints.push_back(jointNode.as<int32_t>());
                }
            }
        }

        montage->SetDirtyFlag(false);
        montage->SetReadyFlag(true);
        return montage;
    }
}
