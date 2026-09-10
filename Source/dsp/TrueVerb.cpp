#include "TrueVerb.h"

namespace dr
{

namespace
{
    constexpr float preDelayMs   = 63.5f;
    constexpr float decaySeconds = 0.5f;
    constexpr float hiFreqHz     = 4504.0f;
    constexpr float hiDampRatio  = 0.30f;   // highs decay 3.3x faster
    constexpr float loFreqHz     = 120.0f;
    constexpr float loDampRatio  = 1.10f;
    constexpr float revShelfDb   = -6.5f;
    constexpr float erLowCutHz   = 160.0f;
    constexpr float density      = 0.60f;
    constexpr float speedOfSound = 343.0f;  // m/s

    constexpr float lineLengthsMs[] = { 23.9f, 31.1f, 37.7f, 43.1f, 51.7f, 59.3f };
    constexpr float diffusionMs[]   = { 6.1f, 9.7f, 13.9f };
}

void TrueVerb::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    for (int i = 0; i < numLines; ++i)
    {
        auto& line = lines[(size_t) i];
        line.length = (float) (lineLengthsMs[i] * 0.001 * sampleRate);
        line.delay.prepare ((int) line.length + 8);
        line.hfDamp.setCutoff (hiFreqHz, sampleRate);
        line.lfDamp.setCutoff (loFreqHz, sampleRate);

        const auto lengthSeconds = line.length / (float) sampleRate;
        line.gMid  = feedbackForRt60 (lengthSeconds, decaySeconds);
        line.gHigh = feedbackForRt60 (lengthSeconds, decaySeconds * hiDampRatio);
        line.lowRatio = feedbackForRt60 (lengthSeconds, decaySeconds * loDampRatio)
                          / juce::jmax (1.0e-6f, line.gMid);
    }

    for (int s = 0; s < 2; ++s)
    {
        auto& side = sides[(size_t) s];
        const auto spread = (s == 0) ? 1.0f : 1.12f;

        side.earlyLine.prepare (msToSamples (sampleRate, 400.0f));
        side.preDelay.prepare (msToSamples (sampleRate, 400.0f));

        for (int i = 0; i < 3; ++i)
            side.diffusion[(size_t) i].prepare (
                msToSamples (sampleRate, diffusionMs[i] * spread), 0.60f);

        side.erLowCut.set (SvfFilter::highpass, erLowCutHz, sampleRate, 0.707f);
        side.revShelf.set (SvfFilter::highShelf, hiFreqHz, sampleRate, 0.707f, revShelfDb);
    }

    geometryDirty = true;
    updateGeometry();
    reset();
}

void TrueVerb::reset()
{
    for (auto& line : lines)
    {
        line.delay.clear();
        line.hfDamp.reset();
        line.lfDamp.reset();
    }

    for (auto& side : sides)
    {
        side.earlyLine.clear();
        side.preDelay.clear();

        for (auto& ap : side.diffusion)
            ap.clear();

        side.airAbsorb.reset();
        side.erLowCut.reset();
        side.revShelf.reset();
    }
}

void TrueVerb::setDistance (float distance) noexcept
{
    const auto t = juce::jlimit (0.0f, 10.0f, distance) * 0.1f;
    const auto metres = 0.5f + 29.5f * t * t;

    if (std::abs (metres - distanceMetres) > 1.0e-3f)
    {
        distanceMetres = metres;
        geometryDirty = true;
    }
}

void TrueVerb::setRoomsize (float roomsize) noexcept
{
    const auto t = juce::jlimit (0.0f, 10.0f, roomsize) * 0.1f;
    const auto volume = 200.0f * std::pow (150.0f, t);

    if (std::abs (volume - roomVolume) > 0.5f)
    {
        roomVolume = volume;
        geometryDirty = true;
    }
}

