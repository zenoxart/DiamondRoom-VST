#pragma once

#include "Theme.h"

namespace dr
{

/** The little red jewel lamp that arms each reverb. */
class LedButton final : public juce::Button
{
public:
    LedButton();

    void setDesignScale (float newScale) { scale = newScale; repaint(); }
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

private:
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LedButton)
};

//==============================================================================
/**
    One bolted-on plate of the rack panel. Draws its own steel, bevel and
    corner screws, and optionally carries an LED plus engraved title along the
    top edge. Controls are added as ordinary children.
*/
class PanelSection final : public juce::Component
{
public:
    explicit PanelSection (juce::String title = {});

    void setDesignScale (float newScale);
    void setTexture (const juce::Image& newTexture) { texture = newTexture; repaint(); }

    LedButton& getLed() noexcept { return led; }
    bool hasHeader() const noexcept { return title.isNotEmpty(); }

    /** The area inside the plate that controls should be laid out in. */
    juce::Rectangle<int> getContentArea() const;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::String title;
    juce::Image texture;
    LedButton led;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelSection)
};

} // namespace dr
