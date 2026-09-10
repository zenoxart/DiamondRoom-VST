#pragma once

#include "DspUtils.h"

namespace dr
{

/**
    "Focused Lead Vocal" style hybrid reverb, after the H-Reverb node in the
    patch: an early-reflection tap bank feeding an eight line FDN whose decay is
    frequency dependent (667 Hz / 2711 Hz crossovers, 1.24x high ratio), the
    preset's fixed reverb EQ, a soft tail compressor, and a tilt Tone control.
*/
class HReverb
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** @param time  0..10 dial; maps to roughly 0.25 s .. 10 s RT60. */
    void setTime (float time) noexcept;

    /** @param tone  -10..+10 dial; tilts the tail dark to bright. */
    void setTone (float tone) noexcept;

    void process (float inL, float inR, float& outL, float& outR) noexcept;

private:
    void updateDecay();
    void updateTone();

    static constexpr int numLines = 8;
    static constexpr int numEarly = 12;

    struct Line
    {
        DelayLine delay;
        Lfo lfo;
        OnePole hfDamp, lfDamp;
        float baseLength = 0.0f;   // samples
        float modDepth = 0.0f;     // samples
        float gMid = 0.0f, gHigh = 0.0f, gLowRatio = 0.0f;
    };

    struct Side
    {
        std::array<Allpass, 4> diffusion;
        DelayLine earlyLine;
        SvfFilter earlyFilter;
        SvfFilter eqA, eqB, eqC;
        SvfFilter toneLow, toneHigh;
        SvfFilter lowCut;
    };

    std::array<Line, numLines> lines;
    std::array<Side, 2> sides;
    std::array<std::pair<float, float>, numEarly> earlyTaps; // ms, gain

    // Tail compressor (the preset runs the Dynamics section in Comp mode).
    float compEnv = 0.0f;
    float compAttack = 0.0f, compRelease = 0.0f;

    double sampleRate = 44100.0;
    float rt60 = 3.24f;
    float toneAmount = 0.0f;
    float toneTrim = 1.0f;
};

} // namespace dr
