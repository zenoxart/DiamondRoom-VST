#include "Theme.h"

namespace dr::theme
{

namespace
{
    //==========================================================================
    /** Value noise with smooth interpolation - the base for every texture. */
    struct ValueNoise
    {
        explicit ValueNoise (int seed) : random (seed)
        {
            for (auto& v : table)
                v = random.nextFloat();

            for (int i = 0; i < tableSize; ++i)
                permutation[i] = i;

            for (int i = tableSize - 1; i > 0; --i)
                std::swap (permutation[i], permutation[random.nextInt (i + 1)]);
        }

        float at (float x, float y) const noexcept
        {
            const auto xi = (int) std::floor (x);
            const auto yi = (int) std::floor (y);
            const auto tx = x - (float) xi;
            const auto ty = y - (float) yi;

            const auto sx = tx * tx * (3.0f - 2.0f * tx);
            const auto sy = ty * ty * (3.0f - 2.0f * ty);

            const auto c00 = valueAt (xi,     yi);
            const auto c10 = valueAt (xi + 1, yi);
            const auto c01 = valueAt (xi,     yi + 1);
            const auto c11 = valueAt (xi + 1, yi + 1);

            const auto a = c00 + sx * (c10 - c00);
            const auto b = c01 + sx * (c11 - c01);
            return a + sy * (b - a);
        }

        /** Fractal sum - several octaves of the above. */
        float fbm (float x, float y, int octaves) const noexcept
        {
            float sum = 0.0f, amplitude = 1.0f, norm = 0.0f, frequency = 1.0f;

            for (int i = 0; i < octaves; ++i)
            {
                sum += amplitude * at (x * frequency, y * frequency);
                norm += amplitude;
                amplitude *= 0.5f;
                frequency *= 2.0f;
            }

            return sum / juce::jmax (1.0e-6f, norm);
        }

    private:
        static constexpr int tableSize = 256;
        static constexpr int mask = tableSize - 1;

        float valueAt (int x, int y) const noexcept
        {
            const auto i = permutation[(size_t) (x & mask)];
            const auto j = permutation[(size_t) ((y + i) & mask)];
            return table[(size_t) j];
        }

        juce::Random random;
        std::array<float, tableSize> table {};
        std::array<int, tableSize> permutation {};
    };

    inline juce::uint8 toByte (float v) noexcept
    {
        return (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (v * 255.0f));
    }
}

//==============================================================================
juce::Image createPanelTexture (int width, int height, int seed, float rustAmount)
{
    juce::Image image (juce::Image::ARGB, juce::jmax (1, width), juce::jmax (1, height), false);
    juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);

    const ValueNoise coarse (seed);
    const ValueNoise rustNoise (seed * 7919 + 13);
    const ValueNoise grain (seed * 104729 + 7);

    const auto w = (float) width;
    const auto h = (float) height;

    for (int y = 0; y < height; ++y)
    {
        auto* line = data.getLinePointer (y);
        const auto fy = (float) y / h;

        for (int x = 0; x < width; ++x)
        {
            const auto fx = (float) x / w;

            // Large scale mottling of the steel itself.
            const auto blotch = coarse.fbm (fx * 7.0f, fy * 5.0f, 5);

            // Brushed streaks running along the panel.
            const auto streak = grain.at (fx * 900.0f, fy * 22.0f) - 0.5f;

            // Fine grain, so the surface never looks flat.
            const auto speck = grain.at (fx * w * 0.9f, fy * h * 0.9f) - 0.5f;

            auto luminance = 0.36f + 0.17f * blotch + 0.035f * streak + 0.045f * speck;

            // Slight top-to-bottom lighting, plus darker edges.
            luminance += 0.05f * (1.0f - fy);
            const auto edge = juce::jmin (juce::jmin (fx, 1.0f - fx) * 12.0f,
                                          juce::jmin (fy, 1.0f - fy) * 12.0f);
            luminance *= 0.65f + 0.35f * juce::jlimit (0.0f, 1.0f, edge);

            auto r = luminance * 1.03f;
            auto gr = luminance * 1.00f;
            auto b = luminance * 0.95f;

            // Rust blooms: patches where the noise crosses a threshold.
            if (rustAmount > 0.0f)
            {
                const auto patch = rustNoise.fbm (fx * 4.5f + 3.1f, fy * 3.5f + 1.7f, 5);
                const auto detail = rustNoise.fbm (fx * 26.0f, fy * 20.0f, 3);
                auto corrosion = juce::jlimit (0.0f, 1.0f, (patch - 0.55f) * 4.2f)
                                   * juce::jlimit (0.0f, 1.0f, (detail - 0.32f) * 2.2f);

                // Corrosion creeps in from the edges, where the plating wears.
                const auto fromEdge = juce::jmin (juce::jmin (fx, 1.0f - fx),
                                                  juce::jmin (fy, 1.0f - fy));
                corrosion *= rustAmount * (0.35f + 0.65f * juce::jlimit (0.0f, 1.0f, 1.0f - fromEdge * 3.2f));

                const auto rustR = 0.46f * luminance / 0.32f;
                r  += corrosion * (juce::jlimit (0.0f, 1.0f, rustR * 0.95f) - r);
                gr += corrosion * (juce::jlimit (0.0f, 1.0f, rustR * 0.55f) - gr);
                b  += corrosion * (juce::jlimit (0.0f, 1.0f, rustR * 0.32f) - b);
            }

            auto* pixel = (juce::PixelARGB*) (line + x * data.pixelStride);
            pixel->setARGB (255, toByte (r), toByte (gr), toByte (b));
        }
    }

