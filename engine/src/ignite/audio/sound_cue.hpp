// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef SOUND_CUE_HPP
#define SOUND_CUE_HPP

#include "fmod_audio.hpp"
#include "fmod_sound.hpp"
#include "fmod_dsp.hpp"

namespace ignite
{
    class SoundCue
    {
    public:
        SoundCue();

        void AddSound(const ignite::Path &filepath);
        void AddSound(Ref<FmodSound> sound);

        void AddDSP(Ref<FmodDsp> dsp);
        
        std::vector<Ref<FmodSound>> sounds;
        FMOD::Channel *channel;

        static Ref<SoundCue> Create();
    };
}

#endif