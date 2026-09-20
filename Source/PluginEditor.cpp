#include "PluginEditor.h"

using namespace dr;

namespace
{
    // Panel artwork coordinates, in design units (2001 x 764).
    const juce::Rectangle<int> drivePanelArea { 110, 150, 238, 380 };
    const juce::Rectangle<int> tubePanelArea  { 1652, 150, 238, 380 };
    const juce::Rectangle<int> mixPanelArea   { 110, 542, 1780, 146 };

    const juce::Rectangle<int> hPanelArea     { 362,  150, 310, 380 };
    const juce::Rectangle<int> mannyPanelArea { 683,  150, 310, 380 };
    const juce::Rectangle<int> valPanelArea   { 1004, 150, 310, 380 };
    const juce::Rectangle<int> truePanelArea  { 1325, 150, 310, 380 };

    constexpr int railWidth = 100;

    // Popup menu item ids. Presets are offset so they cannot collide with the
    // fixed commands.
    constexpr int saveItem = 1, deleteItem = 2, revealItem = 3;
    constexpr int sizeBase = 100, factoryBase = 1000, userBase = 2000;

    struct SizeOption { const char* label; int width; };

    constexpr SizeOption sizeOptions[]
    {
        { "Small",       900 },
        { "Medium",     1100 },
        { "Large",      1300 },
        { "Extra large", 1600 },
        { "Full",       dr::theme::designWidth },
    };

    constexpr size_t numSizeOptions = std::size (sizeOptions);

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

    for (auto* button : { &undoButton, &redoButton, &prevPreset, &nextPreset,
                          &presetPlate, &settingsButton })
        addAndMakeVisible (button);

    undoButton.onClick = [this]
    {
        processor.getUndoManager().undo();
        updateUndoButtons();
    };

    redoButton.onClick = [this]
    {
        processor.getUndoManager().redo();
        updateUndoButtons();
    };

    prevPreset.onClick     = [this] { processor.getPresetManager().loadPrevious(); };
    nextPreset.onClick     = [this] { processor.getPresetManager().loadNext(); };
    presetPlate.onClick    = [this] { showPresetMenu(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };

    processor.getPresetManager().addChangeListener (this);
    processor.getUndoManager().addChangeListener (this);

    wireUndoTransactions();
    updatePresetDisplay();
    updateUndoButtons();

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) theme::designWidth / (double) theme::designHeight);
    setResizeLimits (900, juce::roundToInt (900.0 * theme::designHeight / theme::designWidth),
                     theme::designWidth, theme::designHeight);

    const auto savedWidth = processor.getSavedEditorWidth();
    applyWidth (savedWidth > 0 ? savedWidth : 1300);
}

DiamondRoomAudioProcessorEditor::~DiamondRoomAudioProcessorEditor()
{
    processor.getPresetManager().removeChangeListener (this);
    processor.getUndoManager().removeChangeListener (this);
}

//==============================================================================
void DiamondRoomAudioProcessorEditor::applyWidth (int width)
{
    const auto clamped = juce::jlimit (900, theme::designWidth, width);
    setSize (clamped, juce::roundToInt ((double) clamped * theme::designHeight / theme::designWidth));
}

void DiamondRoomAudioProcessorEditor::markUndoPoint (const juce::String& description)
{
    processor.getUndoManager().beginNewTransaction (description);
}

void DiamondRoomAudioProcessorEditor::wireUndoTransactions()
{
    // One gesture is one undo step. Without this every intermediate value the
    // parameter passes through on the way would become its own step.
    auto wireSlider = [this] (juce::Slider& slider, const juce::String& name)
    {
        slider.onDragStart = [this, name] { markUndoPoint (name); };
    };

    wireSlider (driveKnob, "Drive");
    wireSlider (tubeKnob, "Tube");
    wireSlider (masterMix, "Mix");

    for (auto* strip : { &hStrip, &mannyStrip, &valStrip, &trueStrip })
    {
        wireSlider (strip->topKnob, strip->topKnob.getName());
        wireSlider (strip->bottomKnob, strip->bottomKnob.getName());
        wireSlider (strip->mix, strip->mix.getName());

        strip->panel.getLed().onClick = [this] { markUndoPoint ("Reverb on/off"); };
    }
}

//==============================================================================
void DiamondRoomAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &processor.getPresetManager())
        updatePresetDisplay();
    else if (source == &processor.getUndoManager())
        updateUndoButtons();
}

void DiamondRoomAudioProcessorEditor::updatePresetDisplay()
{
    presetPlate.setText (processor.getPresetManager().getDisplayName());
}

