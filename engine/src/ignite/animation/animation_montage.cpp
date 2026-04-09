// Copyright (c) 2026 Evangelion Manuhutu

#include "animation_montage.hpp"

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
        return true;
    }

    Ref<AnimationMontage> AnimationMontage::Deserialize(const std::filesystem::path &filepath)
    {
        Ref<AnimationMontage> montage;
        return montage;
    }
}