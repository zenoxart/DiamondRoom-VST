#include "Parameters.h"

namespace dr::params
{

using APVTS = juce::AudioProcessorValueTreeState;

namespace
{
    juce::String oneDecimal (float v, int)
    {
        return juce::String (v, 1);
    }

    juce::String percent (float v, int)
    {
        return juce::String (juce::roundToInt (v)) + " %";
    }

    std::unique_ptr<juce::AudioParameterFloat> knob (const char* id, const juce::String& name,
                                                     float lo, float hi, float def,
                                                     std::function<juce::String (float, int)> fmt)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> (lo, hi, 0.01f), def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (fmt)));
    }
}

APVTS::ParameterLayout createLayout()
{
    APVTS::ParameterLayout layout;

    // -- global ------------------------------------------------------------
    layout.add (knob (drive, "Drive", 0.0f, 10.0f, 3.0f,  oneDecimal));
    layout.add (knob (tube,  "Tube",  0.0f, 10.0f, 3.5f,  oneDecimal));
    layout.add (knob (mix,   "Mix",   0.0f, 100.0f, 35.0f, percent));

    // -- H-Reverb ----------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { hOn, 1 }, "H-Reverb On", true));
    layout.add (knob (hTone, "H-Reverb Tone", -10.0f, 10.0f, 0.0f, oneDecimal));
    layout.add (knob (hTime, "H-Reverb Time", 0.0f, 10.0f, 6.9f, oneDecimal));
    layout.add (knob (hMix,  "H-Reverb Mix",  0.0f, 100.0f, 40.0f, percent));

    // -- MannyM Reverb -----------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { mOn, 1 }, "MannyM Reverb On", true));
    layout.add (knob (mDist,   "MannyM Distortion", 0.0f, 10.0f, 2.0f, oneDecimal));
    layout.add (knob (mAmount, "MannyM Amount",     0.0f, 10.0f, 4.5f, oneDecimal));
    layout.add (knob (mMix,    "MannyM Mix",        0.0f, 100.0f, 40.0f, percent));

    // -- Valhalla ----------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { vOn, 1 }, "Valhalla Reverb On", true));
    layout.add (knob (vHighCut, "Valhalla HighCut", 0.0f, 10.0f, 9.3f, oneDecimal));
    layout.add (knob (vDecay,   "Valhalla Decay",   0.0f, 10.0f, 5.9f, oneDecimal));
    layout.add (knob (vMix,     "Valhalla Mix",     0.0f, 100.0f, 40.0f, percent));

    // -- TrueVerb ----------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { tOn, 1 }, "True Verb On", true));
    layout.add (knob (tDistance, "True Verb Distance", 0.0f, 10.0f, 2.6f, oneDecimal));
    layout.add (knob (tRoomsize, "True Verb Roomsize", 0.0f, 10.0f, 7.8f, oneDecimal));
    layout.add (knob (tMix,      "True Verb Mix",      0.0f, 100.0f, 40.0f, percent));

    return layout;
}

void Cache::attach (juce::AudioProcessorValueTreeState& state)
{
    auto get = [&state] (const char* id) { return state.getRawParameterValue (id); };

    drive = get (params::drive);
    tube  = get (params::tube);
    mix   = get (params::mix);

    hOn   = get (params::hOn);
    hTone = get (params::hTone);
    hTime = get (params::hTime);
    hMix  = get (params::hMix);

    mOn     = get (params::mOn);
    mDist   = get (params::mDist);
    mAmount = get (params::mAmount);
    mMix    = get (params::mMix);

    vOn      = get (params::vOn);
    vHighCut = get (params::vHighCut);
    vDecay   = get (params::vDecay);
    vMix     = get (params::vMix);

    tOn       = get (params::tOn);
    tDistance = get (params::tDistance);
    tRoomsize = get (params::tRoomsize);
    tMix      = get (params::tMix);
}

} // namespace dr::params