void DiamondRoomAudioProcessorEditor::updateUndoButtons()
{
    auto& undoManager = processor.getUndoManager();
    undoButton.setEnabled (undoManager.canUndo());
    redoButton.setEnabled (undoManager.canRedo());
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
void DiamondRoomAudioProcessorEditor::showMessage (const juce::String& title,
                                                   const juce::String& message)
{
    juce::AlertWindow::showAsync (
        juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                 .withTitle (title)
                                 .withMessage (message)
                                 .withButton ("OK")
                                 .withAssociatedComponent (this),
        nullptr);
}

void DiamondRoomAudioProcessorEditor::showPresetMenu()
{
    auto& presets = processor.getPresetManager();
    const auto current = presets.getCurrentPresetName();
    const auto factoryNames = presets.getFactoryPresetNames();
    const auto userNames = presets.getUserPresetNames();

    juce::PopupMenu menu, factoryMenu, userMenu;

    for (int i = 0; i < factoryNames.size(); ++i)
        factoryMenu.addItem (factoryBase + i, factoryNames[i], true, factoryNames[i] == current);

    for (int i = 0; i < userNames.size(); ++i)
        userMenu.addItem (userBase + i, userNames[i], true, userNames[i] == current);

    menu.addSubMenu ("Factory", factoryMenu);

    if (userNames.isEmpty())
        menu.addItem (-1, "User (none saved)", false);
    else
        menu.addSubMenu ("User", userMenu);

    menu.addSeparator();
    menu.addItem (saveItem, "Save as...");

    if (presets.isUserPreset (current))
        menu.addItem (deleteItem, "Delete " + current.quoted());

    menu.addItem (revealItem, "Show preset folder");

    juce::Component::SafePointer<DiamondRoomAudioProcessorEditor> safe (this);

    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (&presetPlate).withMinimumWidth (220),
        [safe, factoryNames, userNames, current] (int result)
        {
            if (safe == nullptr || result == 0)
                return;

            auto& manager = safe->processor.getPresetManager();

            if (result >= userBase)
                manager.loadPreset (userNames[result - userBase]);
            else if (result >= factoryBase)
                manager.loadPreset (factoryNames[result - factoryBase]);
            else if (result == saveItem)
                safe->showSavePresetDialog();
            else if (result == deleteItem)
            {
                const auto outcome = manager.deleteUserPreset (current);

                if (outcome.failed())
                    safe->showMessage ("Could not delete preset", outcome.getErrorMessage());
            }
            else if (result == revealItem)
                dr::PresetManager::getUserPresetDirectory().revealToUser();
        });
}

void DiamondRoomAudioProcessorEditor::showSavePresetDialog()
{
    dialog = std::make_unique<juce::AlertWindow> ("Save preset",
                                                  "Name for this preset:",
                                                  juce::MessageBoxIconType::NoIcon,
                                                  this);
    dialog->addTextEditor ("name", processor.getPresetManager().getCurrentPresetName());
    dialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<DiamondRoomAudioProcessorEditor> safe (this);

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe] (int result)
        {
            // The window is owned by the editor, so it can outlive neither.
            if (safe == nullptr || safe->dialog == nullptr)
                return;

            const auto name = safe->dialog->getTextEditorContents ("name");
            safe->dialog.reset();

            if (result != 1)
                return;

            const auto outcome = safe->processor.getPresetManager().saveUserPreset (name);

            if (outcome.failed())
                safe->showMessage ("Could not save preset", outcome.getErrorMessage());
        }), false);
}

