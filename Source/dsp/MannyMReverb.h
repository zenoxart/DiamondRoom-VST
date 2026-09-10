#pragma once

#include "DspUtils.h"

namespace dr
{

/**
    "Dave Aron - Rock Vocal Hall" chamber, after the MannyM Reverb node: a dense
    medium chamber with the preset's scooped mids and lifted highs, a phaser
    stirring the tail, a compressor on the output, and the Distortion stage
    folded into the feedback path where it grits up the decay rather than the
    transient.
*/
class MannyMReverb
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** @param amount  0..10 dial; density and level of the chamber. */
    void setAmount (float amount) noexcept;

    /** @param distortion  0..10 dial; grit inside the feedback path. */
    void setDistortion (float distortion) noexcept;

    void process (float inL, float inR, float& outL, float& outR) noexcept;

private:
    static constexpr int numLines = 4;

    struct Line
    {
        DelayLine delay;
        ModAllpass mod;
        OnePole damp;
        Lfo lfo;
        float length = 0.0f;
        float feedback = 0.0f;
    };

    struct Side
    {
        std::array<Allpass, 4> diffusion;
        DelayLine preDelay;
        SvfFilter lowShelf, midBell, highShelf, lowCut;
    };

    std::array<Line, numLines> lines;
    std::array<Side, 2> sides;

    // Phaser stage from the preset's Rate / Phaser controls.
    std::array<std::array<SvfFilter, 4>, 2> phaserStages;
    Lfo phaserLfo;

    float compEnv = 0.0f;
    float compAttack = 0.0f, compRelease = 0.0f;

    double sampleRate = 44100.0;
    float amountGain = 0.7f;
    float driveAmount = 0.0f;
    float driveTrim = 1.0f;
};

} // namespace dr
