#include "MannyMReverb.h"

namespace dr
{

namespace
{
    // Medium chamber: shorter and denser than the hall lines in H-Reverb.
    constexpr float lineLengthsMs[] = { 29.7f, 37.1f, 43.3f, 51.9f };
    constexpr float diffusionMs[]   = { 4.7f, 7.3f, 10.9f, 14.3f };

    constexpr float chamberRt60 = 1.85f;
    constexpr float preDelayMs  = 22.0f;
}

void MannyMReverb::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    for (int i = 0; i < numLines; ++i)
    {
        auto& line = lines[(size_t) i];
        line.length = (float) (lineLengthsMs[i] * 0.001 * sampleRate);
        line.delay.prepare ((int) line.length + 8);
        line.feedback = feedbackForRt60 (line.length / (float) sampleRate, chamberRt60);
        line.damp.setCutoff (6800.0f, sampleRate);
        line.lfo.setRate (0.31f + 0.17f * (float) i, sampleRate);
        line.lfo.reset (0.25f * (float) i);
        line.mod.prepare (msToSamples (sampleRate, 3.1f + 1.7f * (float) i), 0.5f,
                          (float) (0.35 * 0.001 * sampleRate));
    }

    for (int s = 0; s < 2; ++s)
    {
        auto& side = sides[(size_t) s];
        const auto spread = (s == 0) ? 1.0f : 1.11f;

        for (int i = 0; i < 4; ++i)
            side.diffusion[(size_t) i].prepare (
                msToSamples (sampleRate, diffusionMs[i] * spread), 0.68f);

        side.preDelay.prepare (msToSamples (sampleRate, 200.0f));

        // Preset EQ: neutral lows, -10.1 mids, +38.5 highs (scaled to dB).
        side.lowShelf.set  (SvfFilter::lowShelf,  180.0f,  sampleRate, 0.707f,  0.0f);
        side.midBell.set   (SvfFilter::peaking,   900.0f,  sampleRate, 0.9f,   -3.0f);
        side.highShelf.set (SvfFilter::highShelf, 4200.0f, sampleRate, 0.707f,  5.5f);
        side.lowCut.set    (SvfFilter::highpass,  110.0f,  sampleRate, 0.707f);

        for (int i = 0; i < 4; ++i)
            phaserStages[(size_t) s][(size_t) i].set (SvfFilter::bandpass, 600.0f, sampleRate, 0.7f);
    }

    phaserLfo.setRate (0.42f, sampleRate);
    phaserLfo.reset (0.0f);

    compAttack  = 1.0f - std::exp (-1.0f / (float) (0.010 * sampleRate));
    compRelease = 1.0f - std::exp (-1.0f / (float) (0.220 * sampleRate));

    reset();
}

void MannyMReverb::reset()
{
    for (auto& line : lines)
    {
        line.delay.clear();
        line.mod.clear();
        line.damp.reset();
    }

    for (auto& side : sides)
    {
        for (auto& ap : side.diffusion)
            ap.clear();

        side.preDelay.clear();
        side.lowShelf.reset();
        side.midBell.reset();
        side.highShelf.reset();
        side.lowCut.reset();
    }

    for (auto& stages : phaserStages)
        for (auto& stage : stages)
            stage.reset();

    compEnv = 0.0f;
}

void MannyMReverb::setAmount (float amount) noexcept
{
    const auto t = juce::jlimit (0.0f, 10.0f, amount) * 0.1f;
    amountGain = t * t * 0.55f + t * 0.55f;

    // More Amount also tightens the diffusion, which is what makes the chamber
    // read as bigger rather than just louder.
    const auto g = 0.60f + 0.16f * t;
    for (auto& side : sides)
        for (auto& ap : side.diffusion)
            ap.setFeedback (g);
}

