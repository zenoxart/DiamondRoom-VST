#include "Theme.h"

namespace dr::theme
{

namespace
{
    //==========================================================================
    struct Facet
    {
        juce::Point<float> a, b, c;

        juce::Point<float> centroid() const noexcept
        {
            return { (a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f };
        }
    };

    /**
        Splits a triangle across its longest edge, over and over, which is what
        gives the low-poly shards their mix of sizes without any of them growing
        needle thin.
    */
    void subdivide (std::vector<Facet>& out, const Facet& facet, int depth, juce::Random& random)
    {
        if (depth <= 0)
        {
            out.push_back (facet);
            return;
        }

        const auto ab = facet.a.getDistanceFrom (facet.b);
        const auto bc = facet.b.getDistanceFrom (facet.c);
        const auto ca = facet.c.getDistanceFrom (facet.a);

        juce::Point<float> p, q, apex;

        if (ab >= bc && ab >= ca)      { p = facet.a; q = facet.b; apex = facet.c; }
        else if (bc >= ca)             { p = facet.b; q = facet.c; apex = facet.a; }
        else                           { p = facet.c; q = facet.a; apex = facet.b; }

        const auto t = 0.36f + 0.28f * random.nextFloat();
        const juce::Point<float> split { p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t };

        subdivide (out, { p, split, apex }, depth - 1, random);
        subdivide (out, { split, q, apex }, depth - 1, random);
    }

    std::vector<Facet> buildFacets (juce::Rectangle<float> area, int depth, juce::Random& random)
    {
        std::vector<Facet> facets;
        facets.reserve ((size_t) 2u << juce::jmin (depth, 20));

        const auto tl = area.getTopLeft();
        const auto tr = area.getTopRight();
        const auto bl = area.getBottomLeft();
        const auto br = area.getBottomRight();

        subdivide (facets, { tl, tr, br }, depth, random);
        subdivide (facets, { tl, br, bl }, depth, random);
        return facets;
    }
}

//==============================================================================
juce::Image createCrystalTexture (int width, int height, int seed, float brightness,
                                  float shardSize, float edgeFalloff)
{
    const auto w = juce::jmax (1, width);
    const auto h = juce::jmax (1, height);

    juce::Image image (juce::Image::ARGB, w, h, false);
    juce::Graphics g (image);

    const auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);

    // Base wash, dark at the bottom so the panel sits heavier than it lights.
    {
        juce::ColourGradient base (colours::backdropMid, area.getCentreX(), 0.0f,
                                   colours::backdropDeep, area.getCentreX(), area.getBottom(), false);
        g.setGradientFill (base);
        g.fillRect (area);
    }

    juce::Random random (seed);

    // Cut until the average facet is about the requested size. Deriving the
    // depth from the area rather than from a fixed number keeps the shards the
    // same size on a narrow rail as on the full width of the panel.
    const auto target = juce::jmax (8.0f, shardSize);
    const auto wanted = area.getWidth() * area.getHeight() / (2.0f * target * target);
    const auto depth = juce::jlimit (2, 12, (int) std::round (std::log2 (juce::jmax (1.0f, wanted))));

    const auto facets = buildFacets (area, depth, random);

