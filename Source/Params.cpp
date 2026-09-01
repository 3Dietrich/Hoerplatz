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

    // Standardaufstellung: 5,50 m auseinander, 35 cm vor der vorderen Wand.
    layout.add (makeFloat (leftX,  "Box links seitlich",  -12.0f, 12.0f, -2.75f));
    layout.add (makeFloat (leftY,  "Box links Abstand",    -1.0f, 20.0f,  0.35f));
    layout.add (makeFloat (rightX, "Box rechts seitlich", -12.0f, 12.0f,  2.75f));
    layout.add (makeFloat (rightY, "Box rechts Abstand",   -1.0f, 20.0f,  0.35f));

    layout.add (makeFloat (roomWidth, "Raumbreite", 1.00f, 20.0f, 6.50f));
    layout.add (makeFloat (roomDepth, "Raumtiefe",  1.00f, 20.0f, 6.50f));

    // Hoerplatz in Metern, Ursprung = Mitte der vorderen Wand. Als Parameter
    // gefuehrt, damit er sich automatisieren und im Projekt speichern
    // laesst - das Ziehen im Grundriss schreibt denselben Wert.
    layout.add (makeFloat (listenerX, "Hoerplatz seitlich", -12.0f, 12.0f, 0.00f));

    // Startwert: das gleichseitige Dreieck zur Standardaufstellung - die
    // uebliche Ausgangslage fuer Stereo.
    layout.add (makeFloat (listenerY, "Hoerplatz Abstand",   -1.0f, 20.0f, 5.11f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { bypassDelay, 1 }, "Bypass Laufzeit", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { bypassGain, 1 }, "Bypass Pegel", false));

    // Wie weit der Pegelausgleich geht. 100 % ist die Rechnung nach 1/r, wie
    // sie im Freien gilt. In einem Raum kommt Diffusschall dazu: der
    // Unterschied zwischen nah und fern faellt dort viel kleiner aus, die
    // volle Korrektur senkt die naehere Box also zu weit ab.
    //
    // 100 % ist der am Hoerplatz eingestellte Normalfall und liegt deshalb in
    // der Mitte des Reglerweges; die Rechnung fuers Freie sitzt bei rund
    // 141 %, die doppelte Absenkung am oberen Anschlag. Die Umrechnung von
    // Prozent auf den Exponenten steht in Geometry::gainExponent.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gainAmount, 1 }, "Pegel-Ausgleich",
        juce::NormalisableRange<float> { 0.0f, 200.0f, 1.0f }, 100.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (
            [] (float v, int) { return juce::String ((int) v) + " %"; })));

    return layout;
}
