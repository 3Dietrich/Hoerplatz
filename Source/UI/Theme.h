#pragma once

#include <juce_graphics/juce_graphics.h>

// Dunkle Palette, kontrastarme Linien, kleine Radien - dieselbe Haltung wie
// in den uebrigen Oberflaechen.
namespace Theme
{
    inline const juce::Colour ground   { 0xff0a0b10 };
    inline const juce::Colour surface  { 0xff15161d };
    inline const juce::Colour surface2 { 0xff1a1c25 };
    inline const juce::Colour line     { juce::Colours::white.withAlpha (0.09f) };
    inline const juce::Colour text     { 0xffeef0f4 };
    inline const juce::Colour textDim  { 0xffbec5d7 };
    inline const juce::Colour cyan     { 0xff52d3e6 };
    inline const juce::Colour amber    { 0xfff2a94e };
    inline const juce::Colour pink     { 0xffef5fa6 };

    inline constexpr float corner = 3.0f;

    // Kleinste Schriftgroesse der Oberflaeche - Massband, Weglaengen,
    // Beschriftung des Knopfes.
    inline constexpr float smallText = 13.75f;
}
