#include "PanelSection.h"

namespace dr
{

namespace
{
    constexpr float headerHeight = 62.0f;  // design units
    constexpr float ledInset     = 26.0f;
    constexpr float ledDiameter  = 30.0f;
}

//==============================================================================
LedButton::LedButton() : juce::Button ("led")
{
    setClickingTogglesState (true);
    setWantsKeyboardFocus (false);
}

void LedButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat();
    const auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto lens = juce::Rectangle<float> (diameter, diameter).withCentre (bounds.getCentre());

    const auto on = getToggleState();

    // Just a hint of a dark socket - the reference reads as a bare glowing
    // orb sitting on the plate, not a lamp set into a chrome bezel.
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (lens);

    lens = lens.reduced (juce::jmax (1.0f, diameter * 0.08f));

    if (on)
    {
        // Halo around a lit lamp.
        const auto halo = lens.expanded (diameter * 0.55f);
        juce::ColourGradient glow (theme::colours::ledOn.withAlpha (0.70f),
                                   halo.getCentreX(), halo.getCentreY(),
                                   theme::colours::ledOn.withAlpha (0.0f),
                                   halo.getRight(), halo.getCentreY(), true);
        g.setGradientFill (glow);
        g.fillEllipse (halo);
    }

    const auto base = on ? theme::colours::ledOn : theme::colours::ledOff;
    const auto lit = highlighted ? base.brighter (0.15f) : base;

    juce::ColourGradient lensFill (lit.brighter (on ? 0.7f : 0.25f),
                                   lens.getX() + lens.getWidth() * 0.3f,
                                   lens.getY() + lens.getHeight() * 0.25f,
                                   lit.darker (0.65f),
                                   lens.getRight(), lens.getBottom(), true);
    g.setGradientFill (lensFill);
    g.fillEllipse (lens);

    // Specular dot.
    g.setColour (juce::Colours::white.withAlpha (on ? 0.75f : 0.28f));
    const auto dot = lens.getWidth() * 0.22f;
    g.fillEllipse (lens.getX() + lens.getWidth() * 0.24f,
                   lens.getY() + lens.getHeight() * 0.18f, dot, dot);

    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (lens);
    }
}

//==============================================================================
PanelSection::PanelSection (juce::String titleIn) : title (std::move (titleIn))
{
    setInterceptsMouseClicks (false, true);

    if (hasHeader())
        addAndMakeVisible (led);
}

void PanelSection::setDesignScale (float newScale)
{
    scale = newScale;
    led.setDesignScale (newScale);
    resized();
    repaint();
}

juce::Rectangle<int> PanelSection::getContentArea() const
{
    auto area = getLocalBounds().reduced (juce::roundToInt (10.0f * scale));

    if (hasHeader())
        area.removeFromTop (juce::roundToInt (headerHeight * scale));

    return area;
}

void PanelSection::resized()
{
    if (! hasHeader())
        return;

    const auto d = juce::roundToInt (ledDiameter * scale);
    const auto inset = juce::roundToInt (ledInset * scale);
    led.setBounds (inset, juce::roundToInt (18.0f * scale), d, d);
}

void PanelSection::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (2.0f * scale);
    theme::drawSubPanel (g, bounds, texture, scale);

    if (! hasHeader())
        return;

    const auto textLeft = (float) led.getRight() + 12.0f * scale;
    auto header = juce::Rectangle<float> (textLeft, (float) led.getY(),
                                          bounds.getRight() - textLeft - 12.0f * scale,
                                          (float) led.getHeight());

    theme::drawEngravedText (g, title.toUpperCase(), header, juce::Justification::centredLeft,
                             juce::jmax (8.0f, 24.0f * scale), true);
}

} // namespace dr