    return image;
}

//==============================================================================
juce::Image createKnobBody (int diameter, float scale)
{
    const auto size = juce::jmax (8, diameter);
    juce::Image image (juce::Image::ARGB, size, size, true);

    juce::Graphics g (image);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    const auto full = juce::Rectangle<float> (0.0f, 0.0f, (float) size, (float) size);
    const auto inset = juce::jmax (1.0f, 2.0f * scale);
    const auto body = full.reduced (inset);
    const auto centre = full.getCentre();
    const auto radius = body.getWidth() * 0.5f;

    // Drop shadow under the rim.
    {
        juce::DropShadow shadow (juce::Colours::black.withAlpha (0.55f),
                                 (int) juce::jmax (2.0f, 6.0f * scale),
                                 { 0, (int) juce::jmax (1.0f, 3.0f * scale) });
        juce::Path circle;
        circle.addEllipse (body);
        shadow.drawForPath (g, circle);
    }

    // Outer chamfer ring.
    {
        juce::ColourGradient ring (juce::Colour (0xffe7e7e5), body.getX(), body.getY(),
                                   juce::Colour (0xff53534f), body.getRight(), body.getBottom(), false);
        g.setGradientFill (ring);
        g.fillEllipse (body);
    }

    // Main face, lit from the upper left.
    const auto face = body.reduced (juce::jmax (1.0f, 3.0f * scale));
    {
        juce::ColourGradient grad (juce::Colour (0xfff2f2f0),
                                   face.getX() + face.getWidth() * 0.18f,
                                   face.getY() + face.getHeight() * 0.10f,
                                   juce::Colour (0xff8e8e8a),
                                   face.getRight(), face.getBottom(), true);
        grad.addColour (0.55, juce::Colour (0xffc9c9c5));
        g.setGradientFill (grad);
        g.fillEllipse (face);
    }

    // The diagonal split that gives these knobs their machined look: the
    // lower right half sits in shadow behind a hard edge.
    {
        juce::Path lower;
        lower.startNewSubPath (face.getX() - 1.0f, face.getBottom() + 1.0f);
        lower.lineTo (face.getRight() + 1.0f, face.getY() - 1.0f);
        lower.lineTo (face.getRight() + 1.0f, face.getBottom() + 1.0f);
        lower.closeSubPath();

        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addEllipse (face);
        g.reduceClipRegion (clip);

        juce::ColourGradient grad (juce::Colour (0x00000000),
                                   face.getCentreX(), face.getCentreY(),
                                   juce::Colour (0x59000000),
                                   face.getRight(), face.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (lower);

        // Bright catch-light along the split.
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.drawLine (face.getX(), face.getBottom(), face.getRight(), face.getY(),
                    juce::jmax (1.0f, 1.6f * scale));
    }

    // Concentric brushing.
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addEllipse (face);
        g.reduceClipRegion (clip);

        juce::Random random (0x5eed11);
        for (float r = radius; r > 2.0f; r -= juce::jmax (0.8f, 1.4f * scale))
        {
            const auto alpha = 0.02f + 0.045f * random.nextFloat();
            g.setColour ((random.nextBool() ? juce::Colours::white : juce::Colours::black)
                             .withAlpha (alpha));
            g.drawEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f,
                           juce::jmax (0.6f, 0.9f * scale));
        }
    }

    // Rim highlight and contact shadow.
    g.setColour (juce::Colours::white.withAlpha (0.30f));
    g.drawEllipse (face.reduced (0.5f), juce::jmax (0.7f, 1.1f * scale));
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (body.reduced (0.5f), juce::jmax (0.7f, 1.2f * scale));

    return image;
}

