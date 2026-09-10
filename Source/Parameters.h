#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace dr::params
{

// Global
static constexpr const char* drive = "drive";
static constexpr const char* tube  = "tube";
static constexpr const char* mix   = "mix";

// H-Reverb
static constexpr const char* hOn   = "h_on";
static constexpr const char* hTone = "h_tone";
static constexpr const char* hTime = "h_time";
static constexpr const char* hMix  = "h_mix";

// MannyM Reverb
static constexpr const char* mOn     = "m_on";
static constexpr const char* mDist   = "m_dist";
static constexpr const char* mAmount = "m_amount";
static constexpr const char* mMix    = "m_mix";

// Valhalla Reverb
static constexpr const char* vOn      = "v_on";
static constexpr const char* vHighCut = "v_highcut";
static constexpr const char* vDecay   = "v_decay";
static constexpr const char* vMix     = "v_mix";

// TrueVerb
static constexpr const char* tOn       = "t_on";
static constexpr const char* tDistance = "t_distance";
static constexpr const char* tRoomsize = "t_roomsize";
static constexpr const char* tMix      = "t_mix";

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

//==============================================================================
/**
    Cached raw parameter pointers, so the audio thread never does a string
    lookup. Values are read once per block.
*/
struct Cache
{
    void attach (juce::AudioProcessorValueTreeState& state);

    std::atomic<float>* drive = nullptr;
    std::atomic<float>* tube  = nullptr;
    std::atomic<float>* mix   = nullptr;

    std::atomic<float>* hOn = nullptr;
    std::atomic<float>* hTone = nullptr;
    std::atomic<float>* hTime = nullptr;
    std::atomic<float>* hMix  = nullptr;

    std::atomic<float>* mOn = nullptr;
    std::atomic<float>* mDist = nullptr;
    std::atomic<float>* mAmount = nullptr;
    std::atomic<float>* mMix = nullptr;

    std::atomic<float>* vOn = nullptr;
    std::atomic<float>* vHighCut = nullptr;
    std::atomic<float>* vDecay = nullptr;
    std::atomic<float>* vMix = nullptr;

    std::atomic<float>* tOn = nullptr;
    std::atomic<float>* tDistance = nullptr;
    std::atomic<float>* tRoomsize = nullptr;
    std::atomic<float>* tMix = nullptr;
};

} // namespace dr::params