void DiamondRoomAudioProcessorEditor::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Window size");

    for (int i = 0; i < (int) numSizeOptions; ++i)
    {
        const auto width = sizeOptions[i].width;
        const auto height = juce::roundToInt ((double) width * theme::designHeight
                                                / theme::designWidth);

        menu.addItem (sizeBase + i,
                      juce::String (sizeOptions[i].label) + "  -  "
                        + juce::String (width) + " x " + juce::String (height),
                      true, getWidth() == width);
    }

    menu.addSeparator();
    menu.addItem (-1, "The window can also be dragged from its corner", false);

    juce::Component::SafePointer<DiamondRoomAudioProcessorEditor> safe (this);

    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (&settingsButton).withMinimumWidth (260),
        [safe] (int result)
        {
            if (safe != nullptr && result >= sizeBase && result < sizeBase + (int) numSizeOptions)
                safe->applyWidth (sizeOptions[result - sizeBase].width);
        });
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

    const auto uiScale = (float) w / (float) theme::designWidth;

    // Three separate cuts, at three brightnesses: the rails are the showpiece,
    // the panel behind the plates is dark enough to read controls against, and
    // the plates themselves are darker still.
    panelTexture = theme::createCrystalTexture (w, h, 20240517, 0.40f, 110.0f * uiScale);
    plateTexture = theme::createCrystalTexture (juce::jmax (1, w / 3), juce::jmax (1, h / 3),
                                                77345, 0.30f, 26.0f * uiScale);
    railTexture  = theme::createCrystalTexture (
        juce::jmax (1, juce::roundToInt ((float) railWidth * uiScale)),
        h, 991733, 1.5f, 52.0f * uiScale);

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
    driveKnob.setBounds (scaleRect ({ 118,  205, 228, 300 }, scale));
    tubeKnob.setBounds  (scaleRect ({ 1660, 205, 228, 300 }, scale));

    masterMix.setDesignScale (scale);
    masterMix.setBounds (scaleRect ({ 255, 544, 1490, 144 }, scale));

    // Top rail: undo/redo and preset stepping to the left of the title, the
    // settings gear to the right of it.
    for (auto* button : { &undoButton, &redoButton, &prevPreset, &nextPreset,
                          &presetPlate, &settingsButton })
        button->setDesignScale (scale);

    undoButton.setBounds     (scaleRect ({ 118,  46,  52, 52 }, scale));
    redoButton.setBounds     (scaleRect ({ 178,  46,  52, 52 }, scale));
    prevPreset.setBounds     (scaleRect ({ 248,  46,  42, 52 }, scale));
    presetPlate.setBounds    (scaleRect ({ 290,  46, 180, 52 }, scale));
    nextPreset.setBounds     (scaleRect ({ 470,  46,  42, 52 }, scale));
    settingsButton.setBounds (scaleRect ({ 1831, 46,  52, 52 }, scale));

    layoutStrip (hStrip,     hPanelArea,     scale);
    layoutStrip (mannyStrip, mannyPanelArea, scale);
    layoutStrip (valStrip,   valPanelArea,   scale);
    layoutStrip (trueStrip,  truePanelArea,  scale);

    processor.setSavedEditorWidth (getWidth());
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
    // room is where the captions go.
    strip.topKnob.setBounds    (scaleRect ({ x + 14,  205, 170, 160 }, scale));
    strip.bottomKnob.setBounds (scaleRect ({ x + 14,  370, 170, 160 }, scale));
    strip.mix.setBounds        (scaleRect ({ x + 150, 205, 170, 325 }, scale));
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

            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.fillRoundedRectangle (slot.expanded (2.5f * scale), 8.0f * scale);

            juce::ColourGradient lens (juce::Colour (0xffe8f6ff), slot.getX(), slot.getY(),
                                       theme::colours::accentDeep, slot.getRight(), slot.getBottom(), false);
            g.setGradientFill (lens);
            g.fillRoundedRectangle (slot, 8.0f * scale);

            g.setColour (theme::colours::accent.withAlpha (0.35f));
            g.drawRoundedRectangle (slot.expanded (2.5f * scale), 8.0f * scale,
                                    juce::jmax (0.8f, 1.6f * scale));
            g.setColour (juce::Colours::white.withAlpha (0.65f));
            g.drawRoundedRectangle (slot.reduced (0.5f), 8.0f * scale, juce::jmax (0.8f, 1.2f * scale));
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
                                     juce::jmax (7.0f, 20.0f * scale), false,
                                     theme::colours::text.withAlpha (0.85f));
        }

        {
            const auto d = 26.0f * scale;
            const auto gem = juce::Rectangle<float> (d, d * 0.92f)
                                 .withCentre ({ area.getCentreX(), bounds.getHeight() * 0.62f });
            theme::drawDiamond (g, gem, scale);
        }

        // Lit seam between the ear and the main panel.
        const auto edgeX = side == 0 ? area.getRight() : area.getX();
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.drawLine (edgeX, 0.0f, edgeX, bounds.getHeight(), juce::jmax (1.0f, 2.0f * scale));
        g.setColour (theme::colours::accent.withAlpha (0.20f));
        g.drawLine (edgeX + (side == 0 ? 2.0f : -2.0f) * scale, 0.0f,
                    edgeX + (side == 0 ? 2.0f : -2.0f) * scale, bounds.getHeight(),
                    juce::jmax (0.8f, 1.4f * scale));
    }

    // -- footer and outer frame ---------------------------------------------
    {
        const auto plate = bounds.reduced (rail, 0.0f);
        const auto footer = juce::Rectangle<float> (plate.getX(), bounds.getBottom() - 44.0f * scale,
                                                    plate.getWidth(), 28.0f * scale);
        const auto fontHeight = juce::jmax (7.0f, 19.0f * scale);

        juce::Font font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                            fontHeight, juce::Font::plain));
        font.setExtraKerningFactor (0.34f);

        const auto textWidth = juce::GlyphArrangement::getStringWidth (font, "DIAMOND ROOM");

        g.setFont (font);
        g.setColour (theme::colours::textDim.withAlpha (0.75f));
        g.drawText ("DIAMOND ROOM", footer, juce::Justification::centred, false);

        const auto y = footer.getCentreY();
        const auto gap = textWidth * 0.5f + 26.0f * scale;
        const auto ruleLength = 70.0f * scale;

        g.setColour (theme::colours::textDim.withAlpha (0.4f));

        for (int side = 0; side < 2; ++side)
        {
            const auto x1 = plate.getCentreX() + (side == 0 ? -gap : gap);
            g.drawLine (x1, y, x1 + (side == 0 ? -ruleLength : ruleLength), y,
                        juce::jmax (0.8f, 1.2f * scale));
        }
    }

    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRect (bounds, juce::jmax (1.0f, 3.0f * scale));
}

