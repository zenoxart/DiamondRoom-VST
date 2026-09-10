#include "HReverb.h"

namespace dr
{

namespace
{
    // Mutually prime line lengths in ms, sized for the preset's 1.1 "Size".
    constexpr float lineLengthsMs[] = { 41.3f, 47.9f, 53.7f, 61.1f,
                                        67.3f, 73.9f, 79.1f, 88.7f };

    // Input diffusion, per side, in ms (right side offset for decorrelation).
    constexpr float diffusionMs[] = { 8.3f, 11.7f, 17.9f, 23.1f };

    // Damping crossovers from the preset.
    constexpr float loFreq = 667.0f;
    constexpr float hiFreq = 2711.0f;
    constexpr float hiRatio = 1.24f;
    constexpr float loRatio = 1.0f;
}

void HReverb::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    for (int i = 0; i < numLines; ++i)
    {
        auto& line = lines[(size_t) i];
        line.baseLength = (float) (lineLengthsMs[i] * 0.001 * sampleRate);
        line.modDepth = (float) (0.6 * 0.001 * sampleRate); // 0.6 ms of chorusing
        line.delay.prepare ((int) line.baseLength + (int) line.modDepth + 8);
        line.lfo.setRate (0.07f + 0.031f * (float) i, sampleRate);
        line.lfo.reset ((float) i / (float) numLines);
        line.hfDamp.setCutoff (hiFreq, sampleRate);
        line.lfDamp.setCutoff (loFreq, sampleRate);
    }

    for (int s = 0; s < 2; ++s)
    {
        auto& side = sides[(size_t) s];
        const auto spread = (s == 0) ? 1.0f : 1.13f;

        for (int i = 0; i < 4; ++i)
            side.diffusion[(size_t) i].prepare (
                msToSamples (sampleRate, diffusionMs[i] * spread), 0.62f);

        side.earlyLine.prepare (msToSamples (sampleRate, 200.0f));

        // ER filter from the preset: heavily shelved off above 10 kHz.
        side.earlyFilter.set (SvfFilter::highShelf, 10013.0f, sampleRate, 0.707f, -12.0f);

        // The preset's fixed reverb EQ: +11.1 dB at 839 Hz (Q 3.68) and
        // +6.8 dB at 2939 Hz (Q 0.70), with a 240 Hz shaping band.
        side.eqA.set (SvfFilter::peaking, 839.0f,  sampleRate, 3.68f, 11.1f);
        side.eqB.set (SvfFilter::peaking, 2939.0f, sampleRate, 0.70f, 6.8f);
        side.eqC.set (SvfFilter::peaking, 240.0f,  sampleRate, 1.20f, 0.0f);
        side.lowCut.set (SvfFilter::highpass, 90.0f, sampleRate, 0.707f);
    }

    // Early reflection pattern (ER Select 5), times in ms and linear gains.
    const std::array<std::pair<float, float>, numEarly> taps { {
        {  7.3f,  0.84f }, { 11.9f, -0.71f }, { 17.1f,  0.62f }, { 23.7f, -0.55f },
        { 29.3f,  0.48f }, { 36.9f,  0.42f }, { 44.1f, -0.37f }, { 52.7f,  0.31f },
        { 61.3f, -0.27f }, { 71.9f,  0.23f }, { 83.1f,  0.19f }, { 95.7f, -0.15f } } };
    earlyTaps = taps;

    compAttack  = 1.0f - std::exp (-1.0f / (float) (0.005 * sampleRate));
    compRelease = 1.0f - std::exp (-1.0f / (float) (0.300 * sampleRate));

    updateDecay();
    updateTone();
    reset();
}

void HReverb::reset()
{
    for (auto& line : lines)
    {
        line.delay.clear();
        line.hfDamp.reset();
        line.lfDamp.reset();
    }

    for (auto& side : sides)
    {
        for (auto& ap : side.diffusion)
            ap.clear();

        side.earlyLine.clear();
        side.earlyFilter.reset();
        side.eqA.reset();
        side.eqB.reset();
        side.eqC.reset();
        side.toneLow.reset();
        side.toneHigh.reset();
        side.lowCut.reset();
    }

    compEnv = 0.0f;
}

void HReverb::setTime (float time) noexcept
{
    // 0 -> 0.25 s, 10 -> 10 s; the preset's 3.24 s sits near 6.9 on the dial.
    const auto t = juce::jlimit (0.0f, 10.0f, time) * 0.1f;
    const auto newRt = 0.25f * std::pow (40.0f, t);

    if (std::abs (newRt - rt60) > 1.0e-4f)
    {
        rt60 = newRt;
        updateDecay();
    }
}

void HReverb::setTone (float tone) noexcept
{
    const auto t = juce::jlimit (-10.0f, 10.0f, tone);

    if (std::abs (t - toneAmount) > 1.0e-4f)
    {
        toneAmount = t;
        updateTone();
    }
}

