#include "CleanVoiceTube.h"

namespace dr
{

namespace
{
    float ratioForOvershoot (float overDb) noexcept
    {
        const auto over = juce::jmax (0.0f, overDb);
        return 1.6f + (4.0f - 1.6f) * over / (over + 12.0f);
    }

    float toDb (float linear) noexcept
    {
        return linear > 1.0e-7f ? 20.0f * std::log10 (linear) : -120.0f;
    }
}

void CleanVoiceTube::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    fastEnv.prepare (sampleRate);
    slowEnv.prepare (sampleRate);
    fastEnv.setTimes (attackMs, fastReleaseMs);
    slowEnv.setTimes (attackMs * 4.0f, slowReleaseMs);

    gainSmoothCoeff = EnvelopeFollower::timeToCoefficient (1.0f, sampleRate);

    for (auto& f : sidechainHp) f.makeHighpass (sampleRate, sidechainHpHz, 0.707);
    for (auto& f : transformer) f.makeHighShelf (sampleRate, transformerHz, 0.707, transformerDb);
    for (auto& f : dcBlocker)   f.makeHighpass (sampleRate, 12.0, 0.707);

    // Make-up comes from the static curve at a nominal vocal level. Because it
    // does not depend on the incoming signal it cannot pump, which is what the
    // input/output gain pairing on a real valve compressor gives you.
    constexpr float nominalLevelDb = -14.0f;
    const auto overDb = nominalLevelDb - thresholdDb;
    makeUp = dbToGain (-softKneeGain (overDb, kneeDb, ratioForOvershoot (overDb)) * autoMakeup);

    mix.reset (sampleRate, 0.02);

    reset();
}

void CleanVoiceTube::reset()
{
    fastEnv.reset();
    slowEnv.reset();

    for (auto& f : sidechainHp) f.reset();
    for (auto& f : transformer) f.reset();
    for (auto& f : dcBlocker)   f.reset();

    currentGain = 1.0f;
    gainReductionDb.store (0.0f);
}

void CleanVoiceTube::setMix (float amount) noexcept
{
    mix.setTargetValue (juce::jlimit (0.0f, 10.0f, amount) * 0.1f);
}

float CleanVoiceTube::valveStage (float x, float colour) const noexcept
{
    if (colour <= 0.0f)
        return x;

    const auto driven = x * (1.0f + colour * 2.0f);

    // The asymmetry is where the even harmonics come from; the DC it leaves
    // behind is taken out by the blocker downstream.
    const auto shaped = fastTanh (driven + colour * 0.25f * driven * driven);
    return juce::jmap (colour, 0.0f, 1.0f, x, shaped / (1.0f + colour * 2.0f));
}

void CleanVoiceTube::process (juce::dsp::AudioBlock<float> block)
{
    const auto numChannels = juce::jmin ((size_t) 2, block.getNumChannels());
    const auto numSamples = block.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    auto* left = block.getChannelPointer (0);
    auto* right = numChannels > 1 ? block.getChannelPointer (1) : nullptr;

    float peakReduction = 0.0f;

    for (size_t i = 0; i < numSamples; ++i)
    {
        const auto dryL = left[i];
        const auto dryR = right != nullptr ? right[i] : dryL;

        // -- detector: stereo linked, sidechain high passed ------------------
        const auto scL = sidechainHp[0].processSample (dryL);
        const auto scR = right != nullptr ? sidechainHp[1].processSample (dryR) : scL;
        const auto rectified = juce::jmax (std::abs (scL), std::abs (scR));

        const auto detector = juce::jmax (fastEnv.processSample (rectified),
                                          slowEnv.processSample (rectified));

        // -- program dependent gain computer ---------------------------------
        const auto overDb = toDb (detector) - thresholdDb;
        const auto targetGain = dbToGain (softKneeGain (overDb, kneeDb, ratioForOvershoot (overDb)));

        currentGain = flushDenormal (targetGain + gainSmoothCoeff * (currentGain - targetGain));
        peakReduction = juce::jmax (peakReduction, -toDb (currentGain));

        // -- valve colour, tracking the gain reduction -----------------------
        const auto colour = harmonics * (0.35f + 0.65f * (1.0f - currentGain));
        const auto wetMix = mix.getNextValue();

        auto wetL = valveStage (dryL * currentGain * makeUp, colour);
        wetL = dcBlocker[0].processSample (transformer[0].processSample (wetL));
        left[i] = dryL + wetMix * (wetL - dryL);

        if (right != nullptr)
        {
            auto wetR = valveStage (dryR * currentGain * makeUp, colour);
            wetR = dcBlocker[1].processSample (transformer[1].processSample (wetR));
            right[i] = dryR + wetMix * (wetR - dryR);
        }
    }

    gainReductionDb.store (peakReduction);
}

} // namespace dr