//==============================================================================
juce::Image createFaderCap (int width, int height, bool horizontal, float scale)
{
    const auto w = juce::jmax (6, width);
    const auto h = juce::jmax (6, height);
    juce::Image image (juce::Image::ARGB, w, h, true);

    juce::Graphics g (image);
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h)
                            .reduced (juce::jmax (1.0f, 2.0f * scale));
    const auto corner = juce::jmax (1.5f, 3.0f * scale);

    {
        juce::DropShadow shadow (juce::Colours::black.withAlpha (0.6f),
                                 (int) juce::jmax (2.0f, 5.0f * scale),
                                 { 0, (int) juce::jmax (1.0f, 2.0f * scale) });
        juce::Path p;
        p.addRoundedRectangle (bounds, corner);
        shadow.drawForPath (g, p);
    }

    juce::ColourGradient grad (juce::Colour (0xfff4f4f2), bounds.getX(), bounds.getY(),
                               juce::Colour (0xff6e6e6a), bounds.getRight(), bounds.getBottom(), false);
    grad.addColour (0.45, juce::Colour (0xffcfcfcb));
    grad.addColour (0.52, juce::Colour (0xff9a9a96));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, corner);

    // Machined groove across the middle, and the orange indicator pips.
    const auto centreLine = horizontal ? bounds.getCentreX() : bounds.getCentreY();

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    if (horizontal)
        g.fillRect (juce::Rectangle<float> (centreLine - 1.0f * scale, bounds.getY() + 2.0f * scale,
                                            juce::jmax (1.0f, 2.0f * scale), bounds.getHeight() - 4.0f * scale));
    else
        g.fillRect (juce::Rectangle<float> (bounds.getX() + 2.0f * scale, centreLine - 1.0f * scale,
                                            bounds.getWidth() - 4.0f * scale, juce::jmax (1.0f, 2.0f * scale)));

    g.setColour (colours::sliderTint.withAlpha (0.9f));
    const auto pip = juce::jmax (1.2f, 2.2f * scale);
    if (horizontal)
    {
        g.fillEllipse (centreLine - pip, bounds.getY() + 3.0f * scale, pip * 2.0f, pip * 2.0f);
        g.fillEllipse (centreLine - pip, bounds.getBottom() - 3.0f * scale - pip * 2.0f, pip * 2.0f, pip * 2.0f);
    }
    else
    {
        g.fillEllipse (bounds.getX() + 3.0f * scale, centreLine - pip, pip * 2.0f, pip * 2.0f);
        g.fillEllipse (bounds.getRight() - 3.0f * scale - pip * 2.0f, centreLine - pip, pip * 2.0f, pip * 2.0f);
    }

    // Fine machined ridges across the grip.
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (bounds, corner);
        g.reduceClipRegion (clip);

        const auto span = horizontal ? bounds.getWidth() : bounds.getHeight();
        const auto step = juce::jmax (2.0f, 5.0f * scale);

        for (float offset = step; offset < span - step * 0.5f; offset += step)
        {
            g.setColour (juce::Colours::black.withAlpha (0.16f));
            if (horizontal)
                g.drawLine (bounds.getX() + offset, bounds.getY(),
                            bounds.getX() + offset, bounds.getBottom(), juce::jmax (0.6f, 0.9f * scale));
            else
                g.drawLine (bounds.getX(), bounds.getY() + offset,
                            bounds.getRight(), bounds.getY() + offset, juce::jmax (0.6f, 0.9f * scale));

            g.setColour (juce::Colours::white.withAlpha (0.16f));
            if (horizontal)
                g.drawLine (bounds.getX() + offset + scale, bounds.getY(),
                            bounds.getX() + offset + scale, bounds.getBottom(), juce::jmax (0.5f, 0.7f * scale));
            else
                g.drawLine (bounds.getX(), bounds.getY() + offset + scale,
                            bounds.getRight(), bounds.getY() + offset + scale, juce::jmax (0.5f, 0.7f * scale));
        }
    }

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, juce::jmax (0.7f, 1.0f * scale));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (bounds.expanded (0.5f), corner, juce::jmax (0.7f, 1.0f * scale));

    return image;
}

