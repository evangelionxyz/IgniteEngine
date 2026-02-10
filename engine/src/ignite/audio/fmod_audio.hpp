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
#include <fmod_common.h>
#include <fmod_errors.h>

#include <string>
#include <unordered_map>

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/logger.hpp"

#define FMOD_CHECK(x) do { FMOD_RESULT _fmod_result = (x); if (_fmod_result != FMOD_OK) { LOG_ERROR("[FMOD] {}", FMOD_ErrorString(_fmod_result)); DEBUGBREAK(); } } while (false)

namespace ignite
{
    struct FmodSound;
    
    class FmodAudio
    {
    public:
        static void Init();
        static void Shutdown();

        static void SetMasterVolume(float volume);
        static void MuteMaster(bool mute);

        static void Update(float deltaTime);

        static FMOD::ChannelGroup *CreateChannelGroup(const std::string &name);
        static std::unordered_map<std::string, FMOD::ChannelGroup *> GetChannelGroupMap();
        
        static FMOD::ChannelGroup *GetChannelGroup(const std::string &name);
        static FmodAudio &GetInstance();
        static FMOD::System *GetFmodSystem();
        static FMOD::ChannelGroup *GetMasterChannel();
        static float GetMasterVolume();
        static void InsertFmodSound(const std::string &name, const Ref<FmodSound>& sound);
        static void RemoveFmodSound(const std::string &name);

    private:
        FMOD::System *m_System;
        FMOD::ChannelGroup *m_MasterGroup;
        std::unordered_map<std::string, FMOD::ChannelGroup *> m_ChannelGroups;
        std::unordered_map<std::string, Ref<FmodSound>> m_SoundMap;

        FMOD_VECTOR listenerPos;
        FMOD_VECTOR listenerVel;
        FMOD_VECTOR listenerForward;
        FMOD_VECTOR listenerUp;
        
    };
}
