#include "MetalSlider.h"
#include <BinaryData.h>

namespace dr
{

namespace
{
    // Design units.
    constexpr float captionRow      = 34.0f;
    constexpr float figureRow       = 26.0f;
    constexpr float trackThickness  = 8.0f;
    constexpr float masterTrackThickness = 9.0f;

    // The "major" dimension of each gem, in design units - its height, since
    // both cuts stand taller than they are wide. The other dimension is
    // derived from the asset's own pixel aspect ratio, so the cut stays
    // faithful to the source crop rather than an assumed rectangle.
    constexpr float sectionCapTargetHeight = 52.0f;
    constexpr float masterCapTargetHeight  = 74.0f;

    // How far the graduations reach out from the track. The component itself
    // is wider than this so the caption underneath has room to breathe.
    constexpr float trackBreadth    = 84.0f;
    constexpr int   numGraduations  = 21;

    /** Gem caps cropped from the reference mockup: a squarer pillow cut for
        the section faders, a tall baguette for the master one. Decoded once
        and shared by every fader on the panel. */
    const juce::Image& sectionGemAsset()
    {
        static const juce::Image image = juce::ImageCache::getFromMemory (
            BinaryData::FaderGemSection_png, BinaryData::FaderGemSection_pngSize);
        return image;
    }

    const juce::Image& masterGemAsset()
    {
        static const juce::Image image = juce::ImageCache::getFromMemory (
            BinaryData::FaderGemMaster_png, BinaryData::FaderGemMaster_pngSize);
        return image;
    }
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
}

juce::Rectangle<float> MetalSlider::getCapSize() const
{
    const auto& asset = horizontal ? masterGemAsset() : sectionGemAsset();
    const auto aspect = asset.isValid() && asset.getHeight() > 0
                             ? (float) asset.getWidth() / (float) asset.getHeight()
                             : 1.0f;

    const auto height = (horizontal ? masterCapTargetHeight : sectionCapTargetHeight) * scale;
    return { height * aspect, height };
}

juce::Rectangle<float> MetalSlider::getTrackArea() const
{
    auto area = getLocalBounds().toFloat();

    if (horizontal)
    {
        area.removeFromTop (captionRow * scale);
        area.removeFromBottom (figureRow * scale);

        // Inset by the cap's width so it stays inside the plate at both ends.
        return area.withSizeKeepingCentre (area.getWidth() - getCapSize().getWidth(), area.getHeight());
    }

    area.removeFromBottom (captionRow * scale);
    return area.withSizeKeepingCentre (juce::jmin (area.getWidth(), trackBreadth * scale),
                                       area.getHeight() - getCapSize().getHeight());
}

juce::Rectangle<float> MetalSlider::getCapBounds (juce::Rectangle<float> track) const
{
    const auto range = getRange();
    const auto proportion = range.getLength() > 0.0
                                ? (float) ((getValue() - range.getStart()) / range.getLength())
                                : 0.0f;

    const auto size = getCapSize();

    if (horizontal)
    {
        const auto x = track.getX() + proportion * track.getWidth();
        return size.withCentre ({ x, track.getCentreY() });
    }

    const auto y = track.getBottom() - proportion * track.getHeight();
    return size.withCentre ({ track.getCentreX(), y });
}

void MetalSlider::paint (juce::Graphics& g)
{
    const auto track = getTrackArea();

    if (track.getWidth() < 2.0f || track.getHeight() < 2.0f)
        return;

    // -- graduations -------------------------------------------------------
    {
        g.setColour (theme::colours::text.withAlpha (0.55f));
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
        const auto thickness = (horizontal ? masterTrackThickness : trackThickness) * scale;
        const auto slot = horizontal
                              ? juce::Rectangle<float> (track.getX(), track.getCentreY() - thickness * 0.5f,
                                                        track.getWidth(), thickness)
                              : juce::Rectangle<float> (track.getCentreX() - thickness * 0.5f, track.getY(),
                                                        thickness, track.getHeight());

        const auto corner = thickness * 0.45f;

        g.setColour (theme::colours::panelEdge.withAlpha (0.22f));
        g.fillRoundedRectangle (slot.translated (0.0f, juce::jmax (1.0f, 1.5f * scale)), corner);

        juce::ColourGradient grad (juce::Colour (0xff05080d),
                                   slot.getX(), slot.getY(),
                                   juce::Colour (0xff1b2531),
                                   horizontal ? slot.getX() : slot.getRight(),
                                   horizontal ? slot.getBottom() : slot.getY(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (slot, corner);

        // The master fader reads as a lit hairline running the width of the
        // panel - a thin glowing wire rather than a thick neon bar - while the
        // section faders stay dark so they do not compete with it.
        if (horizontal)
        {
            const auto filament = slot.withSizeKeepingCentre (slot.getWidth(), thickness * 0.16f);

            for (int pass = 2; pass >= 1; --pass)
            {
                g.setColour (theme::colours::accent.withAlpha (0.09f * (float) pass));
                g.fillRoundedRectangle (filament.expanded (0.0f, thickness * 0.42f * (float) pass),
                                        corner);
            }

            g.setColour (theme::colours::accent.withAlpha (0.95f));
            g.fillRoundedRectangle (filament, filament.getHeight() * 0.5f);
        }
    }

    // -- cap: a gem cropped from the reference mockup -----------------------
    {
        const auto cap = getCapBounds (track);
        const auto& asset = horizontal ? masterGemAsset() : sectionGemAsset();

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (asset, cap, juce::RectanglePlacement::stretchToFit);
    }

    // -- lettering ---------------------------------------------------------
    auto area = getLocalBounds().toFloat();

    // Fader captions are the longest lettering on the panel, so they are cut a
    // size down from the knob captions to keep neighbouring plates legible.
    const auto captionHeight = juce::jmax (6.0f, (horizontal ? 25.0f : 16.0f) * scale);
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
                                 area.removeFromBottom (captionRow * scale).reduced (26.0f * scale, 0.0f),
                                 juce::Justification::centredTop, captionHeight, true);
    }
}

} // namespace dr
