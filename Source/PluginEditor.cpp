#include "PluginEditor.h"

using namespace dr;

namespace
{
    // Panel artwork coordinates, in design units (2001 x 764).
    const juce::Rectangle<int> drivePanelArea { 110, 110, 238, 430 };
    const juce::Rectangle<int> tubePanelArea  { 1652, 110, 238, 430 };
    const juce::Rectangle<int> mixPanelArea   { 110, 556, 1780, 156 };

    const juce::Rectangle<int> hPanelArea     { 362,  110, 310, 430 };
    const juce::Rectangle<int> mannyPanelArea { 683,  110, 310, 430 };
    const juce::Rectangle<int> valPanelArea   { 1004, 110, 310, 430 };
    const juce::Rectangle<int> truePanelArea  { 1325, 110, 310, 430 };

    constexpr int railWidth = 100;

    juce::Rectangle<int> scaleRect (juce::Rectangle<int> design, float scale)
    {
        return { juce::roundToInt ((float) design.getX() * scale),
                 juce::roundToInt ((float) design.getY() * scale),
                 juce::roundToInt ((float) design.getWidth() * scale),
                 juce::roundToInt ((float) design.getHeight() * scale) };
    }
}

//==============================================================================
DiamondRoomAudioProcessorEditor::DiamondRoomAudioProcessorEditor (DiamondRoomAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    auto& state = processor.getState();

    for (auto* panel : { &drivePanel, &tubePanel, &mixPanel })
        addAndMakeVisible (panel);

    for (auto* strip : { &hStrip, &mannyStrip, &valStrip, &trueStrip })
        addAndMakeVisible (strip->panel);

    for (auto* knob : { &driveKnob, &tubeKnob })
    {
        knob->setDoubleClickReturnValue (true, 3.0);
        addAndMakeVisible (knob);
    }

    addAndMakeVisible (masterMix);

    for (auto* strip : { &hStrip, &mannyStrip, &valStrip, &trueStrip })
    {
        addAndMakeVisible (strip->topKnob);
        addAndMakeVisible (strip->bottomKnob);
        addAndMakeVisible (strip->mix);
    }

    driveAttachment = std::make_unique<SliderAttachment> (state, params::drive, driveKnob);
    tubeAttachment  = std::make_unique<SliderAttachment> (state, params::tube,  tubeKnob);
    mixAttachment   = std::make_unique<SliderAttachment> (state, params::mix,   masterMix);

    attachStrip (hStrip,     params::hTone,    params::hTime,      params::hMix, params::hOn);
    attachStrip (mannyStrip, params::mDist,    params::mAmount,    params::mMix, params::mOn);
    attachStrip (valStrip,   params::vHighCut, params::vDecay,     params::vMix, params::vOn);
    attachStrip (trueStrip,  params::tDistance, params::tRoomsize, params::tMix, params::tOn);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) theme::designWidth / (double) theme::designHeight);
    setResizeLimits (900, juce::roundToInt (900.0 * theme::designHeight / theme::designWidth),
                     theme::designWidth, theme::designHeight);
    setSize (1300, juce::roundToInt (1300.0 * theme::designHeight / theme::designWidth));
}

void DiamondRoomAudioProcessorEditor::attachStrip (ReverbStrip& strip, const char* topId,
                                                   const char* bottomId, const char* mixId,
                                                   const char* onId)
{
    auto& state = processor.getState();

    strip.topAttachment    = std::make_unique<SliderAttachment> (state, topId, strip.topKnob);
    strip.bottomAttachment = std::make_unique<SliderAttachment> (state, bottomId, strip.bottomKnob);
    strip.mixAttachment    = std::make_unique<SliderAttachment> (state, mixId, strip.mix);
    strip.ledAttachment    = std::make_unique<ButtonAttachment> (state, onId, strip.panel.getLed());
}

//==============================================================================
void DiamondRoomAudioProcessorEditor::buildBackground()
{
    const auto w = getWidth();
    const auto h = getHeight();

    if (w <= 0 || h <= 0 || (w == textureWidth && h == textureHeight))
        return;

    textureWidth = w;
    textureHeight = h;

    // Three separate bakes so the rails and plates do not repeat the same
    // blotches as the panel behind them.
    panelTexture = theme::createPanelTexture (w, h, 20240517, 1.0f);
    plateTexture = theme::createPanelTexture (juce::jmax (1, w / 2), juce::jmax (1, h / 2),
                                              77345, 0.55f);
    railTexture  = theme::createPanelTexture (juce::jmax (1, railWidth), h, 991733, 1.15f);

    for (auto* panel : { &drivePanel, &tubePanel, &mixPanel })
        panel->setTexture (plateTexture);

    for (auto* strip : { &hStrip, &mannyStrip, &valStrip, &trueStrip })
        strip->panel.setTexture (plateTexture);
}

