#include "OneKnobDriver.h"

namespace dr
{

void OneKnobDriver::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, oversampleFactor,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler->initProcessing ((size_t) spec.maximumBlockSize);

    const auto osRate = sampleRate * (1 << oversampleFactor);

    for (auto& ch : channels)
    {
        // 30 Hz high pass keeps the clipper from smearing the low end, and a
        // gentle mid emphasis before it is what gives the Driver its bite.
        ch.dcBlock.set  (SvfFilter::highpass,  30.0f,  osRate, 0.707f);
        ch.preTilt.set  (SvfFilter::peaking,   1200.0f, osRate, 0.6f,  4.5f);
        ch.postTilt.set (SvfFilter::peaking,   1200.0f, osRate, 0.6f, -3.0f);
        ch.toneShelf.set (SvfFilter::highShelf, 6500.0f, osRate, 0.707f, -2.5f);
        ch.bias.setCutoff (12.0f, osRate);
    }

    driveGain.reset (osRate, 0.02);
    makeUp.reset (osRate, 0.02);
    blend.reset (osRate, 0.02);

    reset();
}

void OneKnobDriver::reset()
{
    if (oversampler != nullptr)
        oversampler->reset();

    for (auto& ch : channels)
    {
        ch.dcBlock.reset();
        ch.preTilt.reset();
        ch.postTilt.reset();
        ch.toneShelf.reset();
        ch.bias.reset();
    }
}

void OneKnobDriver::setDrive (float amount) noexcept
{
    const auto t = juce::jlimit (0.0f, 10.0f, amount) * 0.1f;

    // 0 dB .. +30 dB into the shaper.
    const auto gainDb = 30.0f * t * t + 6.0f * t;
    driveGain.setTargetValue (dbToGain (gainDb));

    // Compensate most, but not all, of the added gain: the stage should still
    // push the reverbs a little harder as you turn it up.
    makeUp.setTargetValue (dbToGain (-gainDb * 0.82f));

    // Below about 1 on the dial the stage is essentially clean.
    blend.setTargetValue (juce::jlimit (0.0f, 1.0f, t * 4.0f));
}

void OneKnobDriver::process (juce::dsp::AudioBlock<float> block)
{
    if (oversampler == nullptr)
        return;

    auto upsampled = oversampler->processSamplesUp (block);
    processOversampled (upsampled);
    oversampler->processSamplesDown (block);
}

void OneKnobDriver::processOversampled (juce::dsp::AudioBlock<float>& block)
{
    const auto numChannels = juce::jmin ((size_t) 2, block.getNumChannels());
    const auto numSamples  = block.getNumSamples();

    for (size_t n = 0; n < numSamples; ++n)
    {
        const auto g   = driveGain.getNextValue();
        const auto mu  = makeUp.getNextValue();
        const auto wet = blend.getNextValue();

        for (size_t c = 0; c < numChannels; ++c)
        {
            auto& ch = channels[c];
            auto* data = block.getChannelPointer (c);

            const auto dry = data[n];

            auto x = ch.dcBlock.process (dry);
            x = ch.preTilt.process (x) * g;

            // Asymmetric transfer curve: the squared term adds the even
            // harmonics that make the stage read as "tube" rather than "fuzz".
            const auto shaped = fastTanh (x) + 0.16f * fastTanh (x * x * 0.5f);

            // Remove the DC the asymmetry introduces.
            auto y = shaped - ch.bias.processLowpass (shaped);

            y = ch.postTilt.process (y);
            y = ch.toneShelf.process (y) * mu;

            data[n] = dry + wet * (y - dry);
        }
    }
}

} // namespace dr
