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
    const juce::Colour backdropDeep { 0xff0b0f16 };
    const juce::Colour backdropMid  { 0xff141b26 };
    const juce::Colour panelFill    { 0xff19202d };
    const juce::Colour panelEdge    { 0xff44566e };

    const juce::Colour text       { 0xffdae6f5 };
    const juce::Colour textDim    { 0xff8ba0ba };
    const juce::Colour textShadow { 0xcc05080d };

    const juce::Colour accent     { 0xff6fd3ff };
    const juce::Colour accentDeep { 0xff2a7fb8 };
    const juce::Colour pointer    { 0xff8fe0ff };
    const juce::Colour ledOn      { 0xff5cc8ff };
    const juce::Colour ledOff     { 0xff17303f };
}

//==============================================================================
/** Faceted crystal, generated once and cached.

    `brightness` scales the whole field: the side rails are cut bright, the
    panel behind the plates is dark enough to read controls against.
    `shardSize` is the rough width a facet should end up, in pixels, which is
    what decides how deep the subdivision runs. */
juce::Image createCrystalTexture (int width, int height, int seed,
                                  float brightness, float shardSize);

/** Diamond-cut knob body, without the pointer. */
juce::Image createKnobBody (int diameter, float scale);

/** The emerald-cut gem used as a fader cap. */
juce::Image createFaderCap (int width, int height, bool horizontal, float scale);

//==============================================================================
/** A recessed screw head. */
void drawScrew (juce::Graphics& g, juce::Point<float> centre, float radius);

/** Lettering: a dark drop shadow under a light face, for a lit-glass look. */
void drawEngravedText (juce::Graphics& g, const juce::String& text,
                       juce::Rectangle<float> area, juce::Justification justification,
                       float fontHeight, bool bold, juce::Colour colour = colours::text);

/** Glass plate with a lit edge and screws in the corners. */
void drawSubPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                   const juce::Image& texture, float scale);

/** Tick marks around a knob, from 7 o'clock to 5 o'clock. */
void drawKnobTicks (juce::Graphics& g, juce::Point<float> centre, float radius,
                    int numTicks, float scale);

/** A brilliant-cut gem seen face on, used as the badge and the rail marks. */
juce::Path makeDiamondPath (juce::Rectangle<float> bounds);
void drawDiamond (juce::Graphics& g, juce::Rectangle<float> bounds, float scale);

} // namespace dr::theme
