#include "OneKnobDriver.h"

namespace dr
{

namespace
{
    // The level the output is normalised against. Roughly a signal sitting at
    // -12 dBFS, which is where the stage is expected to be driven from.
    constexpr float nominalLevel = 0.20f;
}

void OneKnobDriver::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, oversampleFactor,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler->initProcessing ((size_t) spec.maximumBlockSize);

    oversampledRate = sampleRate * (1 << oversampleFactor);

    for (auto& ch : channels)
    {
        // 30 Hz high pass keeps the clipper from smearing the low end, and a
        // mid emphasis before it is what gives the Driver its bite.
        ch.dcBlock.set  (SvfFilter::highpass, 30.0f,   oversampledRate, 0.707f);
        ch.preTilt.set  (SvfFilter::peaking,  1200.0f, oversampledRate, 0.6f,  4.5f);
        ch.postTilt.set (SvfFilter::peaking,  1200.0f, oversampledRate, 0.6f, -3.0f);
        ch.dcOut.setCutoff (12.0f, oversampledRate);
    }

    driveGain.reset (oversampledRate, 0.02);
    asymmetry.reset (oversampledRate, 0.02);
    makeUp.reset (oversampledRate, 0.02);
    blend.reset (oversampledRate, 0.02);

    lastToneAmount = -1.0f;
    updateToneFilters (0.0f);

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
        ch.toneLowpass.reset();
        ch.dcOut.reset();
    }
}

void OneKnobDriver::updateToneFilters (float t)
{
    if (juce::approximatelyEqual (t, lastToneAmount))
        return;

    lastToneAmount = t;

    // Driving harder makes the stage darker, not brighter: the added
    // harmonics sit under a top end that is progressively shelved and rolled
    // off. This is what stops it sounding like a fuzz box.
    const auto shelfDb = -12.0f * t;
    const auto cutoffHz = 20000.0f - 13000.0f * t;

    for (auto& ch : channels)
    {
        ch.toneShelf.set (SvfFilter::highShelf, 5500.0f, oversampledRate, 0.707f, shelfDb);
        ch.toneLowpass.set (SvfFilter::lowpass,
                            juce::jmin (cutoffHz, (float) oversampledRate * 0.45f),
                            oversampledRate, 0.707f);
    }
}

void OneKnobDriver::setDrive (float amount) noexcept
{
    const auto t = juce::jlimit (0.0f, 10.0f, amount) * 0.1f;

    // Enough gain that an ordinary signal is well past the bend in the curve.
    const auto gain = dbToGain (36.0f * std::pow (t, 1.2f));
    driveGain.setTargetValue (gain);

    // The negative half is shaped with less curvature than the positive one,
    // so it saturates later and further. That is what produces the even
    // harmonics, and because it is a property of the curve rather than an
    // offset it survives however hard the stage is driven.
    // Square-rooted so the second harmonic is already audible at low settings,
    // where the curve is barely saturating and a linear ramp gives nothing.
    asymmetry.setTargetValue (1.0f - 0.30f * std::sqrt (t));

    // Normalise on the shaper's response to a nominal level. Compensating on
    // the input gain instead would collapse the level once the curve
    // saturates, because the peaks stop growing with it.
    const auto shapedNominal = std::tanh (nominalLevel * gain);
    makeUp.setTargetValue (nominalLevel / juce::jmax (1.0e-6f, shapedNominal));

    // Fully engaged by about 2 on the dial; below that it stays clean.
    blend.setTargetValue (juce::jlimit (0.0f, 1.0f, t * 5.0f));

    updateToneFilters (t);
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
        const auto k   = asymmetry.getNextValue();
        const auto mu  = makeUp.getNextValue();
        const auto wet = blend.getNextValue();

        for (size_t c = 0; c < numChannels; ++c)
        {
            auto& ch = channels[c];
            auto* data = block.getChannelPointer (c);

            const auto dry = data[n];

            auto x = ch.dcBlock.process (dry);
            x = ch.preTilt.process (x);

            // Asymmetric soft clip: both halves have the same slope through
            // the origin, but the negative one saturates later and reaches
            // further. The offset that leaves behind is taken out by the DC
            // blocker below.
            const auto driven = x * g;
            const auto shaped = driven >= 0.0f ? fastTanh (driven)
                                               : fastTanh (driven * k) / k;
            auto y = shaped * mu;

            y = ch.postTilt.process (y);
            y = ch.toneShelf.process (y);
            y = ch.toneLowpass.process (y);
            y = ch.dcOut.processHighpass (y);

            data[n] = dry + wet * (y - dry);
        }
    }
}

} // namespace dr