void DiamondRoomAudioProcessorEditor::drawTitle (juce::Graphics& g, float scale)
{
    const auto plate = juce::Rectangle<float> ((float) railWidth * scale, 0.0f,
                                               (float) getWidth() - 2.0f * railWidth * scale,
                                               (float) getHeight());

    // The badge sits at the top edge, like a stone set into the panel.
    {
        const auto d = 50.0f * scale;
        const auto gem = juce::Rectangle<float> (d, d * 0.92f)
                             .withCentre ({ plate.getCentreX(), 30.0f * scale });

        for (int pass = 3; pass >= 1; --pass)
        {
            g.setColour (theme::colours::accent.withAlpha (0.07f * (float) pass));
            g.fillEllipse (gem.expanded (d * 0.20f * (float) pass));
        }

        theme::drawDiamond (g, gem, scale);
    }

    const auto titleArea = juce::Rectangle<float> (plate.getX(), 60.0f * scale,
                                                   plate.getWidth(), 58.0f * scale);
    const auto fontHeight = juce::jmax (11.0f, 46.0f * scale);

    juce::Font font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                        fontHeight, juce::Font::plain));
    font.setExtraKerningFactor (0.30f);

    const auto textWidth = juce::GlyphArrangement::getStringWidth (font, "DIAMOND ROOM");

    g.setFont (font);

    // A soft halo behind the lettering, so it reads as lit rather than printed.
    g.setColour (theme::colours::accent.withAlpha (0.16f));

    for (const auto offset : { -1.5f, 1.5f })
        g.drawText ("DIAMOND ROOM", titleArea.translated (offset * scale, 0.0f),
                    juce::Justification::centred, false);

    g.setColour (theme::colours::textShadow);
    g.drawText ("DIAMOND ROOM", titleArea.translated (0.0f, 2.0f * scale),
                juce::Justification::centred, false);
    g.setColour (juce::Colours::white);
    g.drawText ("DIAMOND ROOM", titleArea, juce::Justification::centred, false);

    // Rules either side of the title, fading out away from the lettering.
    const auto y = titleArea.getCentreY();
    const auto gap = textWidth * 0.5f + 34.0f * scale;
    const auto ruleLength = 150.0f * scale;

    for (int side = 0; side < 2; ++side)
    {
        const auto x1 = plate.getCentreX() + (side == 0 ? -gap : gap);
        const auto x2 = x1 + (side == 0 ? -ruleLength : ruleLength);

        juce::ColourGradient rule (theme::colours::text.withAlpha (0.85f), x1, y,
                                   theme::colours::text.withAlpha (0.0f), x2, y, false);
        g.setGradientFill (rule);
        g.drawLine (x1, y, x2, y, juce::jmax (0.9f, 1.6f * scale));
    }

    // Strapline.
    const auto strapArea = juce::Rectangle<float> (plate.getX(), 116.0f * scale,
                                                   plate.getWidth(), 26.0f * scale);

    juce::Font strapFont (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                             juce::jmax (7.0f, 18.0f * scale), juce::Font::plain));
    strapFont.setExtraKerningFactor (0.45f);

    const auto dot = juce::String::fromUTF8 ("\xc2\xb7");

    g.setFont (strapFont);
    g.setColour (theme::colours::textDim.withAlpha (0.9f));
    g.drawText ("REFLECT " + dot + " SHAPE " + dot + " SPACE",
                strapArea, juce::Justification::centred, false);
}
