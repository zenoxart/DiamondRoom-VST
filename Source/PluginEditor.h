#pragma once

#include "PluginProcessor.h"
#include "gui/MetalKnob.h"
#include "gui/MetalSlider.h"
#include "gui/PanelSection.h"
#include "gui/RackButton.h"
#include "gui/Theme.h"

//==============================================================================
/**
    The 4U rack panel: weathered steel, six bolted-on plates, and the same
    control set the Patcher surface exposes, with preset, undo and window size
    controls along the top rail.
*/
class DiamondRoomAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::ChangeListener
{
public:
    explicit DiamondRoomAudioProcessorEditor (DiamondRoomAudioProcessor&);
    ~DiamondRoomAudioProcessorEditor() override;

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

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    void buildBackground();
    void attachStrip (ReverbStrip& strip, const char* topId, const char* bottomId,
                      const char* mixId, const char* onId);
    void layoutStrip (ReverbStrip& strip, juce::Rectangle<int> panelBounds, float scale);
    void drawRackFrame (juce::Graphics& g, float scale);
    void drawTitle (juce::Graphics& g, float scale);

    /** Every control begins an undo transaction when its gesture starts, so
        one drag is one step rather than a hundred. */
    void markUndoPoint (const juce::String& description);
    void wireUndoTransactions();

    void updatePresetDisplay();
    void updateUndoButtons();
    void showPresetMenu();
    void showSettingsMenu();
    void showSavePresetDialog();
    void showMessage (const juce::String& title, const juce::String& message);
    void applyWidth (int width);

    DiamondRoomAudioProcessor& processor;

    dr::PanelSection drivePanel, tubePanel, mixPanel;
    dr::MetalKnob driveKnob { "Drive", "0", "10" };
    dr::MetalKnob tubeKnob  { "Tube",  "0", "10" };
    dr::MetalSlider masterMix { "Mix", true, "0", "100" };

    ReverbStrip hStrip     { "H-Reverb",       "Tone",       "-10", "+10", "Time",     "0", "10", "H-Mix" };
    ReverbStrip mannyStrip { "MannyM Reverb",  "Distortion", "0",   "10",  "Amount",   "0", "10", "MannyM Mix" };
    ReverbStrip valStrip   { "Valhalla Reverb","HighCut",    "0",   "10",  "Decay",    "0", "10", "Valhalla Mix" };
    ReverbStrip trueStrip  { "True Verb",      "Distance",   "0",   "10",  "Roomsize", "0", "10", "TrueVerb Mix" };

    dr::RackButton undoButton   { dr::RackButton::Glyph::undo };
    dr::RackButton redoButton   { dr::RackButton::Glyph::redo };
    dr::RackButton prevPreset   { dr::RackButton::Glyph::previous };
    dr::RackButton nextPreset   { dr::RackButton::Glyph::next };
    dr::RackButton presetPlate  { dr::RackButton::Glyph::none, "Diamond Room" };
    dr::RackButton settingsButton { dr::RackButton::Glyph::gear };

    std::unique_ptr<SliderAttachment> driveAttachment, tubeAttachment, mixAttachment;
    std::unique_ptr<juce::AlertWindow> dialog;

    juce::Image panelTexture, plateTexture;
    int textureWidth = 0, textureHeight = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiamondRoomAudioProcessorEditor)
};