void HReverb::updateDecay()
{
    for (auto& line : lines)
    {
        const auto lengthSeconds = line.baseLength / (float) sampleRate;

        line.gMid  = feedbackForRt60 (lengthSeconds, rt60);
        line.gHigh = feedbackForRt60 (lengthSeconds, rt60 * hiRatio);
        line.gLowRatio = feedbackForRt60 (lengthSeconds, rt60 * loRatio) / juce::jmax (1.0e-6f, line.gMid);
    }
}

void HReverb::updateTone()
{
    // +-10 on the dial becomes a +-9 dB tilt around 900 Hz.
    const auto db = toneAmount * 0.9f;

    for (auto& side : sides)
    {
        side.toneLow.set  (SvfFilter::lowShelf,  400.0f,  sampleRate, 0.707f, -db);
        side.toneHigh.set (SvfFilter::highShelf, 2200.0f, sampleRate, 0.707f,  db);
    }

    // Keep perceived level roughly constant across the tilt.
    toneTrim = dbToGain (-std::abs (db) * 0.25f);
}

void HReverb::process (float inL, float inR, float& outL, float& outR) noexcept
{
    std::array<float, 2> early { 0.0f, 0.0f };
    std::array<float, 2> diffused { 0.0f, 0.0f };

    for (int s = 0; s < 2; ++s)
    {
        auto& side = sides[(size_t) s];
        const auto in = (s == 0) ? inL : inR;

        side.earlyLine.write (in);

        float sum = 0.0f;
        for (int i = 0; i < numEarly; ++i)
        {
            const auto tapMs = earlyTaps[(size_t) i].first * (s == 0 ? 1.0f : 1.07f);
            sum += earlyTaps[(size_t) i].second
                     * side.earlyLine.read ((float) (tapMs * 0.001 * sampleRate));
        }

        early[(size_t) s] = side.earlyFilter.process (sum * 0.42f);

        auto x = in;
        for (auto& ap : side.diffusion)
            x = ap.process (x);

        diffused[(size_t) s] = x;
    }

    // -- FDN tail ----------------------------------------------------------
    std::array<float, numLines> taps {};

    for (int i = 0; i < numLines; ++i)
    {
        auto& line = lines[(size_t) i];
        const auto mod = line.lfo.next() * line.modDepth;
        taps[(size_t) i] = line.delay.read (line.baseLength + mod);
    }

    std::array<float, 8> mixed = taps;
    householder8 (mixed);

    for (int i = 0; i < numLines; ++i)
    {
        auto& line = lines[(size_t) i];

        // Alternate which input feeds which line so the tail stays wide.
        const auto in = (i % 2 == 0) ? diffused[0] : diffused[1];

        auto v = mixed[(size_t) i] + in * 0.35f;

        // Frequency dependent decay: gHigh above 2711 Hz, gMid in the middle,
        // gMid * gLowRatio below 667 Hz.
        const auto low = line.hfDamp.processLowpass (v);
        v = line.gHigh * v + (line.gMid - line.gHigh) * low;

        if (std::abs (line.gLowRatio - 1.0f) > 1.0e-4f)
        {
            const auto veryLow = line.lfDamp.processLowpass (v);
            v += (line.gLowRatio - 1.0f) * veryLow;
        }

        line.delay.write (flushDenormal (v));
    }

    auto tailL = (taps[0] - taps[2] + taps[4] - taps[6]) * 0.5f;
    auto tailR = (taps[1] - taps[3] + taps[5] - taps[7]) * 0.5f;

    // -- tail dynamics -----------------------------------------------------
    const auto detect = juce::jmax (std::abs (tailL), std::abs (tailR));
    const auto coeff = detect > compEnv ? compAttack : compRelease;
    compEnv += coeff * (detect - compEnv);
    compEnv = flushDenormal (compEnv);

    const auto threshold = 0.35f;
    auto compGain = 1.0f;
    if (compEnv > threshold)
        compGain = threshold + (compEnv - threshold) * 0.45f; // ~2.2:1
    else
        compGain = compEnv;

    compGain = (compEnv > 1.0e-6f) ? compGain / compEnv : 1.0f;

    tailL *= compGain;
    tailR *= compGain;

    // -- output stage ------------------------------------------------------
    auto shape = [this] (Side& side, float x)
    {
        x = side.eqA.process (x);
        x = side.eqB.process (x);
        x = side.eqC.process (x);
        x = side.toneLow.process (x);
        x = side.toneHigh.process (x);
        return side.lowCut.process (x) * toneTrim;
    };

    // ER/Tail balance sits at 50/50 in the preset.
    outL = shape (sides[0], early[0] * 0.5f + tailL * 0.5f) * 0.9f;
    outR = shape (sides[1], early[1] * 0.5f + tailR * 0.5f) * 0.9f;
}

} // namespace dr
