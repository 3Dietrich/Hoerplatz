#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Parameter-Kennungen und das Layout an einer Stelle. Die Bereiche sind
// bewusst weit gefasst: der genannte Einsatzbereich sind Raeume von 5 bis
// 90 m2, die Regler koennen aber deutlich darueber hinaus, damit sich
// niemand an einer erfundenen Obergrenze stoesst.
//
// Beide Boxen stehen einzeln im Raum - so laesst sich die eigene Aufstellung
// nachbauen, auch wenn sie nicht symmetrisch ist.
namespace Params
{
    inline constexpr const char* leftX      = "spkLx";
    inline constexpr const char* leftY      = "spkLy";
    inline constexpr const char* rightX     = "spkRx";
    inline constexpr const char* rightY     = "spkRy";
    inline constexpr const char* roomWidth  = "roomW";
    inline constexpr const char* roomDepth  = "roomD";
    inline constexpr const char* listenerX  = "posX";
    inline constexpr const char* listenerY  = "posY";
    inline constexpr const char* bypassDelay = "byDelay";
    inline constexpr const char* bypassGain  = "byGain";
    inline constexpr const char* gainAmount  = "gainAmt";

    // Sprache und Hilfe sind Sache der Oberflaeche, nicht des Klangs. Sie
    // liegen deshalb als Eigenschaften im gespeicherten Zustand und nicht
    // als automatisierbare Parameter.
    inline constexpr const char* language = "sprache";
    inline constexpr const char* showHelp = "hilfe";

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Zahl aus einer Eingabe lesen. Nimmt Komma wie Punkt und ignoriert
    // alles, was keine Zahl ist - "3,20 m" und "3.2" fuehren zum selben
    // Wert wie "3,2 Meter".
    double parseNumber (const juce::String& text);
}
