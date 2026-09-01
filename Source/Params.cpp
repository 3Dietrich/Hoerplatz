#include "Params.h"

#include <cmath>

namespace
{
    // Werte knapp unter null wuerden sonst als "-0.00" erscheinen.
    juce::String metres (float v, int)
    {
        if (std::abs (v) < 0.005f) v = 0.0f;
        return juce::String (v, 2) + " m";
    }

    std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name,
                                                          float lo, float hi, float def, float step = 0.01f)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> { lo, hi, step }, def,
            juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (metres));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout Params::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeFloat (speakerDistance, "Boxenabstand", 0.30f, 12.0f, 5.50f));
    layout.add (makeFloat (roomWidth,       "Raumbreite",   1.00f, 20.0f, 6.50f));
    layout.add (makeFloat (roomDepth,       "Raumtiefe",    1.00f, 20.0f, 6.50f));

    // Hoerplatz in Metern, Ursprung = Mitte zwischen den Boxen, +y in den
    // Raum hinein. Als Parameter gefuehrt, damit er sich automatisieren und
    // im Projekt speichern laesst - das Ziehen im Grundriss schreibt
    // denselben Wert.
    layout.add (makeFloat (listenerX, "Hoerplatz seitlich", -10.0f, 10.0f, 0.00f));

    // Startwert: das gleichseitige Dreieck zum voreingestellten
    // Boxenabstand - die uebliche Ausgangslage fuer Stereo.
    layout.add (makeFloat (listenerY, "Hoerplatz Abstand",    0.10f, 20.0f, 4.76f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { bypassDelay, 1 }, "Laufzeit umgehen", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { bypassGain, 1 }, "Pegel umgehen", false));

    return layout;
}