    for (const auto& facet : facets)
    {
        juce::Path path;
        path.startNewSubPath (facet.a);
        path.lineTo (facet.b);
        path.lineTo (facet.c);
        path.closeSubPath();

        // Treat the longest edge as the facet's tilt and light it from the top
        // left, which is what stops the field looking like random confetti.
        const auto edge = facet.b - facet.a;
        const auto angle = std::atan2 (edge.y, edge.x);
        const auto lit = 0.5f + 0.5f * std::cos (angle * 2.0f - 0.8f);

        const auto centre = facet.centroid();
        const auto fromEdge = juce::jmin (juce::jmin (centre.x, area.getWidth() - centre.x),
                                          juce::jmin (centre.y, area.getHeight() - centre.y))
                                / juce::jmax (1.0f, juce::jmin (area.getWidth(), area.getHeight()) * 0.5f);

        auto value = 0.06f + 0.94f * lit * lit;
        value *= 0.72f + 0.28f * (1.0f - fromEdge);          // brighter towards the edges

        // Fade the field out towards the horizontal centre, so a wide texture
        // reads as crystal breaking in from the left and right edges rather
        // than a pattern covering the whole plate.
        if (edgeFalloff > 0.0f)
        {
            const auto normX = centre.x / juce::jmax (1.0f, area.getWidth());
            const auto distFromEdge = juce::jmin (normX, 1.0f - normX) * 2.0f;   // 0 at edges, 1 at centre
            constexpr float falloffSpan = 0.55f;    // how far in the fade reaches, as a fraction of the half-width
            const auto mask = juce::jlimit (0.0f, 1.0f, 1.0f - distFromEdge / falloffSpan);
            const auto centreFloor = 0.05f;         // a trace of shimmer even where the fade has finished

            value *= centreFloor + (1.0f - centreFloor) * juce::jmap (edgeFalloff, 0.0f, 1.0f, 1.0f, mask);
        }

        value += 0.07f * (random.nextFloat() - 0.5f);
        value = juce::jlimit (0.0f, 1.0f, value);

        // Below 1, brightness scales the field down as before. Above 1 it
        // lifts the floor towards white instead of just multiplying - the
        // rails want even their shadow facets reading as lit ice rather than
        // scaling every facet including the highlights past 1.0 and clipping
        // the whole ring to a flat white.
        value = brightness <= 1.0f
                    ? value * brightness
                    : juce::jmap (juce::jlimit (1.0f, 2.0f, brightness), 1.0f, 2.0f, value, 1.0f);
        value = juce::jlimit (0.0f, 1.0f, value);

        // Crystal is faintly blue in the shadows and white where it catches.
        const auto shard = juce::Colour::fromFloatRGBA (
            juce::jlimit (0.0f, 1.0f, 0.05f + value * 0.98f),
            juce::jlimit (0.0f, 1.0f, 0.08f + value * 1.00f),
            juce::jlimit (0.0f, 1.0f, 0.13f + value * 1.02f),
            1.0f);

        juce::ColourGradient grad (shard.brighter (0.16f), facet.a,
                                   shard.darker (0.34f), facet.c, false);
        g.setGradientFill (grad);
        g.fillPath (path);

        // Hairline along the cut.
        g.setColour (juce::Colours::white.withAlpha (0.018f + 0.055f * value));
        g.strokePath (path, juce::PathStrokeType (0.8f));
    }

    // Settle the darker cuts back down so controls stay legible on top of them.
    // At full brightness nothing is taken off: that is the showpiece.
    const auto knockBack = juce::jlimit (0.0f, 0.9f, 0.55f - brightness * 0.55f);

    if (knockBack > 0.0f)
    {
        g.setColour (colours::backdropDeep.withAlpha (knockBack));
        g.fillRect (area);
    }

    return image;
}

//==============================================================================
juce::Image createBrushedMetalTexture (int width, int height, int seed)
{
    const auto w = juce::jmax (1, width);
    const auto h = juce::jmax (1, height);

    juce::Image image (juce::Image::ARGB, w, h, false);
    juce::Graphics g (image);

    const auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);

    // A cool, dark gunmetal gradient - lighter at the top, so the plate reads
    // as lit from above like the crystal around it, with no facets of its own.
    {
        juce::ColourGradient base (juce::Colour (0xff232b38), area.getCentreX(), 0.0f,
                                   colours::backdropDeep, area.getCentreX(), area.getBottom(), false);
        base.addColour (0.5, colours::panelFill);
        g.setGradientFill (base);
        g.fillRect (area);
    }

    juce::Random random (seed);

    // Fine horizontal brushing, the same technique as the old steel panel:
    // short randomised strokes rather than one texture-wide sweep, so it does
    // not tile visibly when the same image backs several plates.
    constexpr int strokeCount = 900;

    for (int i = 0; i < strokeCount; ++i)
    {
        const auto y = random.nextFloat() * area.getHeight();
        const auto x0 = random.nextFloat() * area.getWidth();
        const auto length = 6.0f + random.nextFloat() * 30.0f;
        const auto alpha = 0.015f + 0.03f * random.nextFloat();

        g.setColour ((random.nextBool() ? juce::Colours::white : juce::Colours::black)
                         .withAlpha (alpha));
        g.drawLine (x0, y, x0 + length, y, 1.0f);
    }

