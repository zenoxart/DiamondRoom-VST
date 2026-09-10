#include "MetalSlider.h"

namespace dr
{

namespace
{
    // Design units.
    constexpr float captionRow      = 34.0f;
    constexpr float figureRow       = 26.0f;
    constexpr float trackThickness  = 13.0f;
    constexpr float capLongSide     = 62.0f;
    constexpr float capShortSide    = 48.0f;

    // The master fader carries a heavier cap than the section faders.
    constexpr float masterCapExtraBreadth = 20.0f;
    constexpr float masterCapExtraLength  = 14.0f;

    // How far the graduations reach out from the track. The component itself
    // is wider than this so the caption underneath has room to breathe.
    constexpr float trackBreadth    = 84.0f;
    constexpr int   numGraduations  = 21;
}

juce::Slider::SliderLayout MetalSlider::TrackLayout::getSliderLayout (juce::Slider& slider)
{
    juce::Slider::SliderLayout layout;
    layout.sliderBounds = static_cast<MetalSlider&> (slider).getTrackArea().toNearestInt();
    layout.textBoxBounds = {};
    return layout;
}

MetalSlider::MetalSlider (const juce::String& captionIn, bool isHorizontal,
                          const juce::String& minLabelIn, const juce::String& maxLabelIn)
    : caption (captionIn), minLabel (minLabelIn), maxLabel (maxLabelIn),
      horizontal (isHorizontal)
{
    setName (caption);
    setSliderStyle (horizontal ? juce::Slider::LinearHorizontal : juce::Slider::LinearVertical);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setWantsKeyboardFocus (false);
    setLookAndFeel (&trackLayout);
}

MetalSlider::~MetalSlider()
{
    setLookAndFeel (nullptr);
}

void MetalSlider::setDesignScale (float newScale)
{
    if (! juce::approximatelyEqual (scale, newScale))
    {
        scale = newScale;
        cachedCapSize = {};

        // The track geometry is scale dependent, so the draggable region has
        // to be recalculated along with the artwork.
        resized();
        repaint();
    }
}

void MetalSlider::resized()
{
    // Slider::resized() is what derives the region the mouse is mapped onto.
    // Skipping it leaves that region one pixel wide.
    juce::Slider::resized();
    cachedCapSize = {};
}

juce::Rectangle<float> MetalSlider::getTrackArea() const
{
    auto area = getLocalBounds().toFloat();

    if (horizontal)
    {
        area.removeFromTop (captionRow * scale);
        area.removeFromBottom (figureRow * scale);

        // Inset by the cap's breadth so it stays inside the plate at both ends.
        const auto capBreadth = (capShortSide + masterCapExtraBreadth) * scale;
        return area.withSizeKeepingCentre (area.getWidth() - capBreadth, area.getHeight());
    }

    area.removeFromBottom (captionRow * scale);
    return area.withSizeKeepingCentre (juce::jmin (area.getWidth(), trackBreadth * scale),
                                       area.getHeight() - capShortSide * scale);
}

juce::Rectangle<float> MetalSlider::getCapBounds (juce::Rectangle<float> track) const
{
    const auto range = getRange();
    const auto proportion = range.getLength() > 0.0
                                ? (float) ((getValue() - range.getStart()) / range.getLength())
                                : 0.0f;

    if (horizontal)
    {
        const auto x = track.getX() + proportion * track.getWidth();
        return juce::Rectangle<float> ((capShortSide + masterCapExtraBreadth) * scale,
                                       (capLongSide + masterCapExtraLength) * scale)
                   .withCentre ({ x, track.getCentreY() });
    }

    const auto y = track.getBottom() - proportion * track.getHeight();
    return juce::Rectangle<float> (capLongSide * scale, capShortSide * scale)
               .withCentre ({ track.getCentreX(), y });
}

void MetalSlider::paint (juce::Graphics& g)
{
    const auto track = getTrackArea();

    if (track.getWidth() < 2.0f || track.getHeight() < 2.0f)
        return;

    // -- graduations -------------------------------------------------------
    {
        g.setColour (theme::colours::engraveDark.withAlpha (0.7f));
        const auto tickThickness = juce::jmax (1.0f, 2.2f * scale);

        // Graduations run out towards the edges of the plate, so they stay in
        // proportion whatever shape the fader is.
        const auto reach = (horizontal ? track.getHeight() : trackBreadth * scale) * 0.5f;

        for (int i = 0; i < numGraduations; ++i)
        {
            const auto t = (float) i / (float) (numGraduations - 1);
            const auto major = (i % 5) == 0;
            const auto gap = reach * 0.13f;
            const auto length = reach * (major ? 0.72f : 0.48f);

            if (horizontal)
            {
                const auto x = track.getX() + t * track.getWidth();
                g.drawLine (x, track.getCentreY() - gap - length,
                            x, track.getCentreY() - gap, tickThickness);
                g.drawLine (x, track.getCentreY() + gap,
                            x, track.getCentreY() + gap + length, tickThickness);
            }
            else
            {
                const auto y = track.getBottom() - t * track.getHeight();
                g.drawLine (track.getCentreX() - gap - length, y,
                            track.getCentreX() - gap, y, tickThickness);
                g.drawLine (track.getCentreX() + gap, y,
                            track.getCentreX() + gap + length, y, tickThickness);
            }
        }
    }

    // -- recessed track ----------------------------------------------------
    {
        const auto thickness = trackThickness * scale;
        const auto slot = horizontal
                              ? juce::Rectangle<float> (track.getX(), track.getCentreY() - thickness * 0.5f,
                                                        track.getWidth(), thickness)
                              : juce::Rectangle<float> (track.getCentreX() - thickness * 0.5f, track.getY(),
                                                        thickness, track.getHeight());

        const auto corner = thickness * 0.4f;

        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.fillRoundedRectangle (slot.translated (0.0f, juce::jmax (1.0f, 1.5f * scale)), corner);

        juce::ColourGradient grad (juce::Colour (0xff141412),
                                   slot.getX(), slot.getY(),
                                   juce::Colour (0xff36352f),
                                   horizontal ? slot.getX() : slot.getRight(),
                                   horizontal ? slot.getBottom() : slot.getY(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (slot, corner);
    }

    // -- cap ---------------------------------------------------------------
    {
        const auto cap = getCapBounds (track);
        const juce::Rectangle<int> capSize { juce::roundToInt (cap.getWidth()),
                                             juce::roundToInt (cap.getHeight()) };

        if (capImage.isNull() || cachedCapSize != capSize)
        {
            capImage = theme::createFaderCap (capSize.getWidth(), capSize.getHeight(),
                                              horizontal, scale);
            cachedCapSize = capSize;
        }

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageAt (capImage, juce::roundToInt (cap.getX()), juce::roundToInt (cap.getY()));
    }

    // -- lettering ---------------------------------------------------------
    auto area = getLocalBounds().toFloat();

    // Fader captions are the longest lettering on the panel, so they are cut a
    // size down from the knob captions to keep neighbouring plates legible.
    const auto captionHeight = juce::jmax (7.0f, (horizontal ? 25.0f : 21.0f) * scale);
    const auto figureHeight = juce::jmax (7.0f, 21.0f * scale);

    if (horizontal)
    {
        theme::drawEngravedText (g, caption.toUpperCase(), area.removeFromTop (captionRow * scale),
                                 juce::Justification::centred, captionHeight, true);

        // Figures sit under the ends of the track, not the component.
        const auto figures = area.removeFromBottom (figureRow * scale);
        const auto figureWidth = 90.0f * scale;

        theme::drawEngravedText (g, minLabel,
                                 figures.withX (track.getX() - figureWidth * 0.5f)
                                        .withWidth (figureWidth),
                                 juce::Justification::centred, figureHeight, false);
        theme::drawEngravedText (g, maxLabel,
                                 figures.withX (track.getRight() - figureWidth * 0.5f)
                                        .withWidth (figureWidth),
                                 juce::Justification::centred, figureHeight, false);
    }
    else
    {
        theme::drawEngravedText (g, caption.toUpperCase(),
                                 area.removeFromBottom (captionRow * scale).reduced (14.0f * scale, 0.0f),
                                 juce::Justification::centredTop, captionHeight, true);
    }
}

} // namespace dr
