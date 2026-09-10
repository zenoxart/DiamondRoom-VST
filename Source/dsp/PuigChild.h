#pragma once

#include "DspUtils.h"

namespace dr
{

/**
    The PuigChild 670 that closes the patch, driven by the Tube dial: a vari-mu
    compressor with the preset's linked lateral/vertical detection and time
    constant 4 (0.4 ms attack, two-stage program dependent release), wrapped in
    the tube and transformer colouration that the stage is really there for.

    Turning Tube up pushes more level into the tubes, so it compresses harder
    and saturates more at the same time - one dial, both effects.
*/
class PuigChild
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** @param amount  0..10, matching the Tube knob. */
    void setAmount (float amount) noexcept;

    void process (juce::dsp::AudioBlock<float> block);

    /** Latency the internal oversampler adds, in samples. */
    float getLatencySamples() const noexcept
    {
        return oversampler != nullptr ? (float) oversampler->getLatencyInSamples() : 0.0f;
    }

    /** Current gain reduction in dB, for metering. */
    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }

private:
    void processOversampled (juce::dsp::AudioBlock<float>& block);

    struct Channel
    {
        SvfFilter transformerLow, transformerHigh, dcBlock;
        OnePole bias;
    };

    std::array<Channel, 2> channels;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    // Two-stage release, as in the real time-constant network.
    float envFast = 0.0f, envSlow = 0.0f;
    float attackCoeff = 0.0f, releaseFast = 0.0f, releaseSlow = 0.0f;
    float smoothedGain = 1.0f, gainSmoothCoeff = 0.0f;

    juce::SmoothedValue<float> inputGain  { 1.0f };
    juce::SmoothedValue<float> outputGain { 1.0f };
    juce::SmoothedValue<float> saturation { 0.0f };

    std::atomic<float> gainReductionDb { 0.0f };

    double sampleRate = 44100.0;
    static constexpr int oversampleFactor = 1; // 2x
};

} // namespace dr
