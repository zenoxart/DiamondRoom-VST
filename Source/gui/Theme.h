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
/**
    Faceted crystal, generated once and cached. Superseded by the photographic
    PanelBackground asset (see PluginEditor.cpp), which is the actual crystal
    field from the reference rather than an approximation of it, but kept as a
    fallback should the background ever need to scale to a size or aspect
    ratio the source crop cannot cover well.

    `brightness` scales the whole field. `shardSize` is the rough width a facet
    should end up, in pixels, which is what decides how deep the subdivision
    runs. `edgeFalloff` (0..1) fades the field out towards the horizontal
    centre of the image, so it reads as a crystal formation breaking in from
    the left and right rather than a texture that fills the whole plate. 0
    disables the fade entirely.
*/
juce::Image createCrystalTexture (int width, int height, int seed, float brightness,
                                  float shardSize, float edgeFalloff = 0.0f);

/** Plain brushed dark metal, no crystal - what the control plates sit on. */
juce::Image createBrushedMetalTexture (int width, int height, int seed);

/** Procedural diamond-cut knob body, without the pointer. Superseded by the
    photographic KnobBody asset (see MetalKnob.cpp) but kept as a fallback
    should that ever need replacing without new reference art on hand. */
juce::Image createKnobBody (int diameter, float scale);

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

/** The glowing, gradient-filled cut - the title badge. */
void drawDiamond (juce::Graphics& g, juce::Rectangle<float> bounds, float scale);

/** A plain line-art outline of the same cut - the small mark on the rails. */
void drawDiamondOutline (juce::Graphics& g, juce::Rectangle<float> bounds, float scale,
                         juce::Colour colour = colours::text);

} // namespace dr::theme
