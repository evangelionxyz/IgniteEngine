namespace Ignite;

public abstract class FmodDsp
{
    internal abstract bool Apply(ulong entityID);
}

public sealed class FmodReverb : FmodDsp
{
    public float DecayTime { get; set; } = 1500.0f;
    public float EarlyDelay { get; set; } = 20.0f;
    public float LateDelay { get; set; } = 40.0f;
    public float HighFrequencyReference { get; set; } = 5000.0f;
    public float Diffusion { get; set; } = 50.0f;
    public float Density { get; set; } = 50.0f;
    public float LowShelfGain { get; set; } = 250.0f;
    public float HighCut { get; set; } = 20000.0f;
    public float DryLevel { get; set; } = 0.0f;
    public float WetLevel { get; set; } = -6.0f;

    public void setMix(float wetLevel) => WetLevel = wetLevel;
    public void SetMix(float wetLevel) => WetLevel = wetLevel;

    internal override bool Apply(ulong entityID)
    {
        return InternalCalls.AudioSourceComponent_AddReverbDSP(entityID, DecayTime, EarlyDelay, LateDelay, HighFrequencyReference, Diffusion, Density, LowShelfGain, HighCut, DryLevel, WetLevel);
    }
}

public sealed class FmodDistortion : FmodDsp
{
    public float DistortionLevel { get; set; } = 0.5f;

    internal override bool Apply(ulong entityID)
    {
        return InternalCalls.AudioSourceComponent_AddDistortionDSP(entityID, DistortionLevel);
    }
}

public sealed class FmodChorus : FmodDsp
{
    public float Mix { get; set; } = 50.0f;
    public float Rate { get; set; } = 0.8f;
    public float Depth { get; set; } = 3.0f;

    public void setMix(float value) => Mix = value;
    public void SetMix(float value) => Mix = value;

    internal override bool Apply(ulong entityID)
    {
        return InternalCalls.AudioSourceComponent_AddChorusDSP(entityID, Mix, Rate, Depth);
    }
}

public sealed class FmodCompressor : FmodDsp
{
    public float Threshold { get; set; } = 0.0f;
    public float Ratio { get; set; } = 2.5f;
    public float Release { get; set; } = 100.0f;
    public float GainMakeup { get; set; } = 0.0f;
    public bool UseSidechain { get; set; } = false;

    internal override bool Apply(ulong entityID)
    {
        return InternalCalls.AudioSourceComponent_AddCompressorDSP(entityID, Threshold, Ratio, Release, GainMakeup, UseSidechain);
    }
}

public sealed class FmodDelay : FmodDsp
{
    public float DelayMs { get; set; } = 250.0f;
    public float Feedback { get; set; } = 20.0f;

    internal override bool Apply(ulong entityID)
    {
        return InternalCalls.AudioSourceComponent_AddDelayDSP(entityID, DelayMs, Feedback);
    }
}
