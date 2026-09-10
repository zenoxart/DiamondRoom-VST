#include "MetalKnob.h"

namespace dr
{

namespace
{
    constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
    constexpr float endAngle   = juce::MathConstants<float>::pi * 2.75f;

    // Fractions of the component height, in design units.
    constexpr float scaleRowHeight   = 26.0f;
    constexpr float captionRowHeight = 34.0f;
}

MetalKnob::MetalKnob (const juce::String& captionIn,
                      const juce::String& minLabelIn,
                      const juce::String& maxLabelIn)
    : caption (captionIn), minLabel (minLabelIn), maxLabel (maxLabelIn)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (startAngle, endAngle, true);
    setVelocityBasedMode (false);
    setMouseDragSensitivity (220);
    setWantsKeyboardFocus (false);
}

void MetalKnob::setDesignScale (float newScale)
{
    if (! juce::approximatelyEqual (scale, newScale))
    {
        scale = newScale;
        cachedDiameter = 0;
        repaint();
    }
}

juce::Rectangle<float> MetalKnob::getKnobBounds() const
{
    auto area = getLocalBounds().toFloat();
    area.removeFromBottom ((scaleRowHeight + captionRowHeight) * scale);

    // The tick ring lives outside the body, so leave room for it.
    const auto diameter = juce::jmin (area.getWidth(), area.getHeight()) * 0.74f;

    return juce::Rectangle<float> (diameter, diameter)
               .withCentre ({ area.getCentreX(), area.getCentreY() });
}

void MetalKnob::resized()
{
    cachedDiameter = 0;
}

void MetalKnob::paint (juce::Graphics& g)
{
    const auto knob = getKnobBounds();
    const auto centre = knob.getCentre();
    const auto radius = knob.getWidth() * 0.5f;

    if (radius < 3.0f)
        return;

    // -- ticks -------------------------------------------------------------
    theme::drawKnobTicks (g, centre, radius * 1.13f, 15, scale);

    // -- body --------------------------------------------------------------
    const auto diameter = juce::roundToInt (knob.getWidth());

    if (bodyImage.isNull() || cachedDiameter != diameter)
    {
        bodyImage = theme::createKnobBody (diameter, scale);
        cachedDiameter = diameter;
    }

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImageAt (bodyImage, juce::roundToInt (knob.getX()), juce::roundToInt (knob.getY()));

    // -- pointer -----------------------------------------------------------
    const auto range = getRange();
    const auto proportion = range.getLength() > 0.0
                                ? (float) ((getValue() - range.getStart()) / range.getLength())
                                : 0.0f;
    const auto angle = startAngle + proportion * (endAngle - startAngle);

    const auto sinA = std::sin (angle), cosA = std::cos (angle);
    const auto inner = radius * 0.38f;
    const auto outer = radius * 0.80f;
    const auto thickness = juce::jmax (1.5f, radius * 0.055f);

    const juce::Point<float> from { centre.x + sinA * inner, centre.y - cosA * inner };
    const juce::Point<float> to   { centre.x + sinA * outer, centre.y - cosA * outer };

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawLine ({ from.translated (0.0f, thickness * 0.6f), to.translated (0.0f, thickness * 0.6f) },
                thickness);

    g.setColour (theme::colours::pointerGlow);
    g.drawLine ({ from, to }, thickness * 2.1f);

    g.setColour (theme::colours::pointer);
    g.drawLine ({ from, to }, thickness);

    // -- engraved figures and caption --------------------------------------
    auto area = getLocalBounds().toFloat();
    auto captionArea = area.removeFromBottom (captionRowHeight * scale);
    auto scaleArea = area.removeFromBottom (scaleRowHeight * scale);

    const auto figureHeight = juce::jmax (7.0f, 21.0f * scale);
    const auto captionHeight = juce::jmax (8.0f, 25.0f * scale);

    if (minLabel.isNotEmpty() || maxLabel.isNotEmpty())
    {
        // The figures line up under the ends of the tick arc rather than the
        // edges of the component, which is wider to make room for the caption.
        const auto reach = radius * 1.12f;
        const auto figureWidth = juce::jmax (24.0f, radius * 0.9f);

        theme::drawEngravedText (g, minLabel,
                                 scaleArea.withX (centre.x - reach - figureWidth * 0.5f)
                                          .withWidth (figureWidth),
                                 juce::Justification::centred, figureHeight, false);
        theme::drawEngravedText (g, maxLabel,
                                 scaleArea.withX (centre.x + reach - figureWidth * 0.5f)
                                          .withWidth (figureWidth),
                                 juce::Justification::centred, figureHeight, false);
    }

    // Keep the caption inside its own column: the component is deliberately
    // wider than the knob, but the plate next door starts right after it.
    theme::drawEngravedText (g, caption.toUpperCase(),
                             captionArea.reduced (12.0f * scale, 0.0f),
                             juce::Justification::centredTop, captionHeight, true);
}

} // namespace dr