void MannyMReverb::setDistortion (float distortion) noexcept
{
    const auto t = juce::jlimit (0.0f, 10.0f, distortion) * 0.1f;

    // The shaper sits on the chamber output rather than in the feedback path.
    // In the loop it could only ever be a gentle unity-slope curve - anything
    // audible either made the tank self-oscillate or ate the tail - so the
    // grit lives downstream, where it can be driven properly.
    driveAmount = t * t * 12.0f + t * 2.0f;
    driveShape = 1.0f + driveAmount;

    // Normalised so a nominal tank level keeps its loudness, at half strength
    // in dB, which leaves some of the natural level drop from the saturation.
    constexpr float nominal = 0.25f;
    const auto makeUp = std::sqrt (nominal * driveShape / std::tanh (nominal * driveShape));
    driveTrim = makeUp / driveShape;
}

void MannyMReverb::process (float inL, float inR, float& outL, float& outR) noexcept
{
    const auto preDelaySamples = (float) (preDelayMs * 0.001 * sampleRate);

    std::array<float, 2> diffused {};

    for (int s = 0; s < 2; ++s)
    {
        auto& side = sides[(size_t) s];
        side.preDelay.write ((s == 0) ? inL : inR);

        auto x = side.preDelay.read (preDelaySamples);
        for (auto& ap : side.diffusion)
            x = ap.process (x);

        diffused[(size_t) s] = x;
    }

    std::array<float, numLines> taps {};

    for (int i = 0; i < numLines; ++i)
        taps[(size_t) i] = lines[(size_t) i].delay.readInt ((int) lines[(size_t) i].length);

    auto a = taps[0], b = taps[1], c = taps[2], d = taps[3];
    hadamard4 (a, b, c, d);
    const std::array<float, numLines> mixed { a, b, c, d };

    for (int i = 0; i < numLines; ++i)
    {
        auto& line = lines[(size_t) i];

        auto v = mixed[(size_t) i] + diffused[(size_t) (i % 2)] * 0.4f;
        v = line.mod.process (v, line.lfo.next());
        v = line.damp.processLowpass (v) * line.feedback;

        line.delay.write (flushDenormal (v));
    }

    auto wetL = (taps[0] + taps[2]) * 0.5f;
    auto wetR = (taps[1] + taps[3]) * 0.5f;

    // -- distortion --------------------------------------------------------
    if (driveAmount > 1.0e-4f)
    {
        wetL = fastTanh (wetL * driveShape) * driveTrim;
        wetR = fastTanh (wetR * driveShape) * driveTrim;
    }

    // -- phaser ------------------------------------------------------------
    const auto lfo = phaserLfo.next();
    const auto phaserFreq = 400.0f * std::pow (6.0f, 0.5f * (lfo + 1.0f));

    for (int s = 0; s < 2; ++s)
    {
        auto& wet = (s == 0) ? wetL : wetR;
        const auto offset = (s == 0) ? 1.0f : 1.25f;

        float phased = wet;
        for (auto& stage : phaserStages[(size_t) s])
        {
            stage.set (SvfFilter::bandpass, phaserFreq * offset, sampleRate, 0.55f);
            phased = stage.process (phased);
        }

        wet += phased * 0.22f;
    }

    // -- compressor --------------------------------------------------------
    const auto detect = juce::jmax (std::abs (wetL), std::abs (wetR));
    const auto coeff = detect > compEnv ? compAttack : compRelease;
    compEnv += coeff * (detect - compEnv);
    compEnv = flushDenormal (compEnv);

    auto compGain = 1.0f;
    constexpr float threshold = 0.30f;
    if (compEnv > threshold)
        compGain = (threshold + (compEnv - threshold) * 0.35f) / compEnv;

    wetL *= compGain;
    wetR *= compGain;

    auto shape = [] (Side& side, float x)
    {
        x = side.lowShelf.process (x);
        x = side.midBell.process (x);
        x = side.highShelf.process (x);
        return side.lowCut.process (x);
    };

    outL = shape (sides[0], wetL) * amountGain;
    outR = shape (sides[1], wetR) * amountGain;
}

} // namespace dr
