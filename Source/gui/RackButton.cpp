#include "RackButton.h"

namespace dr
{

namespace
{
    /** A curved arrow, drawn in a unit circle around the origin. */
    juce::Path makeCurvedArrow (bool mirrored)
    {
        constexpr auto pi = juce::MathConstants<float>::pi;
        const auto startAngle = pi * 0.36f;
        const auto endAngle   = pi * 1.72f;
        const auto radius = 0.82f;

        juce::Path arc;
        arc.addCentredArc (0.0f, 0.0f, radius, radius, 0.0f, startAngle, endAngle, true);

        juce::Path result;
        juce::PathStrokeType (0.24f, juce::PathStrokeType::curved,
                              juce::PathStrokeType::butt).createStrokedPath (result, arc);

        // Arrowhead at the start of the arc, pointing back along the tangent.
        // addCentredArc measures from straight up and runs clockwise, so the
        // tangent at an angle a is (cos a, sin a) and the tail points the
        // other way.
        const juce::Point<float> tip { std::sin (startAngle) * radius,
                                       -std::cos (startAngle) * radius };
        const juce::Point<float> along { -std::cos (startAngle), -std::sin (startAngle) };
        const juce::Point<float> across { along.y, -along.x };

        const auto length = 0.52f;
        const auto halfWidth = 0.34f;

        juce::Path head;
        head.addTriangle (tip + along * length,
                          tip + across * halfWidth,
                          tip - across * halfWidth);
        result.addPath (head);

        if (mirrored)
            result.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f));

        return result;
    }

    juce::Path makeChevron (bool pointingRight)
    {
        juce::Path chevron;
        chevron.startNewSubPath (0.45f, -0.8f);
        chevron.lineTo (-0.35f, 0.0f);
        chevron.lineTo (0.45f, 0.8f);

        juce::Path result;
        juce::PathStrokeType (0.36f, juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded).createStrokedPath (result, chevron);

        if (pointingRight)
            result.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f));

        return result;
    }

    juce::Path makeGear()
    {
        constexpr int numTeeth = 8;
        constexpr auto twoPi = juce::MathConstants<float>::twoPi;

        juce::Path gear;
        gear.setUsingNonZeroWinding (true);

        // Each tooth is a stub sitting on the rim rather than a bar through the
        // middle. A bar would cross the bore, and eight of them would wind the
        // centre up to nine, so the hole below could never cancel it back out.
        for (int i = 0; i < numTeeth; ++i)
        {
            juce::Path tooth;
            tooth.addRoundedRectangle (juce::Rectangle<float> (0.30f, 0.46f)
                                           .withCentre ({ 0.0f, -0.76f }), 0.07f);
            tooth.applyTransform (juce::AffineTransform::rotation (
                twoPi * (float) i / (float) numTeeth));
            gear.addPath (tooth);
        }

        gear.addEllipse (-0.74f, -0.74f, 1.48f, 1.48f);

        // The bore has to be wound the opposite way round to the body, so that
        // non-zero winding cancels it into a hole. addEllipse would wind it the
        // same way and simply fill it in, so it is stepped out by hand.
        constexpr auto bore = 0.34f;
        constexpr int segments = 48;

        gear.startNewSubPath (0.0f, -bore);

        for (int i = 1; i < segments; ++i)
        {
            const auto angle = -twoPi * (float) i / (float) segments;
            gear.lineTo (std::sin (angle) * bore, -std::cos (angle) * bore);
        }

        gear.closeSubPath();

        return gear;
    }
}

//==============================================================================
RackButton::RackButton (Glyph glyphToUse, juce::String textToUse)
    : juce::Button (textToUse.isNotEmpty() ? textToUse : "rack"),
      glyph (glyphToUse), text (std::move (textToUse))
{
    setWantsKeyboardFocus (false);
}

void RackButton::setDesignScale (float newScale)
{
    if (! juce::approximatelyEqual (scale, newScale))
    {
        scale = newScale;
        repaint();
    }
}

void RackButton::setText (const juce::String& newText)
{
    if (text != newText)
    {
        text = newText;
        repaint();
    }
}

juce::Path RackButton::getGlyphPath (juce::Rectangle<float> area) const
{
    juce::Path path;

    switch (glyph)
    {
        case Glyph::undo:     path = makeCurvedArrow (false); break;
        case Glyph::redo:     path = makeCurvedArrow (true);  break;
        case Glyph::previous: path = makeChevron (false);     break;
        case Glyph::next:     path = makeChevron (true);      break;
        case Glyph::gear:     path = makeGear();              break;
        case Glyph::none:
        default:              return path;
    }

    // The unit paths are drawn around the origin in a roughly 2x2 box.
    const auto size = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    path.applyTransform (juce::AffineTransform::scale (size)
                             .translated (area.getCentreX(), area.getCentreY()));
    return path;
}

void RackButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced (juce::jmax (1.0f, 2.0f * scale));
    const auto corner = juce::jmax (2.0f, 5.0f * scale);
    const auto enabled = isEnabled();

    // Recess the plate sits in.
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (bounds.expanded (1.5f * scale), corner);

    const auto top = down ? juce::Colour (0xff5c5b56) : juce::Colour (0xff858480);
    const auto bottom = down ? juce::Colour (0xff3d3c38) : juce::Colour (0xff4d4c48);

    juce::ColourGradient face (highlighted && enabled ? top.brighter (0.10f) : top,
                               bounds.getX(), bounds.getY(),
                               bottom, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (face);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (juce::Colours::white.withAlpha (down ? 0.08f : 0.20f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, juce::jmax (0.8f, 1.2f * scale));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (bounds.expanded (0.5f), corner, juce::jmax (0.8f, 1.2f * scale));

    auto content = bounds.reduced (juce::jmax (2.0f, 8.0f * scale));

    if (down)
        content.translate (0.0f, 0.8f * scale);

    const auto ink = theme::colours::textLight.withAlpha (enabled ? 0.92f : 0.35f);

    if (glyph != Glyph::none)
    {
        const auto path = getGlyphPath (content.withSizeKeepingCentre (
            juce::jmin (content.getWidth(), content.getHeight()),
            juce::jmin (content.getWidth(), content.getHeight())));

        g.setColour (juce::Colours::black.withAlpha (enabled ? 0.45f : 0.2f));
        g.fillPath (path, juce::AffineTransform::translation (0.0f, 1.2f * scale));
        g.setColour (ink);
        g.fillPath (path);
    }

    if (text.isNotEmpty())
    {
        auto textArea = content;

        if (glyph != Glyph::none)
            textArea = textArea.withTrimmedLeft (content.getHeight() + 4.0f * scale);

        theme::drawEngravedText (g, text, textArea, juce::Justification::centred,
                                 juce::jmax (8.0f, 24.0f * scale), false, ink);
    }
}

} // namespace dr
