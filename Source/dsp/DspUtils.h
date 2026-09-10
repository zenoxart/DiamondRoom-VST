#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>

namespace dr
{

//==============================================================================
inline float flushDenormal (float x) noexcept
{
    return (std::abs (x) < 1.0e-20f) ? 0.0f : x;
}

/** Pade 5/4 tanh approximation: smooth, monotonic, cheap. */
inline float fastTanh (float x) noexcept
{
    const auto x2 = x * x;

    if (x2 > 25.0f)
        return x > 0.0f ? 1.0f : -1.0f;

    const auto num = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const auto den = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return num / den;
}

inline float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db * 0.05f);
}

//==============================================================================
/** One-pole low-pass, used for damping and control smoothing. */
class OnePole
{
public:
    void reset() noexcept { z = 0.0f; }

    void setCutoff (float hz, double sampleRate) noexcept
    {
        const auto w = juce::jlimit (0.0001f, 0.49f, (float) (hz / sampleRate));
        a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * w);
    }

    void setCoefficient (float newA) noexcept { a = juce::jlimit (0.0f, 1.0f, newA); }

    inline float processLowpass (float x) noexcept
    {
        z += a * (x - z);
        z = flushDenormal (z);
        return z;
    }

    inline float processHighpass (float x) noexcept { return x - processLowpass (x); }

private:
    float a = 0.5f, z = 0.0f;
};

//==============================================================================
/**
    TPT state-variable filter with output mixing, after Zavalishin. Handles
    low/high/band-pass plus low/high shelf and peaking from the same core.
*/
class SvfFilter
{
public:
    enum Type { lowpass, highpass, bandpass, lowShelf, highShelf, peaking };

    void reset() noexcept { s1 = s2 = 0.0f; }

    void set (Type type, float freq, double sampleRate, float q = 0.7071f, float gainDb = 0.0f) noexcept
    {
        const auto f = juce::jlimit (10.0f, (float) sampleRate * 0.48f, freq);
        const auto A = std::pow (10.0f, gainDb / 40.0f);

        auto gg = std::tan (juce::MathConstants<float>::pi * f / (float) sampleRate);
        auto kk = 1.0f / juce::jmax (0.05f, q);

        switch (type)
        {
            case lowpass:   cLP = 1.0f; cBP = 0.0f;      cHP = 0.0f; break;
            case highpass:  cLP = 0.0f; cBP = 0.0f;      cHP = 1.0f; break;
            case bandpass:  cLP = 0.0f; cBP = 1.0f;      cHP = 0.0f; break;

            case lowShelf:
                gg /= std::sqrt (A);
                cLP = A * A; cBP = kk * A; cHP = 1.0f;
                break;

            case highShelf:
                gg *= std::sqrt (A);
                cLP = 1.0f; cBP = kk * A; cHP = A * A;
                break;

            case peaking:
            default:
                kk /= A;
                cLP = 1.0f; cBP = kk * A * A; cHP = 1.0f;
                break;
        }

        g = gg;
        k = kk;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    inline float process (float x) noexcept
    {
        const auto v3 = x - s2;
        const auto v1 = a1 * s1 + a2 * v3;
        const auto v2 = s2 + a2 * s1 + a3 * v3;

        s1 = flushDenormal (2.0f * v1 - s1);
        s2 = flushDenormal (2.0f * v2 - s2);

        const auto hp = x - k * v1 - v2;
        return cHP * hp + cBP * v1 + cLP * v2;
    }

private:
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float cLP = 1.0f, cBP = 0.0f, cHP = 0.0f;
    float s1 = 0.0f, s2 = 0.0f;
};

//==============================================================================
/** Fractional delay line with linear interpolation, power-of-two masked. */
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        int newSize = 1;
        while (newSize < maxSamples + 4)
            newSize <<= 1;

