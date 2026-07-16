// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "fmod_audio.hpp"
#include "fmod_sound.hpp"

namespace ignite {

    static FMOD_RESULT result;
    static FmodAudio *s_FmodAudio = nullptr;

    void FmodAudio::SetMasterVolume(const float volume)
    {
        result = s_FmodAudio->m_MasterGroup->setVolume(volume); 
        FMOD_CHECK(result);
    }

    void FmodAudio::MuteMaster(const bool mute)
    {
        result = s_FmodAudio->m_MasterGroup->setMute(mute);
        FMOD_CHECK(result);
    }

    void FmodAudio::Update()
    {
        s_FmodAudio->m_System->update();
    }

    void FmodAudio::Init()
    {
        s_FmodAudio = new FmodAudio();

        result = FMOD::System_Create(&s_FmodAudio->m_System);
        FMOD_CHECK(result);

        result = s_FmodAudio->m_System->init(32, FMOD_INIT_NORMAL, nullptr);
        FMOD_CHECK(result);

        s_FmodAudio->m_MasterGroup = FmodAudio::CreateChannelGroup("Master");

        // Initialize listener position
        s_FmodAudio->listenerPos = { 0.0f,0.0f,0.0f };
        s_FmodAudio->listenerVel = { 0.0f,0.0f,0.0f };
        s_FmodAudio->listenerForward = { 0.0f,0.0f,1.0f };
        s_FmodAudio->listenerUp = { 0.0f,1.0f,0.0f };
    }

    void FmodAudio::Shutdown()
    {
        if (!s_FmodAudio)
        {
            return;
        }

        if (s_FmodAudio->m_System)
        {
            s_FmodAudio->m_System->mixerSuspend();

            if (s_FmodAudio->m_MasterGroup)
            {
                s_FmodAudio->m_MasterGroup->stop();
                s_FmodAudio->m_MasterGroup->release();
                s_FmodAudio->m_MasterGroup = nullptr;
            }

            for (auto &[name, group] : s_FmodAudio->m_ChannelGroups)
            {
                if (group)
                {
                    group->stop();
                    group->release();
                }
            }

            s_FmodAudio->m_ChannelGroups.clear();

            s_FmodAudio->m_System->mixerResume();
            s_FmodAudio->m_System->update();

            result = s_FmodAudio->m_System->close();
            FMOD_CHECK(result);
            result = s_FmodAudio->m_System->release();
            FMOD_CHECK(result);
            s_FmodAudio->m_System = nullptr;
        }

        delete s_FmodAudio;
        s_FmodAudio = nullptr;
    }

    FMOD::ChannelGroup* FmodAudio::CreateChannelGroup(const std::string &name)
    {
        FMOD::ChannelGroup* group = nullptr;
        s_FmodAudio->m_System->createChannelGroup(name.c_str(), &group);
        s_FmodAudio->m_ChannelGroups[name] = group;
        return group;
    }
    
    std::unordered_map<std::string, FMOD::ChannelGroup*> FmodAudio::GetChannelGroupMap()
    {
        return s_FmodAudio->m_ChannelGroups;    
    }

    FMOD::ChannelGroup* FmodAudio::GetChannelGroup(const std::string& name)
    {
        if (s_FmodAudio->m_ChannelGroups.contains(name))
            return s_FmodAudio->m_ChannelGroups[name];
        return nullptr;
    }

    FmodAudio &FmodAudio::GetInstance()
    {
        return *s_FmodAudio;
    }

    FMOD::System* FmodAudio::GetFmodSystem()
    {
        return s_FmodAudio->m_System;
    }

    FMOD::ChannelGroup* FmodAudio::GetMasterChannel()
    {
        return s_FmodAudio->m_MasterGroup;
    }

    float FmodAudio::GetMasterVolume()
    {
        float volume = 0.0f;
        s_FmodAudio->m_MasterGroup->getVolume(&volume);
        return volume;
    }
}
