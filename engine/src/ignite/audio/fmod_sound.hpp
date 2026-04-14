/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include <fmod.hpp>
#include <fmod_dsp.h>

#include "ignite/asset/asset.hpp"

namespace ignite {
    
    struct FmodDsp;

    enum class AudioMode
    {
        Loop = 0,
    };

    struct FmodSound : public Asset
    {
        FmodSound() = default;
        FmodSound(const std::string &name, FMOD_MODE mode = FMOD_DEFAULT | FMOD_LOOP_OFF);
    
        void Play();
        void Stop() const;
        void Pause() const;
        void Resume() const;

        void SetName(const std::string &name);
        void SetPan(float pan) const;
        void SetVolume(float volume) const;
        void SetLoop(bool enable);
        void SetPitch(float pitch) const;
        void SetMode(FMOD_MODE mode);
        void SetFadeIn(uint32_t fade_in_start_ms, uint32_t fade_in_end_ms);
        void SetFadeOut(uint32_t fade_out_start_ms, uint32_t fade_out_end_ms);
        void AddToChannelGroup(FMOD::ChannelGroup *channel_group);

        void Release();

        float GetPitch() const;
        float GetVolume() const;

        void Update(float delta_time) const;
        void AddDsp(FMOD::DSP* dsp);

        FMOD::Sound* GetFmodSound() const;
        FMOD::Channel* GetFmodChannel() const;
        const FMOD_MODE &GetMode();
        const std::string &GetName() const;
        bool IsPlaying() const;
        bool IsPaused() const;
        bool IsLooping() const;
        uint32_t GetLengthMs() const;
        uint32_t GetPositionMs() const;
        FMOD::ChannelGroup *GetChannelGroup() const;

        static Ref<FmodSound> Create(const std::string &name, const std::string &filepath, FMOD_MODE mode = FMOD_DEFAULT | FMOD_LOOP_OFF);
        static Ref<FmodSound> CreateStream(const std::string &name, const std::string &filepath, FMOD_MODE mode = FMOD_DEFAULT | FMOD_LOOP_OFF);
    
        static AssetType GetStaticType() { return AssetType::Audio; }
        AssetType GetAssetType() override { return GetStaticType(); }

    private:
        void UpdateFading() const;
    
        FMOD::Sound *m_Sound;
        FMOD::Channel *m_Channel;
        FMOD_MODE m_Mode;

        std::string m_Name;

        uint32_t m_FadeInStartMs;
        uint32_t m_FadeInEndMs;

        uint32_t m_FadeOutStartMs;
        uint32_t m_FadeOutEndMs;

        FMOD::ChannelGroup *m_ChannelGroup;
        std::vector<FMOD::DSP *> m_DSPs;
    };

}
