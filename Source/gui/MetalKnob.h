#pragma once

#include "Theme.h"

namespace dr
{

/**
    A diamond-cut knob: the body is a photographic asset cropped from the
    reference mockup (with its pointer inpainted back out), a rotating accent
    pointer drawn on top, engraved tick marks and end-of-scale figures, and a
    caption underneath.

    The component owns its whole label block, so laying one out is a matter of
    handing it the rectangle from the panel artwork.
*/
class MetalKnob final : public juce::Slider
{
public:
    MetalKnob (const juce::String& caption,
               const juce::String& minLabel,
               const juce::String& maxLabel);

    void setDesignScale (float newScale);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::Rectangle<float> getKnobBounds() const;

    juce::String caption, minLabel, maxLabel;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetalKnob)
};

} // namespace dr
