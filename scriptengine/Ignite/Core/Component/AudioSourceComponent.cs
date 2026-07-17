// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
namespace Ignite;

public class AudioSourceComponent : IComponent
{
    public bool HasAudio => ComponentInternalCalls.AudioSourceComponent_HasAudio(Entity!.ID);

    public float Volume
    {
        get
        {
            ComponentInternalCalls.AudioSourceComponent_GetVolume(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.AudioSourceComponent_SetVolume(Entity!.ID, value);
    }

    public float Pitch
    {
        get
        {
            ComponentInternalCalls.AudioSourceComponent_GetPitch(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.AudioSourceComponent_SetPitch(Entity!.ID, value);
    }

    public float Pan
    {
        get
        {
            ComponentInternalCalls.AudioSourceComponent_GetPan(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.AudioSourceComponent_SetPan(Entity!.ID, value);
    }

    public bool PlayAtStart
    {
        get
        {
            ComponentInternalCalls.AudioSourceComponent_GetPlayOnStart(Entity!.ID, out bool result);
            return result;
        }
        set => ComponentInternalCalls.AudioSourceComponent_SetPlayOnStart(Entity!.ID, value);
    }

    public bool Loop
    {
        get
        {
            ComponentInternalCalls.AudioSourceComponent_GetLoop(Entity!.ID, out bool result);
            return result;
        }
        set => ComponentInternalCalls.AudioSourceComponent_SetLoop(Entity!.ID, value);
    }

    public void Play() => ComponentInternalCalls.AudioSourceComponent_Play(Entity!.ID);
    public void Stop() => ComponentInternalCalls.AudioSourceComponent_Stop(Entity!.ID);
    public void Pause() => ComponentInternalCalls.AudioSourceComponent_Pause(Entity!.ID);
    public void Resume() => ComponentInternalCalls.AudioSourceComponent_Resume(Entity!.ID);
    public void ClearDSPs() => ComponentInternalCalls.AudioSourceComponent_ClearDSPs(Entity!.ID);

    public bool AddDSP(FmodDsp dsp)
    {
        if (dsp == null)
            return false;

        return dsp.Apply(Entity!.ID);
    }

    public bool addDSP(FmodDsp dsp)
    {
        return AddDSP(dsp);
    }
}
