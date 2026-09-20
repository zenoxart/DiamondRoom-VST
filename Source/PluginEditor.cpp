#include "PluginEditor.h"
#include <BinaryData.h>

using namespace dr;

namespace
{
    // Complete user-supplied artwork, including title and side branding.
    juce::Image panelBackgroundAsset()
    {
        return juce::ImageCache::getFromMemory (
            BinaryData::PanelBackground_png, BinaryData::PanelBackground_pngSize);
    }

    // Panel artwork coordinates, in design units (2001 x 764).
    const juce::Rectangle<int> drivePanelArea { 110, 150, 238, 380 };
    const juce::Rectangle<int> tubePanelArea  { 1652, 150, 238, 380 };
    const juce::Rectangle<int> mixPanelArea   { 110, 542, 1780, 146 };

    const juce::Rectangle<int> hPanelArea     { 362,  150, 310, 380 };
    const juce::Rectangle<int> mannyPanelArea { 683,  150, 310, 380 };
    const juce::Rectangle<int> valPanelArea   { 1004, 150, 310, 380 };
    const juce::Rectangle<int> truePanelArea  { 1325, 150, 310, 380 };


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

    // Map the whole control bank into the artwork's inner frame, including
    // its controls, so resizing keeps the plates and hit areas aligned.
    juce::Rectangle<int> controlRect (juce::Rectangle<int> design, float scale)
    {
        const auto mapX = [scale] (int x)
        {
            return juce::roundToInt ((122.0f + (float) (x - 110) * 1758.0f / 1780.0f) * scale);
        };
        const auto mapY = [scale] (int y)
        {
            return juce::roundToInt ((123.0f + (float) (y - 150) * 563.0f / 538.0f) * scale);
        };
        return { mapX (design.getX()), mapY (design.getY()),
                 mapX (design.getRight()) - mapX (design.getX()),
                 mapY (design.getBottom()) - mapY (design.getY()) };
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

    // The crystal field itself is panelBackgroundAsset() now (drawn in
    // paint()), so the only texture still generated here is the plain brushed
    // metal the control plates sit on, which has no crystal in it at all and
    // is what keeps a bank of six plates from turning into visual noise.
    plateTexture = theme::createBrushedMetalTexture (juce::jmax (1, w / 3), juce::jmax (1, h / 3), 77345);

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

    drivePanel.setBounds (controlRect (drivePanelArea, scale));
    tubePanel.setBounds  (controlRect (tubePanelArea, scale));
    mixPanel.setBounds   (controlRect (mixPanelArea, scale));

    // The two big knobs, centred on their plates.
    driveKnob.setDesignScale (scale);
    tubeKnob.setDesignScale (scale);
    driveKnob.setBounds (controlRect ({ 118,  205, 228, 300 }, scale));
    tubeKnob.setBounds  (controlRect ({ 1660, 205, 228, 300 }, scale));

    masterMix.setDesignScale (scale);
    masterMix.setBounds (controlRect ({ 255, 544, 1490, 144 }, scale));

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
    strip.panel.setBounds (controlRect (panelBounds, scale));

    const auto x = panelBounds.getX();

    strip.topKnob.setDesignScale (scale);
    strip.bottomKnob.setDesignScale (scale);
    strip.mix.setDesignScale (scale);

    // The knob and fader components are wider than their controls: the extra
    // room is where the captions go.
    strip.topKnob.setBounds    (controlRect ({ x + 14,  205, 170, 160 }, scale));
    strip.bottomKnob.setBounds (controlRect ({ x + 14,  370, 170, 160 }, scale));
    strip.mix.setBounds        (controlRect ({ x + 150, 205, 170, 325 }, scale));
}

//==============================================================================
void DiamondRoomAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (dr::theme::colours::backdropDeep);
    const auto& background = panelBackgroundAsset();
    if (background.isValid())
    {
        g.setOpacity (1.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    }
    // The artwork already contains the frame, title, tagline and side labels.
    // Interactive child components are painted above this background by JUCE.
}
