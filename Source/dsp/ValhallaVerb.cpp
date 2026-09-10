#include "ValhallaVerb.h"

namespace dr
{

namespace
{
    // Concert Hall at Size 70%: long, widely spaced lines.
    constexpr float lineLengthsMs[] = { 43.7f,  56.3f,  67.1f,  79.9f,
                                        91.3f, 104.7f, 117.1f, 131.3f };
    constexpr float sizeScale = 0.70f * 1.35f;

    // Early diffusion 63.6% - deliberately low, so density builds over time.
    constexpr float earlyDiffusionMs[] = { 13.7f, 19.3f, 27.1f, 35.9f };
    constexpr float earlyDiffusionG = 0.52f;
    constexpr float lateDiffusionG  = 0.66f;

    constexpr float preDelayMs   = 13.05f;
    constexpr float bassXoverHz  = 780.0f;
    constexpr float bassMult     = 1.50f;
    constexpr float highShelfDb  = -6.05f;
    constexpr float highFreqHz   = 13410.0f;
    constexpr float lowCutHz     = 960.0f;
    constexpr float vintageBandwidthHz = 10000.0f;
    constexpr float modRateHz    = 0.10f;
    constexpr float modDepthMs   = 0.36f * 4.0f;
}

void ValhallaVerb::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    for (int i = 0; i < numLines; ++i)
    {
        auto& line = lines[(size_t) i];
        line.length = (float) (lineLengthsMs[i] * sizeScale * 0.001 * sampleRate);
        line.modDepth = (float) (modDepthMs * 0.001 * sampleRate);
        line.delay.prepare ((int) line.length + 8);
        line.mod.prepare (msToSamples (sampleRate, 7.3f + 2.9f * (float) i),
                          lateDiffusionG, line.modDepth);
        line.hfDamp.setCutoff (highFreqHz, sampleRate);
        line.bassDamp.setCutoff (bassXoverHz, sampleRate);

        // The 1970s mode modulates with noise rather than a clean sine.
        line.lfo.setRate (modRateHz * (1.0f + 0.13f * (float) i), sampleRate);
        line.lfo.reset ((float) i / (float) numLines);
    }

    for (int s = 0; s < 2; ++s)
    {
        auto& side = sides[(size_t) s];
        const auto spread = (s == 0) ? 1.0f : 1.09f;

        for (int i = 0; i < 4; ++i)
            side.earlyDiffusion[(size_t) i].prepare (
                msToSamples (sampleRate, earlyDiffusionMs[i] * spread), earlyDiffusionG);

        side.preDelay.prepare (msToSamples (sampleRate, 200.0f));
        side.bandLimit.set (SvfFilter::lowpass,  vintageBandwidthHz, sampleRate, 0.707f);
        side.highShelf.set (SvfFilter::highShelf, highFreqHz, sampleRate, 0.707f, highShelfDb);
        side.lowCut.set    (SvfFilter::highpass, lowCutHz, sampleRate, 0.707f);
    }

    updateDecay();
    highCutDirty = true;
    reset();
}

void ValhallaVerb::reset()
{
    random.setSeed (0x5a17a11);

    for (auto& line : lines)
    {
        line.delay.clear();
        line.mod.clear();
        line.hfDamp.reset();
        line.bassDamp.reset();
        line.noise = 0.0f;
    }

    for (auto& side : sides)
    {
        for (auto& ap : side.earlyDiffusion)
            ap.clear();

        side.preDelay.clear();
        side.bandLimit.reset();
        side.highShelf.reset();
        side.highCut.reset();
        side.lowCut.reset();
    }
}

void ValhallaVerb::setDecay (float decay) noexcept
{
    const auto t = juce::jlimit (0.0f, 10.0f, decay) * 0.1f;
    const auto newDecay = 0.3f * std::pow (30.0f, t);

    if (std::abs (newDecay - decaySeconds) > 1.0e-4f)
    {
        decaySeconds = newDecay;
        updateDecay();
    }
}

void ValhallaVerb::setHighCut (float highCut) noexcept
{
    const auto t = juce::jlimit (0.0f, 10.0f, highCut) * 0.1f;
    const auto newFreq = 1000.0f * std::pow (20.0f, t);

    if (std::abs (newFreq - highCutHz) > 0.5f)
    {
        highCutHz = newFreq;
        highCutDirty = true;
    }
}

void ValhallaVerb::updateDecay()
{
    for (auto& line : lines)
    {
        const auto lengthSeconds = line.length / (float) sampleRate;

        line.gMid  = feedbackForRt60 (lengthSeconds, decaySeconds);
        line.gHigh = feedbackForRt60 (lengthSeconds, decaySeconds * 0.55f);

        const auto gBass = feedbackForRt60 (lengthSeconds, decaySeconds * bassMult);
        line.bassBoost = gBass / juce::jmax (1.0e-6f, line.gMid) - 1.0f;
    }
}

void ValhallaVerb::process (float inL, float inR, float& outL, float& outR) noexcept
{
    if (highCutDirty)
    {
        const auto f = juce::jmin (highCutHz, (float) sampleRate * 0.45f);
        for (auto& side : sides)
            side.highCut.set (SvfFilter::lowpass, f, sampleRate, 0.707f);

        highCutDirty = false;
    }

    const auto preDelaySamples = (float) (preDelayMs * 0.001 * sampleRate);

    std::array<float, 2> diffused {};

    for (int s = 0; s < 2; ++s)
    {
        auto& side = sides[(size_t) s];
        side.preDelay.write ((s == 0) ? inL : inR);

        auto x = side.preDelay.read (preDelaySamples);
        x = side.bandLimit.process (x);

        for (auto& ap : side.earlyDiffusion)
            x = ap.process (x);

        diffused[(size_t) s] = x;
    }

    std::array<float, numLines> taps {};

    for (int i = 0; i < numLines; ++i)
    {
        auto& line = lines[(size_t) i];

        // Slow sine plus a slewed noise source: the vintage modes wobble
        // rather than chorus cleanly.
        line.noise += 0.002f * (random.nextFloat() * 2.0f - 1.0f - line.noise);
        const auto mod = (line.lfo.next() + line.noise * 6.0f) * line.modDepth;

        taps[(size_t) i] = line.delay.read (juce::jmax (2.0f, line.length + mod));
    }

    std::array<float, 8> mixed = taps;
    householder8 (mixed);

    for (int i = 0; i < numLines; ++i)
    {
        auto& line = lines[(size_t) i];

        // Feeding successive lines with a decreasing weight is what gives the
        // Concert Hall its slow attack: the far lines fill in late.
        const auto feedWeight = 0.42f - 0.035f * (float) i;
        auto v = mixed[(size_t) i] + diffused[(size_t) (i % 2)] * juce::jmax (0.08f, feedWeight);

        v = line.mod.process (v, line.lfo.next() * 0.5f);

        const auto low = line.hfDamp.processLowpass (v);
        v = line.gHigh * v + (line.gMid - line.gHigh) * low;

        const auto bass = line.bassDamp.processLowpass (v);
        v += line.bassBoost * bass;

        line.delay.write (flushDenormal (v));
    }

    auto wetL = (taps[0] + taps[2] - taps[5] + taps[7]) * 0.45f;
    auto wetR = (taps[1] - taps[3] + taps[4] + taps[6]) * 0.45f;

    auto shape = [] (Side& side, float x)
    {
        x = side.highShelf.process (x);
        x = side.highCut.process (x);
        return side.lowCut.process (x);
    };

    outL = shape (sides[0], wetL);
    outR = shape (sides[1], wetR);
}

} // namespace dr
