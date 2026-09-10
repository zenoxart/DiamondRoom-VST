#include "PuigChild.h"

namespace dr
{

namespace
{
    // Time constant 4 on the front panel.
    constexpr float attackMs      = 0.4f;
    constexpr float releaseFastMs = 500.0f;
    constexpr float releaseSlowMs = 5000.0f;

    constexpr float thresholdDb = -22.0f;
    constexpr float kneeDb      = 12.0f;
    constexpr float maxRatio    = 12.0f;   // vari-mu tightens as it is pushed
}

void PuigChild::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, oversampleFactor,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler->initProcessing ((size_t) spec.maximumBlockSize);

    const auto osRate = sampleRate * (1 << oversampleFactor);

    auto coeffFor = [osRate] (float ms)
    {
        return 1.0f - std::exp (-1.0f / (float) (ms * 0.001 * osRate));
    };

    attackCoeff = coeffFor (attackMs);
    releaseFast = coeffFor (releaseFastMs);
    releaseSlow = coeffFor (releaseSlowMs);
    gainSmoothCoeff = coeffFor (1.5f);

    for (auto& ch : channels)
    {
        // Input transformer: a little weight low, a gentle roll off on top.
        ch.transformerLow.set  (SvfFilter::lowShelf,   90.0f, osRate, 0.707f,  1.2f);
        ch.transformerHigh.set (SvfFilter::highShelf, 9000.0f, osRate, 0.707f, -1.5f);
        ch.dcBlock.set (SvfFilter::highpass, 12.0f, osRate, 0.707f);
        ch.bias.setCutoff (10.0f, osRate);
    }

    inputGain.reset (osRate, 0.02);
    outputGain.reset (osRate, 0.02);
    saturation.reset (osRate, 0.02);

    reset();
}

void PuigChild::reset()
{
    if (oversampler != nullptr)
        oversampler->reset();

    for (auto& ch : channels)
    {
        ch.transformerLow.reset();
        ch.transformerHigh.reset();
        ch.dcBlock.reset();
        ch.bias.reset();
    }

    envFast = envSlow = 0.0f;
    smoothedGain = 1.0f;
    gainReductionDb.store (0.0f);
}

void PuigChild::setAmount (float amount) noexcept
{
    const auto t = juce::jlimit (0.0f, 10.0f, amount) * 0.1f;

    // Up to +20 dB into the tubes, with most of it taken back on the way out.
    const auto driveDb = 20.0f * t;
    inputGain.setTargetValue (dbToGain (driveDb));
    outputGain.setTargetValue (dbToGain (-driveDb * 0.62f));
    saturation.setTargetValue (t);
}

void PuigChild::process (juce::dsp::AudioBlock<float> block)
{
    if (oversampler == nullptr)
        return;

    auto upsampled = oversampler->processSamplesUp (block);
    processOversampled (upsampled);
    oversampler->processSamplesDown (block);
}

void PuigChild::processOversampled (juce::dsp::AudioBlock<float>& block)
{
    const auto numChannels = juce::jmin ((size_t) 2, block.getNumChannels());
    const auto numSamples  = block.getNumSamples();

    float peakReduction = 0.0f;

    for (size_t n = 0; n < numSamples; ++n)
    {
        const auto gIn  = inputGain.getNextValue();
        const auto gOut = outputGain.getNextValue();
        const auto sat  = saturation.getNextValue();

        // -- linked detection ----------------------------------------------
        float detect = 0.0f;
        for (size_t c = 0; c < numChannels; ++c)
            detect = juce::jmax (detect, std::abs (block.getChannelPointer (c)[n] * gIn));

        const auto fastCoeff = (detect > envFast) ? attackCoeff : releaseFast;
        envFast += fastCoeff * (detect - envFast);
        envFast = flushDenormal (envFast);

        const auto slowCoeff = (detect > envSlow) ? attackCoeff : releaseSlow;
        envSlow += slowCoeff * (detect - envSlow);
        envSlow = flushDenormal (envSlow);

        // The real network recovers about half quickly, the rest over seconds.
        const auto env = juce::jmax (envFast * 0.55f + envSlow * 0.45f, 1.0e-7f);

        // -- vari-mu gain computer, soft knee ------------------------------
        const auto levelDb = juce::Decibels::gainToDecibels (env, -120.0f);
        const auto over = levelDb - thresholdDb;

        float reductionDb = 0.0f;

        if (over > kneeDb * 0.5f)
        {
            reductionDb = over * (1.0f - 1.0f / maxRatio);
        }
        else if (over > -kneeDb * 0.5f)
        {
            const auto x = over + kneeDb * 0.5f;
            reductionDb = (1.0f - 1.0f / maxRatio) * x * x / (2.0f * kneeDb);
        }

        const auto targetGain = dbToGain (-reductionDb);
        smoothedGain += gainSmoothCoeff * (targetGain - smoothedGain);
        smoothedGain = flushDenormal (smoothedGain);

        peakReduction = juce::jmax (peakReduction, -juce::Decibels::gainToDecibels (smoothedGain, -60.0f));

        // -- tube / transformer stage --------------------------------------
        for (size_t c = 0; c < numChannels; ++c)
        {
            auto* data = block.getChannelPointer (c);
            auto& ch = channels[c];

            auto x = data[n] * gIn;
            x = ch.transformerLow.process (x);
            x *= smoothedGain;

            if (sat > 1.0e-4f)
            {
                // Asymmetric triode curve: second harmonic first, third as it
                // is pushed. Scaled by how hard the stage is working.
                const auto drive = 1.0f + sat * 2.2f;
                const auto d = x * drive;
                const auto shaped = fastTanh (d) + 0.11f * sat * fastTanh (d * d * 0.7f);
                const auto y = shaped / drive;
                x += sat * (y - x);
                x -= ch.bias.processLowpass (x);
            }

            x = ch.transformerHigh.process (x);
            data[n] = ch.dcBlock.process (x * gOut);
        }
    }

    gainReductionDb.store (peakReduction);
}

} // namespace dr