//==============================================================================
void drawScrew (juce::Graphics& g, juce::Point<float> centre, float radius)
{
    const auto bounds = juce::Rectangle<float> (centre.x - radius, centre.y - radius,
                                                radius * 2.0f, radius * 2.0f);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (bounds.translated (0.0f, radius * 0.18f));

    juce::ColourGradient grad (juce::Colour (0xffb9b8b3), bounds.getX(), bounds.getY(),
                               juce::Colour (0xff4a4945), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (bounds);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (bounds.reduced (radius * 0.12f), juce::jmax (0.6f, radius * 0.12f));

    // Cross slot.
    const auto slot = radius * 0.62f;
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawLine (centre.x - slot, centre.y - slot * 0.45f,
                centre.x + slot, centre.y + slot * 0.45f, juce::jmax (0.8f, radius * 0.22f));

    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawLine (centre.x - slot, centre.y - slot * 0.45f + radius * 0.16f,
                centre.x + slot, centre.y + slot * 0.45f + radius * 0.16f,
                juce::jmax (0.6f, radius * 0.12f));
}

//==============================================================================
void drawEngravedText (juce::Graphics& g, const juce::String& text,
                       juce::Rectangle<float> area, juce::Justification justification,
                       float fontHeight, bool bold, juce::Colour colour)
{
    auto makeFont = [bold] (float height)
    {
        juce::Font f (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                         height, bold ? juce::Font::bold : juce::Font::plain));
        f.setExtraKerningFactor (0.08f);
        return f;
    };

    auto font = makeFont (fontHeight);

    // Engraved lettering is cut to fit the plate, so shrink rather than clip.
    const auto available = area.getWidth();
    const auto needed = juce::GlyphArrangement::getStringWidth (font, text);

    if (needed > available && needed > 0.0f)
        font = makeFont (juce::jmax (5.0f, fontHeight * available / needed));

    g.setFont (font);

    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawText (text, area.translated (0.0f, juce::jmax (1.0f, fontHeight * 0.055f)),
                justification, false);

    g.setColour (colour);
    g.drawText (text, area, justification, false);
}

//==============================================================================
void drawSubPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                   const juce::Image& texture, float scale)
{
    const auto corner = juce::jmax (2.0f, 6.0f * scale);

    // Recess behind the plate.
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (bounds.expanded (2.0f * scale), corner);

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (bounds, corner);
        g.reduceClipRegion (clip);

        if (texture.isValid())
            g.drawImage (texture, bounds.expanded (0.0f), juce::RectanglePlacement::stretchToFit);
        else
            g.fillAll (colours::panelBase);

        // Plate lighting: brighter at the top edge, shaded at the bottom.
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.10f),
                                    bounds.getCentreX(), bounds.getY(),
                                    juce::Colours::black.withAlpha (0.18f),
                                    bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (sheen);
        g.fillRect (bounds);
    }

    // Bevelled edge.
    g.setColour (juce::Colours::white.withAlpha (0.20f));
    g.drawRoundedRectangle (bounds.reduced (0.5f * scale), corner, juce::jmax (0.8f, 1.4f * scale));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (bounds.expanded (1.2f * scale), corner, juce::jmax (0.8f, 1.6f * scale));

    // Screws in the corners.
    const auto inset = 14.0f * scale;
    const auto r = 7.0f * scale;
    drawScrew (g, { bounds.getX() + inset,     bounds.getY() + inset },     r);
    drawScrew (g, { bounds.getRight() - inset, bounds.getY() + inset },     r);
    drawScrew (g, { bounds.getX() + inset,     bounds.getBottom() - inset }, r);
    drawScrew (g, { bounds.getRight() - inset, bounds.getBottom() - inset }, r);
}

//==============================================================================
void drawKnobTicks (juce::Graphics& g, juce::Point<float> centre, float radius,
                    int numTicks, float scale)
{
    constexpr auto startAngle = juce::MathConstants<float>::pi * 1.25f;
    constexpr auto endAngle   = juce::MathConstants<float>::pi * 2.75f;

    g.setColour (colours::engraveDark.withAlpha (0.75f));

    for (int i = 0; i < numTicks; ++i)
    {
        const auto t = (float) i / (float) (numTicks - 1);
        const auto angle = startAngle + t * (endAngle - startAngle);
        const auto major = (i == 0 || i == numTicks - 1 || (i % 2) == 0);
        const auto length = radius * (major ? 0.17f : 0.11f);

        const auto sinA = std::sin (angle), cosA = std::cos (angle);
        const auto inner = radius;
        const auto outer = radius + length;

        g.drawLine (centre.x + sinA * inner, centre.y - cosA * inner,
                    centre.x + sinA * outer, centre.y - cosA * outer,
                    juce::jmax (1.0f, (major ? 2.6f : 2.0f) * scale));
    }
}

} // namespace dr::theme
