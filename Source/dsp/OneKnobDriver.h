#pragma once

#include "DspUtils.h"

namespace dr
{

/**
    The Drive stage that opens the patch, after OneKnob Driver: a valve
    overdrive that gets more aggressive and darker as it is turned up, without
    getting quieter.

    The transfer curve is deliberately asymmetric, in two ways that cover
    different parts of the dial. The two halves of the waveform are shaped with
    different curvature, which colours the gentler settings. And the curve is
    offset by a bias proportional to the driven signal's own level, which shifts
    the zero crossings and so changes the duty cycle of the waveform once it is
    clipping hard.

    Both are needed. Curvature alone produces nothing even at the top of the
    dial: when both halves saturate fully, unequal amplitudes are just a
    symmetric square wave plus DC, and the DC blocker takes the asymmetry
    straight back out. Only a duty cycle change survives that, and a fixed
    offset cannot deliver one because it is swamped as the gain rises - it has
    to scale with the signal hitting the shaper.

    Level is held by matching the output's running RMS to the input's, rather
    than by compensating for the input gain. Compensating on gain alone fails
    twice over: the peaks stop growing once the curve saturates, and the top end
    is being shelved and rolled off at the same time, which on real material
    takes away far more energy than the shaper adds. Measuring the result is the
    only compensation that survives both. Runs 4x oversampled.
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
    juce::SmoothedValue<float> biasDepth { 0.0f };
    juce::SmoothedValue<float> blend     { 0.0f };

    // Running mean square of what goes in and what comes out, and the
    // correction derived from the two.
    float dryEnergy = 0.0f, wetEnergy = 0.0f, autoGain = 1.0f;
    float energyCoeff = 0.0f, gainCoeff = 0.0f;

    double sampleRate = 44100.0;
    double oversampledRate = 176400.0;
    float lastToneAmount = -1.0f;

    static constexpr int oversampleFactor = 2; // 2^2 = 4x
};

} // namespace dr
