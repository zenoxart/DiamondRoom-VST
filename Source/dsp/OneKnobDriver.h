#pragma once

#include "DspUtils.h"

namespace dr
{

/**
    Emulation of the OneKnob Driver stage that opens the patch: a warm,
    asymmetric tube-style overdrive with pre/de-emphasis around the clipper and
    automatic output compensation, so turning Drive up changes the tone rather
    than just the level. Runs 4x oversampled to keep the harmonics clean.
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

    struct Channel
    {
        SvfFilter dcBlock, preTilt, postTilt, toneShelf;
        OnePole bias;
    };

    std::array<Channel, 2> channels;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::SmoothedValue<float> driveGain { 1.0f };
    juce::SmoothedValue<float> makeUp    { 1.0f };
    juce::SmoothedValue<float> blend     { 0.0f };

    double sampleRate = 44100.0;
    static constexpr int oversampleFactor = 2; // 2^2 = 4x
};

} // namespace dr
