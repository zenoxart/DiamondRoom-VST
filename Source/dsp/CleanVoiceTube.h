#pragma once

#include "DspUtils.h"

namespace dr
{

/**
    The valve stage from CleanVoice, ported unchanged and put behind a Mix
    control.

    It is a vari-mu compressor rather than a plain saturator: a stereo linked
    detector with a high-passed sidechain drives a program dependent ratio that
    grows from 1.6:1 up towards 4:1 as the signal pushes past -20 dBFS, and the
    valve colour it adds tracks the gain reduction, so the harmonics arrive with
    the compression rather than on their own. Make-up is derived from the static
    curve at a nominal vocal level, which keeps the stage level-neutral without
    pumping.

    In CleanVoice this is a switch. Here the Tube dial is its Mix, so it can be
    blended in rather than only armed - at 0 the stage is bypassed exactly, at
    10 it is fully in circuit.
*/
class CleanVoiceTube
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** @param amount  0..10 Tube dial, used as a dry/wet mix. */
    void setMix (float amount) noexcept;

    void process (juce::dsp::AudioBlock<float> block);

    /** Current gain reduction in dB, for metering. */
    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }

private:
    inline float valveStage (float x, float colour) const noexcept;

    // CleanVoice's tube settings, verbatim.
    static constexpr float thresholdDb   = -20.0f;
    static constexpr float minRatio      = 1.6f;
    static constexpr float maxRatio      = 4.0f;
    static constexpr float ratioKneeDb   = 12.0f;
    static constexpr float kneeDb        = 16.0f;
    static constexpr float attackMs      = 0.2f;
    static constexpr float fastReleaseMs = 300.0f;
    static constexpr float slowReleaseMs = 800.0f;
    static constexpr float sidechainHpHz = 60.0f;
    static constexpr float harmonics     = 0.30f;
    static constexpr float transformerHz = 12000.0f;
    static constexpr float transformerDb = -1.0f;
    static constexpr float autoMakeup    = 1.0f;

    std::array<Biquad, 2> sidechainHp, transformer, dcBlocker;
    EnvelopeFollower fastEnv, slowEnv;

    juce::SmoothedValue<float> mix { 0.0f };

    float currentGain = 1.0f;
    float gainSmoothCoeff = 0.0f;
    float makeUp = 1.0f;

    std::atomic<float> gainReductionDb { 0.0f };

    double sampleRate = 44100.0;
};

} // namespace dr
