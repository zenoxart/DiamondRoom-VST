#pragma once

#include "Theme.h"

namespace dr
{

/**
    The small bolted-on switches along the top of the panel: undo, redo, preset
    stepping, the preset name plate and the settings gear. All the same
    recessed metal plate, carrying either an engraved glyph or a line of text.
*/
class RackButton final : public juce::Button
{
public:
    enum class Glyph { none, undo, redo, previous, next, gear };

    explicit RackButton (Glyph glyph, juce::String text = {});

    void setDesignScale (float newScale);
    void setText (const juce::String& newText);

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

private:
    juce::Path getGlyphPath (juce::Rectangle<float> area) const;

    Glyph glyph;
    juce::String text;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackButton)
};

} // namespace dr
