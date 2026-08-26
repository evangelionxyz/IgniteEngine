// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_FMOD_AUDIO_HPP
#define IGN_FMOD_AUDIO_HPP

#include <fmod.hpp>
#include <fmod_common.h>
#include <fmod_errors.h>

#include <string>
#include <unordered_map>

#include "ignite/core/subsystem.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/logger.hpp"

#define FMOD_CHECK(x) do { FMOD_RESULT _fmod_result = (x); if (_fmod_result != FMOD_OK) { LOG_ERROR("[FMOD] {}", FMOD_ErrorString(_fmod_result)); DEBUGBREAK(); } } while (false)

namespace ignite
{
    struct FmodSound;

    class IGN_API FmodAudio : public Subsystem
    {
    public:
        virtual void Init() override;
        virtual void Shutdown() override;

        static void SetMasterVolume(float volume);
        static void MuteMaster(bool mute);

        static void Update();

        static FMOD::ChannelGroup *CreateChannelGroup(const std::string &name);
        static std::unordered_map<std::string, FMOD::ChannelGroup *> GetChannelGroupMap();

        static FMOD::ChannelGroup *GetChannelGroup(const std::string &name);
        static FmodAudio &GetInstance();
        static FMOD::System *GetFmodSystem();
        static FMOD::ChannelGroup *GetMasterChannel();
        static float GetMasterVolume();

    private:
        FMOD::System *m_System;
        FMOD::ChannelGroup *m_MasterGroup;
        std::unordered_map<std::string, FMOD::ChannelGroup *> m_ChannelGroups;

        FMOD_VECTOR listenerPos;
        FMOD_VECTOR listenerVel;
        FMOD_VECTOR listenerForward;
        FMOD_VECTOR listenerUp;

    };
}

#endif