        buffer.assign ((size_t) newSize, 0.0f);
        mask = newSize - 1;
        writePos = 0;
    }

    void clear() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    inline void write (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    inline float readInt (int delaySamples) const noexcept
    {
        return buffer[(size_t) ((writePos - delaySamples) & mask)];
    }

    inline float read (float delaySamples) const noexcept
    {
        const auto d = (int) delaySamples;
        const auto frac = delaySamples - (float) d;
        const auto s0 = buffer[(size_t) ((writePos - d)     & mask)];
        const auto s1 = buffer[(size_t) ((writePos - d - 1) & mask)];
        return s0 + frac * (s1 - s0);
    }

    int size() const noexcept { return mask + 1; }

private:
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
};

//==============================================================================
/** Schroeder all-pass diffuser. */
class Allpass
{
public:
    void prepare (int delaySamples, float feedback)
    {
        line.prepare (delaySamples + 2);
        delay = juce::jmax (1, delaySamples);
        g = feedback;
    }

    void clear() noexcept { line.clear(); }
    void setFeedback (float newG) noexcept { g = newG; }

    inline float process (float x) noexcept
    {
        const auto delayed = line.readInt (delay);
        const auto v = x + g * delayed;
        line.write (flushDenormal (v));
        return delayed - g * v;
    }

private:
    DelayLine line;
    int delay = 1;
    float g = 0.5f;
};

//==============================================================================
/** All-pass with a modulated delay time, for chorused tails. */
class ModAllpass
{
public:
    void prepare (int delaySamples, float feedback, float modDepthSamples)
    {
        line.prepare (delaySamples + (int) modDepthSamples + 8);
        base = (float) juce::jmax (2, delaySamples);
        depth = modDepthSamples;
        g = feedback;
    }

    void clear() noexcept { line.clear(); }
    void setFeedback (float newG) noexcept { g = newG; }
    void setDepth (float d) noexcept { depth = d; }

    inline float process (float x, float lfo) noexcept
    {
        const auto d = juce::jmax (1.0f, base + depth * lfo);
        const auto delayed = line.read (d);
        const auto v = x + g * delayed;
        line.write (flushDenormal (v));
        return delayed - g * v;
    }

private:
    DelayLine line;
    float base = 1.0f, depth = 0.0f, g = 0.5f;
};

//==============================================================================
class Lfo
{
public:
    void reset (float phase = 0.0f) noexcept { ph = phase; }

    void setRate (float hz, double sampleRate) noexcept
    {
        inc = (float) (hz / sampleRate);
    }

    inline float next() noexcept
    {
        ph += inc;
        if (ph >= 1.0f)
            ph -= 1.0f;

        return std::sin (juce::MathConstants<float>::twoPi * ph);
    }

private:
    float ph = 0.0f, inc = 0.0f;
};

//==============================================================================
/** Energy-preserving 4x4 Hadamard mix. */
inline void hadamard4 (float& a, float& b, float& c, float& d) noexcept
{
    const auto ab = a + b, cd = c + d;
    const auto abd = a - b, cdd = c - d;
    constexpr float s = 0.5f;

    a = (ab  + cd)  * s;
    b = (abd + cdd) * s;
    c = (ab  - cd)  * s;
    d = (abd - cdd) * s;
}

/** Energy-preserving 8x8 Householder mix. */
inline void householder8 (std::array<float, 8>& v) noexcept
{
    float sum = 0.0f;
    for (auto x : v)
        sum += x;

    const auto f = sum * 0.25f; // 2/N
    for (auto& x : v)
        x = f - x;
}

//==============================================================================
inline int msToSamples (double sampleRate, float ms) noexcept
{
    return juce::jmax (1, (int) (ms * 0.001 * sampleRate));
}

/** Feedback gain that yields the requested RT60 for a given delay length. */
inline float feedbackForRt60 (float delaySeconds, float rt60Seconds) noexcept
{
    if (rt60Seconds <= 0.001f)
        return 0.0f;

    return juce::jlimit (0.0f, 0.9995f, std::pow (10.0f, -3.0f * delaySeconds / rt60Seconds));
}

} // namespace dr
