/* MIT License
* 
* Copyright (c) 2026 Evangelion Manuhutu
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
#include "ignite_pch.hpp"

#include "fmod_sound.hpp"
#include "fmod_audio.hpp"

#include "ignite/core/logger.hpp"

namespace ignite
{

    FmodSound::FmodSound(const std::string &name, FMOD_MODE mode)
        : m_Sound(nullptr), m_Channel(nullptr), m_Mode(mode),
        m_Name(name), m_FadeInStartMs(0), m_FadeInEndMs(0),
        m_FadeOutStartMs(0), m_FadeOutEndMs(0), m_ChannelGroup(nullptr)
    { }

	FmodSound::~FmodSound()
	{
        Release();
	}

	void FmodSound::Play()
    {
        const FMOD_RESULT result = FmodAudio::GetFmodSystem()->playSound(
            m_Sound,
            m_ChannelGroup ? m_ChannelGroup : FmodAudio::GetMasterChannel(),
            false, // start paused
            &m_Channel
        );

        FMOD_CHECK(result);

        // remove dsp
        int dsp_count = 0;
        m_Channel->getNumDSPs((&dsp_count));
        for (int i = 0; i < dsp_count; i++)
        {
            FMOD::DSP *dsp;
            m_Channel->getDSP(i, &dsp);
            m_Channel->removeDSP(dsp);
        }

        // re-add dsp
        for (const auto &dsp : m_DSPs)
        {
            m_Channel->addDSP(0, dsp);
        }
    }

    void FmodSound::Stop() const
    {
        m_Channel->stop();
    }

    void FmodSound::Pause() const
    {
        m_Channel->setPaused(true);
    }

    void FmodSound::Resume() const
    {
        m_Channel->setPaused(false);
    }

    void FmodSound::SetName(const std::string &name)
    {
        m_Name = name;
    }

    void FmodSound::SetPan(const float pan) const
    {
        m_Channel->setPan(pan);
    }

    void FmodSound::SetVolume(float volume) const
    {
        volume = std::clamp(volume, 0.0f, 1.0f);
        m_Channel->setVolume(volume);
    }

    void FmodSound::SetPitch(const float pitch) const
    {
        m_Channel->setPitch(pitch);
    }

    void FmodSound::SetMode(const FMOD_MODE mode)
    {
        m_Mode = mode;
        m_Channel->setMode(m_Mode);
    }

    void FmodSound::SetFadeIn(uint32_t fade_in_start_ms, uint32_t fade_in_end_ms)
    {
        m_FadeInStartMs = fade_in_start_ms;
        m_FadeInEndMs = fade_in_end_ms;
    }

    void FmodSound::SetFadeOut(uint32_t fade_out_start_ms, uint32_t fade_out_end_ms)
    {
        m_FadeOutStartMs = fade_out_start_ms;
        m_FadeOutEndMs = fade_out_end_ms;
    }

    void FmodSound::AddToChannelGroup(FMOD::ChannelGroup *channel_group)
    {
        m_ChannelGroup = channel_group;
    }

    void FmodSound::Release()
    {
        if (m_Channel)
        {
            m_Channel->stop();
            m_Channel = nullptr;
        }

        ClearDsps(true);

        if (m_Sound)
        {
            m_Sound->release();
            m_Sound = nullptr;
        }
    }

    float FmodSound::GetPitch() const
    {
        float pitch;
        m_Channel->getPitch(&pitch);
        return pitch;
    }

    float FmodSound::GetVolume() const
    {
        float volume;
        m_Channel->getVolume(&volume);
        return volume;
    }

    void FmodSound::Update([[maybe_unused]] float delta_time) const
    {
        UpdateFading();
    }

    void FmodSound::AddDsp(FMOD::DSP *dsp)
    {
        if (!dsp)
        {
            return;
        }

        m_DSPs.push_back(dsp);
    }

    void FmodSound::ClearDsps(const bool release)
    {
        if (release)
        {
            for (auto &dsp : m_DSPs)
            {
                if (dsp)
                {
                    dsp->release();
                }
            }
        }

        m_DSPs.clear();
    }

    FMOD::Sound *FmodSound::GetFmodSound() const
    {
        return m_Sound;
    }

    FMOD::Channel *FmodSound::GetFmodChannel() const
    {
        return m_Channel;
    }

    const FMOD_MODE &FmodSound::GetMode()
    {
        m_Channel->getMode(&m_Mode);
        return m_Mode;
    }

    const std::string &FmodSound::GetName() const
    {
        return m_Name;
    }

    bool FmodSound::IsPlaying() const
    {
        bool is_playing = false;
        m_Channel->isPlaying(&is_playing);
        return is_playing;
    }

    bool FmodSound::IsPaused() const
    {
        bool paused = false;
        m_Channel->getPaused(&paused);
        return paused;
    }

    bool FmodSound::IsLooping() const
    {
        return ((m_Mode & FMOD_LOOP_NORMAL) == 0);
    }

    u32 FmodSound::GetLengthMs() const
    {
        u32 length;
        m_Sound->getLength(&length, FMOD_TIMEUNIT_MS);
        return length;
    }

    u32 FmodSound::GetPositionMs() const
    {
        u32 position;
        m_Channel->getPosition(&position, FMOD_TIMEUNIT_MS);
        return position;
    }

    FMOD::ChannelGroup *FmodSound::GetChannelGroup() const
    {
        return m_ChannelGroup;
    }

    Ref<FmodSound> FmodSound::Create(const std::string &name, const std::string &filepath, const FMOD_MODE mode)
    {
        Ref<FmodSound> fmod_sound = CreateRef<FmodSound>(name, mode);
        FmodAudio::GetFmodSystem()->createSound(filepath.c_str(), mode, nullptr, &fmod_sound->m_Sound);

        LOG_WARN("[FMOD Sound] Load sound '{}'", filepath);

        return fmod_sound;
    }

    Ref<FmodSound> FmodSound::CreateStream(const std::string &name, const std::string &filepath, const FMOD_MODE mode)
    {
        Ref<FmodSound> fmod_sound = CreateRef<FmodSound>(name, mode);
        FmodAudio::GetFmodSystem()->createStream(filepath.c_str(), mode, nullptr, &fmod_sound->m_Sound);
        return fmod_sound;
    }

    void FmodSound::UpdateFading() const
    {
        u32 position = 0;

        m_Channel->getPosition(&position, FMOD_TIMEUNIT_MS);
        float volume = GetVolume();
        if (position >= m_FadeInStartMs && position <= m_FadeInEndMs)
        {
            const float fade_progress = static_cast<float>(position - m_FadeInStartMs) / static_cast<float>(m_FadeInEndMs - m_FadeInStartMs);
            volume = fade_progress;
        }

        if (position >= m_FadeOutStartMs && position <= m_FadeOutEndMs)
        {
            const float fade_progress = static_cast<float>(position - m_FadeOutStartMs) / static_cast<float>(m_FadeOutEndMs - m_FadeOutStartMs);
            volume = 1.0f - fade_progress;
        }

        SetVolume(volume);
    }

}
