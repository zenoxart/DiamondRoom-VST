#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "dsp/HReverb.h"
#include "dsp/MannyMReverb.h"
#include "dsp/OneKnobDriver.h"
#include "dsp/PuigChild.h"
#include "dsp/TrueVerb.h"
#include "dsp/ValhallaVerb.h"

//==============================================================================
/**
    Diamond Room - the Patcher chain rebuilt as a single plugin:

        in -> Driver -> [ H-Reverb | MannyM | Valhalla | TrueVerb ] -> Sum
                              (each with its own mix)                  |
                                                                       v
        out <- Mix (dry/wet) <------------ Sum + PuigChild 670 (Tube blend)
*/
class DiamondRoomAudioProcessor final : public juce::AudioProcessor
{
public:
    DiamondRoomAudioProcessor();
    ~DiamondRoomAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    float getGainReductionDb() const noexcept { return tube.getGainReductionDb(); }

private:
    void updateParameters();

    juce::AudioProcessorValueTreeState state;
    dr::params::Cache cache;

    dr::OneKnobDriver driver;
    dr::HReverb hReverb;
    dr::MannyMReverb mannyReverb;
    dr::ValhallaVerb valhallaReverb;
    dr::TrueVerb trueVerb;
    dr::PuigChild tube;

    juce::AudioBuffer<float> dryBuffer, drivenBuffer, wetBuffer, sumBuffer;
    int maxBlockSize = 512;

    // Keeps the dry path aligned with the oversamplers in the wet path.
    juce::AudioBuffer<float> dryDelayBuffer;
    int dryDelayLength = 0, dryDelayWritePos = 0;

    juce::SmoothedValue<float> hMixSmooth, mMixSmooth, vMixSmooth, tMixSmooth;
    juce::SmoothedValue<float> tubeBlendSmooth, masterMixSmooth;

    bool hActive = true, mActive = true, vActive = true, tActive = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiamondRoomAudioProcessor)
};