void DiamondRoomAudioProcessorEditor::resized()
{
    const auto scale = juce::jmin ((float) getWidth()  / (float) theme::designWidth,
                                   (float) getHeight() / (float) theme::designHeight);

    buildBackground();

    for (auto* panel : { &drivePanel, &tubePanel, &mixPanel })
        panel->setDesignScale (scale);

    drivePanel.setBounds (scaleRect (drivePanelArea, scale));
    tubePanel.setBounds  (scaleRect (tubePanelArea, scale));
    mixPanel.setBounds   (scaleRect (mixPanelArea, scale));

    // The two big knobs, centred on their plates.
    driveKnob.setDesignScale (scale);
    tubeKnob.setDesignScale (scale);
    driveKnob.setBounds (scaleRect ({ 118, 205, 228, 288 }, scale));
    tubeKnob.setBounds  (scaleRect ({ 1660, 205, 228, 288 }, scale));

    masterMix.setDesignScale (scale);
    masterMix.setBounds (scaleRect ({ 255, 558, 1490, 152 }, scale));

    layoutStrip (hStrip,     hPanelArea,     scale);
    layoutStrip (mannyStrip, mannyPanelArea, scale);
    layoutStrip (valStrip,   valPanelArea,   scale);
    layoutStrip (trueStrip,  truePanelArea,  scale);
}

void DiamondRoomAudioProcessorEditor::layoutStrip (ReverbStrip& strip,
                                                   juce::Rectangle<int> panelBounds,
                                                   float scale)
{
    strip.panel.setDesignScale (scale);
    strip.panel.setBounds (scaleRect (panelBounds, scale));

    const auto x = panelBounds.getX();

    strip.topKnob.setDesignScale (scale);
    strip.bottomKnob.setDesignScale (scale);
    strip.mix.setDesignScale (scale);

    // The knob and fader components are wider than their controls: the extra
    // room is where the engraved captions go.
    strip.topKnob.setBounds    (scaleRect ({ x + 14,  173, 170, 190 }, scale));
    strip.bottomKnob.setBounds (scaleRect ({ x + 14,  350, 170, 190 }, scale));
    strip.mix.setBounds        (scaleRect ({ x + 150, 180, 170, 360 }, scale));
}

//==============================================================================
void DiamondRoomAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto scale = juce::jmin ((float) getWidth()  / (float) theme::designWidth,
                                   (float) getHeight() / (float) theme::designHeight);

    g.fillAll (juce::Colour (0xff1b1b19));

    if (panelTexture.isValid())
        g.drawImageAt (panelTexture, 0, 0);

    drawRackFrame (g, scale);
    drawTitle (g, scale);
}

