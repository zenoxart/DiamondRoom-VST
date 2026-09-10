#pragma once

#include "Theme.h"

namespace dr

{

/**
    The rack faders: a recessed track with engraved graduations either side and
    a ridged metal cap. Used vertically for the four reverb mixes and
    horizontally for the master Mix at the bottom of the panel.
*/
class MetalSlider final : public juce::Slider
{
public:
    MetalSlider (const juce::String& caption, bool horizontal,
                 const juce::String& minLabel = {}, const juce::String& maxLabel = {});

    void setDesignScale (float newScale);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::Rectangle<float> getTrackArea() const;
    juce::Rectangle<float> getCapBounds (juce::Rectangle<float> track) const;

    juce::String caption, minLabel, maxLabel;
    bool horizontal = false;

    juce::Image capImage;
    juce::Rectangle<int> cachedCapSize;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetalSlider)
};

} // namespace dr
