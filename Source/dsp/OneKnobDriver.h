#pragma once

#include "DspUtils.h"

namespace dr
{

/**
    The Drive stage that opens the patch, after OneKnob Driver: a warm valve
    overdrive that gets thicker and darker as it is turned up.

    Two things make it read as "driven" rather than just loud. The transfer
    curve is deliberately asymmetric - the two halves of the waveform are
    shaped with different curvature, so the stage produces even harmonics
    alongside the odd ones the curve gives on its own. Asymmetry belongs in the
    shape rather than in a bias offset: an offset is swamped once the gain is
    high, and the even harmonics fade away exactly where they should be
    strongest. And the top end is rolled off progressively with the setting, so
    more drive means a duller, heavier tone rather than a brighter one.

    The output is normalised against the shaper's own response rather than
    against the input gain: once the curve is saturating, the peaks stop
    growing, so compensating on gain alone would make the stage quieter the
    harder it is pushed. Runs 4x oversampled.
*/
class OneKnobDriver
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** @param amount  0..10, matching the Drive knob. */
    void setDrive (float amount) noexcept;

    void process (juce::dsp::AudioBlock<float> block);

    /** Latency the internal oversampler adds, in samples. */
    float getLatencySamples() const noexcept
    {
        return oversampler != nullptr ? (float) oversampler->getLatencyInSamples() : 0.0f;
    }

private:
    void processOversampled (juce::dsp::AudioBlock<float>& block);
    void updateToneFilters (float t);

    struct Channel
    {
        SvfFilter dcBlock, preTilt, postTilt, toneShelf, toneLowpass;
        OnePole dcOut;
    };

    std::array<Channel, 2> channels;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::SmoothedValue<float> driveGain { 1.0f };
    juce::SmoothedValue<float> asymmetry { 1.0f };
    juce::SmoothedValue<float> makeUp     { 1.0f };
    juce::SmoothedValue<float> blend      { 0.0f };

    double sampleRate = 44100.0;
    double oversampledRate = 176400.0;
    float lastToneAmount = -1.0f;

    static constexpr int oversampleFactor = 2; // 2^2 = 4x
};

} // namespace dr
