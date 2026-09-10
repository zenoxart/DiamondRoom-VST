#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace dr::theme
{

// The whole UI is laid out in these design pixels and scaled to fit, so the
// numbers below match the panel artwork one to one.
static constexpr int designWidth  = 2001;
static constexpr int designHeight = 764;

namespace colours
{
    const juce::Colour panelBase   { 0xff5f5e5a };
    const juce::Colour panelDark   { 0xff3a3936 };
    const juce::Colour railBase    { 0xff565551 };
    const juce::Colour rust        { 0xff7a5a3c };
    const juce::Colour rustDark    { 0xff4e3524 };

    const juce::Colour engraveDark { 0xcc1a1a18 };
    const juce::Colour engraveLite { 0x40ffffff };
    const juce::Colour text        { 0xff2a2a27 };
    const juce::Colour textLight   { 0xffd8d6d0 };

    const juce::Colour pointer     { 0xff8dff6b };
    const juce::Colour pointerGlow { 0x808dff6b };
    const juce::Colour ledOn       { 0xffff3520 };
    const juce::Colour ledOff      { 0xff5a201a };
    const juce::Colour sliderTint  { 0xffe08a2a };
}

//==============================================================================
/** Weathered steel with rust blooms - generated once, then cached. */
juce::Image createPanelTexture (int width, int height, int seed, float rustAmount);

/** Brushed aluminium knob body, without the pointer. */
juce::Image createKnobBody (int diameter, float scale);

/** The ridged metal cap used on every fader. */
juce::Image createFaderCap (int width, int height, bool horizontal, float scale);

//==============================================================================
/** A recessed screw head, drawn straight to the graphics context. */
void drawScrew (juce::Graphics& g, juce::Point<float> centre, float radius);

/** Engraved lettering: dark glyphs with a light lower edge. */
void drawEngravedText (juce::Graphics& g, const juce::String& text,
                       juce::Rectangle<float> area, juce::Justification justification,
                       float fontHeight, bool bold, juce::Colour colour = colours::text);

/** Beveled sub-panel plate with screws in the corners. */
void drawSubPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                   const juce::Image& texture, float scale);

/** Tick marks around a knob, from 7 o'clock to 5 o'clock. */
void drawKnobTicks (juce::Graphics& g, juce::Point<float> centre, float radius,
                    int numTicks, float scale);

} // namespace dr::theme