    // A soft sheen along the top edge and a vignette in the corners, which is
    // what keeps a dozen identical plates from reading as one flat sheet.
    {
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.05f),
                                    area.getCentreX(), 0.0f,
                                    juce::Colours::white.withAlpha (0.0f),
                                    area.getCentreX(), area.getHeight() * 0.4f, false);
        g.setGradientFill (sheen);
        g.fillRect (area);
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
    const auto body = full.reduced (juce::jmax (1.0f, 2.0f * scale));
    const auto centre = full.getCentre();
    const auto radius = body.getWidth() * 0.5f;

    {
        juce::DropShadow shadow (juce::Colours::black.withAlpha (0.7f),
                                 (int) juce::jmax (2.0f, 8.0f * scale),
                                 { 0, (int) juce::jmax (1.0f, 3.0f * scale) });
        juce::Path circle;
        circle.addEllipse (body);
        shadow.drawForPath (g, circle);
    }

    // -- girdle: the ring of cut facets around the rim ----------------------
    // The reference reads as diamond dust rather than a gear: a couple of
    // dozen small, irregularly sized facets rather than a handful of big even
    // wedges, each one a sharp jump from its neighbour rather than a smooth
    // gradient around the rim.
    constexpr int numFacets = 30;
    const auto girdleInner = radius * 0.74f;

    juce::Random facetRandom (0xd1a3f7);

    // Irregular angular steps, so the facets vary in size the way a real cut
    // does - a perfectly even wedge count reads as a pinwheel, not a stone.
    std::array<float, numFacets> weights {};
    float weightSum = 0.0f;

    for (auto& wt : weights)
    {
        wt = 0.55f + facetRandom.nextFloat();
        weightSum += wt;
    }

    std::array<float, numFacets + 1> boundary {};
    {
        float accum = 0.0f;

        for (int i = 0; i < numFacets; ++i)
        {
            boundary[(size_t) i] = accum / weightSum * juce::MathConstants<float>::twoPi;
            accum += weights[(size_t) i];
        }

        boundary[numFacets] = juce::MathConstants<float>::twoPi;
    }

    // Looks one facet past the array end when a pavilion reaches into its
    // neighbour, wrapping the angle by a full turn rather than the index.
    auto boundaryAt = [&boundary, numFacets] (int idx)
    {
        if (idx <= numFacets)
            return boundary[(size_t) idx];

        return boundary[(size_t) (idx - numFacets)] + juce::MathConstants<float>::twoPi;
    };

    auto litAt = [] (float angle)
    {
        // Lit from above: facets facing up catch, the lower ones fall away.
        return 0.5f + 0.5f * std::cos (angle + juce::MathConstants<float>::halfPi);
    };

    auto silver = [] (float lit, float sparkle)
    {
        // Sparkle carries most of the contrast, not the smooth directional
        // term: a real cut stone's facets catch a point light almost at
        // random, all round the ring, rather than fading smoothly from one
        // bright side to one dark side like a lit sphere.
        const auto tone = juce::jlimit (0.0f, 1.0f, 0.56f + 0.22f * lit * lit + sparkle);
        return juce::Colour::fromFloatRGBA (tone * 0.97f, tone * 0.99f, tone, 1.0f);
    };

    // Two triangles per step, one pointing in and one pointing out, so the ring
    // is tiled without gaps. A single triangle per step leaves wedges of
    // background showing between them, which reads as a row of teeth.
    for (int i = 0; i < numFacets; ++i)
    {
        const auto a0 = boundaryAt (i);
        const auto a1 = boundaryAt (i + 1);
        const auto a2 = boundaryAt (i + 2);
        const auto mid0 = (a0 + a1) * 0.5f;
        const auto mid1 = (a1 + a2) * 0.5f;

        auto rim = [&] (float angle)
        {
            return juce::Point<float> (centre.x + std::cos (angle) * radius,
                                       centre.y + std::sin (angle) * radius);
        };

        auto inner = [&] (float angle)
        {
            return juce::Point<float> (centre.x + std::cos (angle) * girdleInner,
                                       centre.y + std::sin (angle) * girdleInner);
        };

        juce::Path crown;
        crown.startNewSubPath (rim (a0));
        crown.lineTo (rim (a1));
        crown.lineTo (inner (mid0));
        crown.closeSubPath();

        juce::Path pavilion;
        pavilion.startNewSubPath (inner (mid0));
        pavilion.lineTo (rim (a1));
        pavilion.lineTo (inner (mid1));
        pavilion.closeSubPath();

        const auto sparkleA = 0.60f * (facetRandom.nextFloat() - 0.5f);
        const auto sparkleB = 0.60f * (facetRandom.nextFloat() - 0.5f);

        g.setColour (silver (litAt (mid0), sparkleA));
        g.fillPath (crown);

        g.setColour (silver (litAt (a1), sparkleB).darker (0.14f));
        g.fillPath (pavilion);

        g.setColour (juce::Colours::white.withAlpha (0.10f + 0.16f * litAt (mid0)));
        g.strokePath (crown, juce::PathStrokeType (juce::jmax (0.4f, 0.6f * scale)));
    }

    // -- table: the brushed silver face -------------------------------------
    const auto face = juce::Rectangle<float> (girdleInner * 2.0f, girdleInner * 2.0f)
                          .withCentre (centre);
    {
        juce::ColourGradient grad (juce::Colour (0xfff4f8fc),
                                   face.getX() + face.getWidth() * 0.26f,
                                   face.getY() + face.getHeight() * 0.18f,
                                   juce::Colour (0xff6d7a8c),
                                   face.getRight(), face.getBottom(), true);
        grad.addColour (0.55, juce::Colour (0xffb9c4d2));
        g.setGradientFill (grad);
        g.fillEllipse (face);
    }

    // Radial brushing, which is what separates the table from the girdle.
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addEllipse (face);
        g.reduceClipRegion (clip);

        juce::Random random (0x6d1a3);
        constexpr int strokes = 160;

        for (int i = 0; i < strokes; ++i)
        {
            const auto angle = juce::MathConstants<float>::twoPi * (float) i / (float) strokes;
            const auto alpha = 0.012f + 0.030f * random.nextFloat();

            g.setColour ((random.nextBool() ? juce::Colours::white : juce::Colours::black)
                             .withAlpha (alpha));
            g.drawLine (centre.x, centre.y,
                        centre.x + std::cos (angle) * girdleInner,
                        centre.y + std::sin (angle) * girdleInner,
                        juce::jmax (0.6f, 1.1f * scale));
        }
    }

    // Catch-light across the upper left of the table.
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addEllipse (face);
        g.reduceClipRegion (clip);

        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.42f),
                                    face.getX(), face.getY(),
                                    juce::Colours::white.withAlpha (0.0f),
                                    face.getCentreX(), face.getCentreY(), false);
        g.setGradientFill (sheen);
        g.fillEllipse (face);
    }

    g.setColour (juce::Colours::white.withAlpha (0.26f));
    g.drawEllipse (face.reduced (0.5f), juce::jmax (0.6f, 1.0f * scale));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (body.reduced (0.5f), juce::jmax (0.7f, 1.1f * scale));

    return image;
}



