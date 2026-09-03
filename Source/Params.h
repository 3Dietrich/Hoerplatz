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
    // Der Boxenabstand ist ein eigener Parameter, obwohl die Standorte der
    // Boxen ihn schon enthalten: so laesst er sich automatisieren und vom
    // Host aus bedienen. Beide Seiten werden gegenseitig nachgefuehrt -
    // siehe HoerplatzProcessor::handleAsyncUpdate.
    inline constexpr const char* speakerDistance = "spkDist";

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

    // Sitzt man hinter der Aufstellung, steht die linke Box vom Platz aus
    // rechts. Steht dieser Schalter an, gehen die Kanaele mit der
    // Kopfrichtung mit - was links gehoert werden soll, kommt dann auch aus
    // der Box, die von dort aus links steht.
    inline constexpr const char* followHead  = "folgtKopf";

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
