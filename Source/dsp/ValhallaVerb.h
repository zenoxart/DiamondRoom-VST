#pragma once

#include "DspUtils.h"

namespace dr
{

/**
    "VaViRb_Vox_9" Concert Hall in 1970s colour, after the ValhallaVintageVerb
    node: slow attack and low initial echo density that builds into a chorused
    tail, bass multiplied 1.5x below 780 Hz, a dark high shelf, and the vintage
    mode's ~10 kHz bandwidth limit with noisy modulation. The preset's 960 Hz
    low cut is what keeps this layer sitting above the other three.
*/
class ValhallaVerb
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** @param decay  0..10 dial; maps to roughly 0.3 s .. 9 s. */
    void setDecay (float decay) noexcept;

    /** @param highCut  0..10 dial; maps to 1 kHz .. 20 kHz. */
    void setHighCut (float highCut) noexcept;

    void process (float inL, float inR, float& outL, float& outR) noexcept;

private:
    void updateDecay();

    static constexpr int numLines = 8;

    struct Line
    {
        DelayLine delay;
        ModAllpass mod;
        OnePole hfDamp, bassDamp;
        Lfo lfo;
        float length = 0.0f;
        float modDepth = 0.0f;
        float gMid = 0.0f, gHigh = 0.0f, bassBoost = 0.0f;
        float noise = 0.0f;
    };

    struct Side
    {
        std::array<Allpass, 4> earlyDiffusion;
        DelayLine preDelay;
        SvfFilter bandLimit;   // 1970s downsampling character
        SvfFilter highShelf;
        SvfFilter highCut;
        SvfFilter lowCut;
    };

    std::array<Line, numLines> lines;
    std::array<Side, 2> sides;

    // Seeded, not clock-seeded: the vintage modes wobble with noise, and a
    // host bouncing the same material twice has to get the same render.
    juce::Random random { 0x5a17a11 };
    double sampleRate = 44100.0;
    float decaySeconds = 2.23f;
    float highCutHz = 16420.0f;
    bool highCutDirty = true;
};

} // namespace dr
