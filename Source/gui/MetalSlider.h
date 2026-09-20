#pragma once

#include "Theme.h"

namespace dr

{

/**
    The rack faders: a recessed track with engraved graduations either side and
    a gem cap cropped from the reference mockup. Used vertically for the four
    reverb mixes and horizontally for the master Mix at the bottom of the
    panel - a differently proportioned gem for each, since the two are not the
    same stone scaled up: the master cap is a tall baguette cut, the section
    caps a squarer pillow cut.
*/
class MetalSlider final : public juce::Slider
{
public:
    MetalSlider (const juce::String& caption, bool horizontal,
                 const juce::String& minLabel = {}, const juce::String& maxLabel = {});
    ~MetalSlider() override;

    void setDesignScale (float newScale);

    /** The span the cap centre travels along. JUCE maps the mouse onto this
        same rectangle, so what is drawn and what is dragged stay in step. */
    juce::Rectangle<float> getTrackArea() const;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    /** Hands JUCE the drawn track as the draggable region. */
    struct TrackLayout final : juce::LookAndFeel_V4
    {
        juce::Slider::SliderLayout getSliderLayout (juce::Slider& slider) override;
    };

    juce::Rectangle<float> getCapBounds (juce::Rectangle<float> track) const;
    juce::Rectangle<float> getCapSize() const;

    juce::String caption, minLabel, maxLabel;
    bool horizontal = false;

    TrackLayout trackLayout;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetalSlider)
};

} // namespace dr