void DiamondRoomAudioProcessorEditor::drawRackFrame (juce::Graphics& g, float scale)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto rail = (float) railWidth * scale;

    // -- rack ears ---------------------------------------------------------
    for (int side = 0; side < 2; ++side)
    {
        const auto area = (side == 0)
                              ? juce::Rectangle<float> (0.0f, 0.0f, rail, bounds.getHeight())
                              : juce::Rectangle<float> (bounds.getRight() - rail, 0.0f,
                                                        rail, bounds.getHeight());

        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (area.toNearestInt());

            if (railTexture.isValid())
                g.drawImage (railTexture, area, juce::RectanglePlacement::stretchToFit);

            juce::ColourGradient shade (juce::Colours::black.withAlpha (0.0f),
                                        side == 0 ? area.getX() : area.getRight(), 0.0f,
                                        juce::Colours::black.withAlpha (0.45f),
                                        side == 0 ? area.getRight() : area.getX(), 0.0f, false);
            g.setGradientFill (shade);
            g.fillRect (area);
        }

        // Mounting slots, top and bottom.
        for (int i = 0; i < 2; ++i)
        {
            const auto slot = juce::Rectangle<float> (52.0f * scale, 30.0f * scale)
                                  .withCentre ({ area.getCentreX(),
                                                 i == 0 ? 46.0f * scale
                                                        : bounds.getHeight() - 46.0f * scale });

            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (slot.expanded (2.0f * scale), 8.0f * scale);
            g.setColour (juce::Colour (0xfff0efe9));
            g.fillRoundedRectangle (slot, 8.0f * scale);
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.drawRoundedRectangle (slot.reduced (0.5f), 8.0f * scale, juce::jmax (0.8f, 1.4f * scale));
        }

        // Screws down the inner edge.
        for (int i = 0; i < 2; ++i)
        {
            const auto y = i == 0 ? 122.0f * scale : bounds.getHeight() - 122.0f * scale;
            theme::drawScrew (g, { area.getCentreX(), y }, 9.0f * scale);
        }

        // Vertical branding and the diamond mark.
        {
            juce::Graphics::ScopedSaveState save (g);

            const auto centre = area.getCentre();
            g.addTransform (juce::AffineTransform::rotation (
                side == 0 ? -juce::MathConstants<float>::halfPi
                          :  juce::MathConstants<float>::halfPi,
                centre.x, centre.y));

            const auto textArea = juce::Rectangle<float> (bounds.getHeight() * 0.7f, rail)
                                      .withCentre (centre)
                                      .translated (0.0f, side == 0 ? -18.0f * scale : 18.0f * scale);

            theme::drawEngravedText (g, "DIAMOND ROOM", textArea, juce::Justification::centred,
                                     juce::jmax (7.0f, 21.0f * scale), true,
                                     theme::colours::text.withAlpha (0.8f));
        }

        {
            const auto d = 34.0f * scale;
            const auto centre = juce::Point<float> (area.getCentreX(),
                                                    bounds.getHeight() * 0.61f);
            juce::Path diamond;
            diamond.addQuadrilateral (centre.x, centre.y - d * 0.5f,
                                      centre.x + d * 0.42f, centre.y,
                                      centre.x, centre.y + d * 0.5f,
                                      centre.x - d * 0.42f, centre.y);

            g.setColour (juce::Colours::white.withAlpha (0.2f));
            g.fillPath (diamond, juce::AffineTransform::translation (0.0f, 1.5f * scale));
            g.setColour (theme::colours::engraveDark.withAlpha (0.8f));
            g.fillPath (diamond);
        }

        // Hard edge between the ear and the main panel.
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        const auto edgeX = side == 0 ? area.getRight() : area.getX();
        g.drawLine (edgeX, 0.0f, edgeX, bounds.getHeight(), juce::jmax (1.0f, 2.0f * scale));
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawLine (edgeX + (side == 0 ? 2.0f : -2.0f) * scale, 0.0f,
                    edgeX + (side == 0 ? 2.0f : -2.0f) * scale, bounds.getHeight(),
                    juce::jmax (0.8f, 1.4f * scale));
    }

    // -- outer frame -------------------------------------------------------
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRect (bounds, juce::jmax (1.0f, 3.0f * scale));
}

void DiamondRoomAudioProcessorEditor::drawTitle (juce::Graphics& g, float scale)
{
    const auto plate = juce::Rectangle<float> ((float) railWidth * scale, 0.0f,
                                               (float) getWidth() - 2.0f * railWidth * scale,
                                               (float) getHeight());

    const auto titleArea = juce::Rectangle<float> (plate.getX(), 24.0f * scale,
                                                   plate.getWidth(), 62.0f * scale);
    const auto fontHeight = juce::jmax (11.0f, 52.0f * scale);

    juce::Font font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                        fontHeight, juce::Font::bold));
    font.setExtraKerningFactor (0.14f);

    const auto textWidth = juce::GlyphArrangement::getStringWidth (font, "DIAMOND ROOM");

    g.setFont (font);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawText ("DIAMOND ROOM", titleArea.translated (0.0f, 2.5f * scale),
                juce::Justification::centred, false);
    g.setColour (theme::colours::text);
    g.drawText ("DIAMOND ROOM", titleArea, juce::Justification::centred, false);

    // Rules either side of the title.
    const auto y = titleArea.getCentreY();
    const auto gap = textWidth * 0.5f + 34.0f * scale;
    const auto ruleLength = 190.0f * scale;
    const auto thickness = juce::jmax (1.0f, 4.0f * scale);

    for (int side = 0; side < 2; ++side)
    {
        const auto x1 = plate.getCentreX() + (side == 0 ? -gap : gap);
        const auto x2 = x1 + (side == 0 ? -ruleLength : ruleLength);

        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawLine (x1, y + thickness, x2, y + thickness, thickness);
        g.setColour (theme::colours::engraveDark);
        g.drawLine (x1, y, x2, y, thickness);
    }
}