void TrueVerb::updateGeometry()
{
    if (! geometryDirty)
        return;

    geometryDirty = false;

    // Mean free path of a room of this volume gives the first-order spacing
    // between reflections; the direct distance sets where they start.
    const auto roomDimension = std::cbrt (roomVolume);          // metres
    const auto firstReflection = juce::jmax (2.0f, roomDimension * 0.5f + distanceMetres);
    const auto baseMs = firstReflection / speedOfSound * 1000.0f;
    const auto spacingMs = roomDimension / speedOfSound * 1000.0f * (2.0f - density);

    juce::Random rng (0x7ff17e0);

    for (int i = 0; i < numEarly; ++i)
    {
        const auto n = (float) i;
        const auto jitter = 0.7f + 0.6f * rng.nextFloat();

        tapTimes[(size_t) i] = juce::jlimit (
            1.0f, 380.0f, baseMs + spacingMs * (n * 0.55f + n * n * 0.045f) * jitter);

        // Reflections lose energy with order and with the extra path length.
        const auto decay = std::pow (0.80f, n * 0.85f);
        const auto sign = (rng.nextInt (2) == 0) ? -1.0f : 1.0f;
        tapGains[(size_t) i] = sign * decay;
    }

    // Further away, the direct/early ratio drops and air absorption bites.
    const auto proximity = juce::jlimit (0.0f, 1.0f, 1.0f - distanceMetres / 30.0f);
    earlyGain = 0.67f * (0.45f + 0.55f * proximity);
    tailGain  = 0.55f + 0.75f * (1.0f - proximity);

    const auto airHz = juce::jlimit (2200.0f, 18000.0f,
                                     18000.0f - distanceMetres * 480.0f);

    for (auto& side : sides)
        side.airAbsorb.set (SvfFilter::lowpass, airHz, sampleRate, 0.707f);
}

void TrueVerb::process (float inL, float inR, float& outL, float& outR) noexcept
{
    updateGeometry();

    const auto preDelaySamples = (float) (preDelayMs * 0.001 * sampleRate);

    std::array<float, 2> early {};
    std::array<float, 2> tailIn {};

    for (int s = 0; s < 2; ++s)
    {
        auto& side = sides[(size_t) s];
        const auto in = (s == 0) ? inL : inR;

        side.earlyLine.write (in);
        side.preDelay.write (in);

        const auto stereoSkew = (s == 0) ? 1.0f : 1.06f;

        float sum = 0.0f;
        for (int i = 0; i < numEarly; ++i)
        {
            const auto t = tapTimes[(size_t) i] * stereoSkew;
            sum += tapGains[(size_t) i]
                     * side.earlyLine.read ((float) (t * 0.001 * sampleRate));
        }

        sum = side.erLowCut.process (side.airAbsorb.process (sum * 0.30f));
        early[(size_t) s] = sum;

        auto x = side.preDelay.read (preDelaySamples);
        for (auto& ap : side.diffusion)
            x = ap.process (x);

        tailIn[(size_t) s] = x;
    }

    std::array<float, numLines> taps {};

    for (int i = 0; i < numLines; ++i)
        taps[(size_t) i] = lines[(size_t) i].delay.readInt ((int) lines[(size_t) i].length);

    // Two Hadamard blocks with a cross-feed between them.
    auto a = taps[0], b = taps[1], c = taps[2], d = taps[3];
    hadamard4 (a, b, c, d);
    const auto e = (taps[4] + taps[5]) * 0.5f;
    const auto f = (taps[4] - taps[5]) * 0.5f;

    const std::array<float, numLines> mixed { a + f * 0.3f, b, c - f * 0.3f, d, e, a * 0.3f + e };

    for (int i = 0; i < numLines; ++i)
    {
        auto& line = lines[(size_t) i];

        auto v = mixed[(size_t) i] * 0.72f + tailIn[(size_t) (i % 2)] * 0.34f;

        const auto low = line.hfDamp.processLowpass (v);
        v = line.gHigh * v + (line.gMid - line.gHigh) * low;

        if (std::abs (line.lowRatio - 1.0f) > 1.0e-4f)
            v += (line.lowRatio - 1.0f) * line.lfDamp.processLowpass (v);

        line.delay.write (flushDenormal (v));
    }

    auto tailL = (taps[0] - taps[2] + taps[4]) * 0.45f;
    auto tailR = (taps[1] - taps[3] + taps[5]) * 0.45f;

    tailL = sides[0].revShelf.process (tailL) * tailGain;
    tailR = sides[1].revShelf.process (tailR) * tailGain;

    outL = early[0] * earlyGain + tailL;
    outR = early[1] * earlyGain + tailR;
}

} // namespace dr
