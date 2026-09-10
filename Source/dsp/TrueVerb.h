#pragma once

#include "DspUtils.h"

namespace dr
{

/**
    "Vocal spread" room simulator, after the TrueVerb node: a geometric early
    reflection bank whose spacing follows the room volume and the listening
    distance, feeding a short (0.5 s) damped tail with the preset's 4504 Hz
    absorption. Distance also sets the early/tail balance and the air
    absorption, which is what actually pushes the source back in the room.
*/
class TrueVerb
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** @param distance  0..10 dial; maps to roughly 0.5 m .. 30 m. */
    void setDistance (float distance) noexcept;

    /** @param roomsize  0..10 dial; maps to roughly 200 m3 .. 30000 m3. */
    void setRoomsize (float roomsize) noexcept;

    void process (float inL, float inR, float& outL, float& outR) noexcept;

private:
    void updateGeometry();

    static constexpr int numLines = 6;
    static constexpr int numEarly = 18;

    struct Line
    {
        DelayLine delay;
        OnePole hfDamp, lfDamp;
        float length = 0.0f;
        float gMid = 0.0f, gHigh = 0.0f, lowRatio = 1.0f;
    };

    struct Side
    {
        DelayLine earlyLine;
        DelayLine preDelay;
        std::array<Allpass, 3> diffusion;
        SvfFilter airAbsorb;
        SvfFilter erLowCut;
        SvfFilter revShelf;
    };

    std::array<Line, numLines> lines;
    std::array<Side, 2> sides;

    // Tap times in ms and signed gains, rebuilt when the geometry changes.
    std::array<float, numEarly> tapTimes {};
    std::array<float, numEarly> tapGains {};

    double sampleRate = 44100.0;
    float distanceMetres = 8.41f;
    float roomVolume = 9981.0f;
    float earlyGain = 0.67f;   // -3.4 dB in the preset
    float tailGain = 1.0f;
    bool geometryDirty = true;
};

} // namespace dr