//==============================================================================
void drawScrew (juce::Graphics& g, juce::Point<float> centre, float radius)
{
    const auto bounds = juce::Rectangle<float> (centre.x - radius, centre.y - radius,
                                                radius * 2.0f, radius * 2.0f);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (bounds.expanded (radius * 0.16f));

    juce::ColourGradient grad (juce::Colour (0xff8e9fb4), bounds.getX(), bounds.getY(),
                               juce::Colour (0xff2b3543), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (bounds);

    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawEllipse (bounds.reduced (radius * 0.1f), juce::jmax (0.5f, radius * 0.14f));

    const auto slot = radius * 0.6f;
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine (centre.x - slot, centre.y - slot * 0.4f,
                centre.x + slot, centre.y + slot * 0.4f, juce::jmax (0.7f, radius * 0.2f));
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
        f.setExtraKerningFactor (0.10f);
        return f;
    };

    auto font = makeFont (fontHeight);

    // Lettering is cut to fit the plate, so shrink rather than clip.
    const auto available = area.getWidth();
    const auto needed = juce::GlyphArrangement::getStringWidth (font, text);

    if (needed > available && needed > 0.0f)
        font = makeFont (juce::jmax (5.0f, fontHeight * available / needed));

    g.setFont (font);

    g.setColour (colours::textShadow);
    g.drawText (text, area.translated (0.0f, juce::jmax (1.0f, fontHeight * 0.06f)),
                justification, false);

    g.setColour (colour);
    g.drawText (text, area, justification, false);
}

//==============================================================================
void drawSubPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                   const juce::Image& texture, float scale)
{
    const auto corner = juce::jmax (2.0f, 8.0f * scale);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f * scale), corner);

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (bounds, corner);
        g.reduceClipRegion (clip);

        if (texture.isValid())
            g.drawImage (texture, bounds, juce::RectanglePlacement::stretchToFit);
        else
            g.fillAll (colours::panelFill);

        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.055f),
                                    bounds.getCentreX(), bounds.getY(),
                                    juce::Colours::black.withAlpha (0.30f),
                                    bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (sheen);
        g.fillRect (bounds);
    }

    // Lit edge: brighter along the top, fading down the sides.
    {
        juce::ColourGradient edge (colours::panelEdge.withAlpha (0.95f),
                                   bounds.getCentreX(), bounds.getY(),
                                   colours::panelEdge.withAlpha (0.28f),
                                   bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (edge);
        g.drawRoundedRectangle (bounds.reduced (0.5f * scale), corner, juce::jmax (0.9f, 1.5f * scale));
    }

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (bounds.expanded (1.4f * scale), corner, juce::jmax (0.8f, 1.4f * scale));

    const auto inset = 15.0f * scale;
    const auto r = 6.0f * scale;
    drawScrew (g, { bounds.getX() + inset,     bounds.getY() + inset },      r);
    drawScrew (g, { bounds.getRight() - inset, bounds.getY() + inset },      r);
    drawScrew (g, { bounds.getX() + inset,     bounds.getBottom() - inset }, r);
    drawScrew (g, { bounds.getRight() - inset, bounds.getBottom() - inset }, r);
}

