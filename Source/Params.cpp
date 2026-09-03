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
            juce::AudioParameterFloatAttributes{}
                .withStringFromValueFunction (metres)
                .withValueFromStringFunction ([] (const juce::String& t) { return (float) Params::parseNumber (t); }));
    }
}

double Params::parseNumber (const juce::String& text)
{
    return text.trim()
               .replaceCharacter (',', '.')
               .replace (juce::String::fromUTF8 ("\xe2\x88\x92"), "-")   // typografisches Minus
               .retainCharacters ("0123456789.+-")
               .getDoubleValue();
}

juce::AudioProcessorValueTreeState::ParameterLayout Params::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Zieht beide Boxen um ihre gemeinsame Mitte auf diesen Abstand und
    // folgt umgekehrt, wenn eine Box verschoben wird.
    layout.add (makeFloat (speakerDistance, "Boxenabstand", 0.30f, 24.0f, 5.50f));

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
    //
    // Der Name sagt "im Raum", weil hier Weltkoordinaten stehen. Die beiden
    // Regler der Oberflaeche zeigen den Platz dagegen an der Aufstellung
    // gemessen - quer aus der Mitte zwischen den Boxen, laengs senkrecht zur
    // Achse zwischen ihnen - und rechnen zwischen beidem um.
    layout.add (makeFloat (listenerX, "Hoerplatz X im Raum", -12.0f, 12.0f, 0.00f));

    // Startwert: das gleichseitige Dreieck zur Standardaufstellung - die
    // uebliche Ausgangslage fuer Stereo.
    layout.add (makeFloat (listenerY, "Hoerplatz Y im Raum",  -1.0f, 20.0f, 5.11f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { bypassDelay, 1 }, "Bypass Laufzeit", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { bypassGain, 1 }, "Bypass Pegel", false));

    // Vorgabe an: sitzt man hinter der Aufstellung, sollen die Kanaele von
    // selbst mitgehen - sonst steht das Stereobild spiegelverkehrt, ohne
    // dass etwas darauf hinweist.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { followHead, 1 }, "L/R folgen Kopfrichtung", true));

    // Wie weit der Pegelausgleich geht. 100 % ist die Rechnung nach 1/r, wie
    // sie im Freien gilt. In einem Raum kommt Diffusschall dazu: der
    // Unterschied zwischen nah und fern faellt dort viel kleiner aus, die
    // volle Korrektur senkt die naehere Box also zu weit ab.
    //
    // 100 % ist die Rechnung nach 1/r, wie sie im Freien gilt. In einem Raum
    // kommt Diffusschall dazu: der Unterschied zwischen nah und fern faellt
    // dort viel kleiner aus, die volle Korrektur waere zu stark.
    //
    // Die Vorgabe steht auf der vollen Rechnung. Wieviel davon im eigenen
    // Raum richtig ist, entscheidet das Ohr - in moeblierten Zimmern liegt
    // der Punkt oft deutlich darunter, weil der Diffusschall den Unterschied
    // zwischen nah und fern kleiner macht, als 1/r ihn rechnet.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gainAmount, 1 }, "Pegel-Ausgleich",
        juce::NormalisableRange<float> { 0.0f, 200.0f, 1.0f }, 100.0f,
        juce::AudioParameterFloatAttributes{}
            .withStringFromValueFunction ([] (float v, int) { return juce::String ((int) v) + " %"; })
            .withValueFromStringFunction ([] (const juce::String& t) { return (float) Params::parseNumber (t); })));

    return layout;
}
