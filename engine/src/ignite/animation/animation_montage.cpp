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
}