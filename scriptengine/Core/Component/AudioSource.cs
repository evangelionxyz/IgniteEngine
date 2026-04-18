namespace Ignite;

public class AudioSource : IComponent
{
    public bool HasAudio => InternalCalls.AudioSourceComponent_HasAudio(Entity.ID);

    public float Volume
    {
        get
        {
            InternalCalls.AudioSourceComponent_GetVolume(Entity.ID, out float result);
            return result;
        }
        set => InternalCalls.AudioSourceComponent_SetVolume(Entity.ID, value);
    }

    public float Pitch
    {
        get
        {
            InternalCalls.AudioSourceComponent_GetPitch(Entity.ID, out float result);
            return result;
        }
        set => InternalCalls.AudioSourceComponent_SetPitch(Entity.ID, value);
    }

    public float Pan
    {
        get
        {
            InternalCalls.AudioSourceComponent_GetPan(Entity.ID, out float result);
            return result;
        }
        set => InternalCalls.AudioSourceComponent_SetPan(Entity.ID, value);
    }

    public bool PlayAtStart
    {
        get
        {
            InternalCalls.AudioSourceComponent_GetPlayOnStart(Entity.ID, out bool result);
            return result;
        }
        set => InternalCalls.AudioSourceComponent_SetPlayOnStart(Entity.ID, value);
    }

    public bool Loop
    {
        get
        {
            InternalCalls.AudioSourceComponent_GetLoop(Entity.ID, out bool result);
            return result;
        }
        set => InternalCalls.AudioSourceComponent_SetLoop(Entity.ID, value);
    }

    public void Play() => InternalCalls.AudioSourceComponent_Play(Entity.ID);
    public void Stop() => InternalCalls.AudioSourceComponent_Stop(Entity.ID);
    public void Pause() => InternalCalls.AudioSourceComponent_Pause(Entity.ID);
    public void Resume() => InternalCalls.AudioSourceComponent_Resume(Entity.ID);

    public void ClearDSPs() => InternalCalls.AudioSourceComponent_ClearDSPs(Entity.ID);

    public bool AddDSP(FmodDsp dsp)
    {
        if (dsp == null)
            return false;

        return dsp.Apply(Entity.ID);
    }

    public bool addDSP(FmodDsp dsp)
    {
        return AddDSP(dsp);
    }
}
