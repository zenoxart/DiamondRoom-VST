#include "OneKnobDriver.h"

namespace dr
{

namespace
{
    // How far the level matcher is allowed to correct, and how fast it moves.
    constexpr float minAutoGain = 0.02f;
    constexpr float maxAutoGain = 40.0f;
    constexpr float energyTimeMs = 120.0f;
    constexpr float gainTimeMs = 40.0f;

    // Below this the input is treated as silence and the correction is frozen,
    // so a gap in the material does not wind the gain up.
    constexpr float silenceEnergy = 1.0e-9f;
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
    biasDepth.reset (oversampledRate, 0.02);
    blend.reset (oversampledRate, 0.02);

    energyCoeff = 1.0f - std::exp (-1.0f / (float) (energyTimeMs * 0.001 * oversampledRate));
    gainCoeff   = 1.0f - std::exp (-1.0f / (float) (gainTimeMs * 0.001 * oversampledRate));

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

    dryEnergy = wetEnergy = 0.0f;
    autoGain = 1.0f;
}

void OneKnobDriver::updateToneFilters (float t)
{
    if (juce::approximatelyEqual (t, lastToneAmount))
        return;

    lastToneAmount = t;

    // Driving harder makes the stage darker, not brighter: the added harmonics
    // sit under a top end that is progressively shelved and rolled off. The
    // level matcher puts back the energy this takes away.
    const auto shelfDb = -14.0f * t;
    const auto cutoffHz = 20000.0f - 14000.0f * t;

    for (auto& ch : channels)
    {
        ch.toneShelf.set (SvfFilter::highShelf, 5000.0f, oversampledRate, 0.707f, shelfDb);
        ch.toneLowpass.set (SvfFilter::lowpass,
                            juce::jmin (cutoffHz, (float) oversampledRate * 0.45f),
                            oversampledRate, 0.707f);
    }
}

void OneKnobDriver::setDrive (float amount) noexcept
{
    const auto t = juce::jlimit (0.0f, 10.0f, amount) * 0.1f;

    // Up to +48 dB into the shaper. At the top of the dial an ordinary signal
    // is driven far past the bend, which is the aggressive breakup wanted here.
    driveGain.setTargetValue (dbToGain (48.0f * std::pow (t, 1.1f)));

    // The negative half is shaped with less curvature than the positive one, so
    // it saturates later and further. Square-rooted so the second harmonic is
    // already audible at low settings, where the curve barely saturates.
    asymmetry.setTargetValue (1.0f - 0.30f * std::sqrt (t));

    // Offset as a fraction of the driven signal's own amplitude, so the duty
    // cycle shift stays put however far the gain is pushed.
    biasDepth.setTargetValue (0.22f * t);

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
        const auto wet = blend.getNextValue();

        // Bias tracks the running input level, so it holds its proportion to
        // the signal arriving at the shaper rather than being swallowed by it.
        const auto bias = biasDepth.getNextValue() * std::sqrt (dryEnergy) * g;

        std::array<float, 2> dry {}, shaped {};
        float drySquared = 0.0f, wetSquared = 0.0f;

        for (size_t c = 0; c < numChannels; ++c)
        {
            auto& ch = channels[c];
            auto* data = block.getChannelPointer (c);

            dry[c] = data[n];

            auto x = ch.dcBlock.process (dry[c]);
            x = ch.preTilt.process (x);

            // Asymmetric soft clip: both halves have the same slope through the
            // origin, but the negative one saturates later and reaches further.
            const auto driven = x * g + bias;
            auto y = driven >= 0.0f ? fastTanh (driven) : fastTanh (driven * k) / k;

            y = ch.postTilt.process (y);
            y = ch.toneShelf.process (y);
            y = ch.toneLowpass.process (y);
            y = ch.dcOut.processHighpass (y);

            shaped[c] = y;
            drySquared += dry[c] * dry[c];
            wetSquared += y * y;
        }

        // -- level matching ---------------------------------------------------
        dryEnergy = flushDenormal (dryEnergy + energyCoeff * (drySquared - dryEnergy));
        wetEnergy = flushDenormal (wetEnergy + energyCoeff * (wetSquared - wetEnergy));

        if (dryEnergy > silenceEnergy)
        {
            const auto target = juce::jlimit (minAutoGain, maxAutoGain,
                                              std::sqrt (dryEnergy / juce::jmax (wetEnergy, 1.0e-12f)));
            autoGain += gainCoeff * (target - autoGain);
        }

        for (size_t c = 0; c < numChannels; ++c)
            block.getChannelPointer (c)[n] = dry[c] + wet * (shaped[c] * autoGain - dry[c]);
    }
}

} // namespace dr
