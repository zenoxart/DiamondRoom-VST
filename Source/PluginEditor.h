#pragma once

#include "PluginProcessor.h"
#include "gui/MetalKnob.h"
#include "gui/MetalSlider.h"
#include "gui/PanelSection.h"
#include "gui/Theme.h"

//==============================================================================
/**
    The 4U rack panel: weathered steel, six bolted-on plates, and the same
    control set the Patcher surface exposes.
*/
class DiamondRoomAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DiamondRoomAudioProcessorEditor (DiamondRoomAudioProcessor&);
    ~DiamondRoomAudioProcessorEditor() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct ReverbStrip
    {
        ReverbStrip (juce::String title,
                     juce::String topCaption, juce::String topMin, juce::String topMax,
                     juce::String bottomCaption, juce::String bottomMin, juce::String bottomMax,
                     juce::String mixCaption)
            : panel (std::move (title)),
              topKnob (std::move (topCaption), std::move (topMin), std::move (topMax)),
              bottomKnob (std::move (bottomCaption), std::move (bottomMin), std::move (bottomMax)),
              mix (std::move (mixCaption), false)
        {
        }

        dr::PanelSection panel;
        dr::MetalKnob topKnob, bottomKnob;
        dr::MetalSlider mix;

        std::unique_ptr<SliderAttachment> topAttachment, bottomAttachment, mixAttachment;
        std::unique_ptr<ButtonAttachment> ledAttachment;
    };

    void buildBackground();
    void attachStrip (ReverbStrip& strip, const char* topId, const char* bottomId,
                      const char* mixId, const char* onId);
    void layoutStrip (ReverbStrip& strip, juce::Rectangle<int> panelBounds, float scale);
    void drawRackFrame (juce::Graphics& g, float scale);
    void drawTitle (juce::Graphics& g, float scale);

    DiamondRoomAudioProcessor& processor;

    dr::PanelSection drivePanel, tubePanel, mixPanel;
    dr::MetalKnob driveKnob { "Drive", "0", "10" };
    dr::MetalKnob tubeKnob  { "Tube",  "0", "10" };
    dr::MetalSlider masterMix { "Mix", true, "0", "100" };

    ReverbStrip hStrip     { "H-Reverb",       "Tone",       "-10", "+10", "Time",     "0", "10", "H-Mix" };
    ReverbStrip mannyStrip { "MannyM Reverb",  "Distortion", "0",   "10",  "Amount",   "0", "10", "MannyM Mix" };
    ReverbStrip valStrip   { "Valhalla Reverb","HighCut",    "0",   "10",  "Decay",    "0", "10", "Valhalla Mix" };
    ReverbStrip trueStrip  { "True Verb",      "Distance",   "0",   "10",  "Roomsize", "0", "10", "TrueVerb Mix" };

    std::unique_ptr<SliderAttachment> driveAttachment, tubeAttachment, mixAttachment;

    juce::Image panelTexture, plateTexture, railTexture;
    int textureWidth = 0, textureHeight = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiamondRoomAudioProcessorEditor)
};