//==============================================================================
void drawKnobTicks (juce::Graphics& g, juce::Point<float> centre, float radius,
                    int numTicks, float scale)
{
    constexpr auto startAngle = juce::MathConstants<float>::pi * 1.25f;
    constexpr auto endAngle   = juce::MathConstants<float>::pi * 2.75f;

    for (int i = 0; i < numTicks; ++i)
    {
        const auto t = (float) i / (float) (numTicks - 1);
        const auto angle = startAngle + t * (endAngle - startAngle);
        const auto major = (i == 0 || i == numTicks - 1 || (i % 2) == 0);
        const auto length = radius * (major ? 0.20f : 0.12f);

        const auto sinA = std::sin (angle), cosA = std::cos (angle);

        g.setColour (colours::text.withAlpha (major ? 0.78f : 0.42f));
        g.drawLine (centre.x + sinA * radius, centre.y - cosA * radius,
                    centre.x + sinA * (radius + length), centre.y - cosA * (radius + length),
                    juce::jmax (0.9f, (major ? 2.2f : 1.6f) * scale));
    }
}

//==============================================================================
juce::Path makeDiamondPath (juce::Rectangle<float> bounds)
{
    const auto x = bounds.getX(), y = bounds.getY();
    const auto w = bounds.getWidth(), h = bounds.getHeight();

    // Brilliant cut seen face on: a wide crown over a tapered pavilion.
    const auto crownY = y + h * 0.34f;

    juce::Path p;
    p.startNewSubPath (x + w * 0.18f, y);
    p.lineTo (x + w * 0.82f, y);
    p.lineTo (x + w, crownY);
    p.lineTo (x + w * 0.5f, y + h);
    p.lineTo (x, crownY);
    p.closeSubPath();
    return p;
}

void drawDiamond (juce::Graphics& g, juce::Rectangle<float> bounds, float scale)
{
    const auto outline = makeDiamondPath (bounds);
    const auto x = bounds.getX(), y = bounds.getY();
    const auto w = bounds.getWidth(), h = bounds.getHeight();
    const auto crownY = y + h * 0.34f;

    {
        juce::ColourGradient grad (juce::Colour (0xffeaf7ff), bounds.getX(), bounds.getY(),
                                   colours::accentDeep, bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (outline);
    }

    // The internal cuts: crown table plus the pavilion facets.
    juce::Path cuts;
    cuts.startNewSubPath (x, crownY);
    cuts.lineTo (x + w, crownY);
    cuts.startNewSubPath (x + w * 0.18f, y);
    cuts.lineTo (x + w * 0.32f, crownY);
    cuts.lineTo (x + w * 0.5f, y + h);
    cuts.startNewSubPath (x + w * 0.82f, y);
    cuts.lineTo (x + w * 0.68f, crownY);
    cuts.lineTo (x + w * 0.5f, y + h);
    cuts.startNewSubPath (x + w * 0.32f, crownY);
    cuts.lineTo (x + w * 0.68f, crownY);

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.strokePath (cuts, juce::PathStrokeType (juce::jmax (0.6f, 1.0f * scale)));

    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.strokePath (outline, juce::PathStrokeType (juce::jmax (0.7f, 1.2f * scale)));
}

void drawDiamondOutline (juce::Graphics& g, juce::Rectangle<float> bounds, float scale, juce::Colour colour)
{
    const auto outline = makeDiamondPath (bounds);
    const auto x = bounds.getX(), y = bounds.getY();
    const auto w = bounds.getWidth(), h = bounds.getHeight();
    const auto crownY = y + h * 0.34f;

    juce::Path cuts;
    cuts.startNewSubPath (x, crownY);
    cuts.lineTo (x + w, crownY);
    cuts.startNewSubPath (x + w * 0.5f, crownY);
    cuts.lineTo (x + w * 0.5f, y + h);

    g.setColour (colour.withAlpha (0.35f));
    g.strokePath (cuts, juce::PathStrokeType (juce::jmax (0.4f, 0.6f * scale)));

    g.setColour (colour.withAlpha (0.7f));
    g.strokePath (outline, juce::PathStrokeType (juce::jmax (0.5f, 0.9f * scale)));
}

} // namespace dr::theme
