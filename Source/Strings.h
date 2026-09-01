#pragma once

#include <juce_core/juce_core.h>

// Alle sichtbaren Texte an einer Stelle, deutsch und englisch nebeneinander.
// Wer eine Beschriftung aendert, aendert sie hier - im Code steht kein
// freier Text mehr.
enum class Lang { de, en };

// Die Texte stehen als UTF-8-Literale im Quelltext. juce::String liest ein
// nacktes const char* als ASCII - Umlaute kaemen dabei zerlegt an. Deshalb
// geht jeder Text durch diese Huelle, die ihn als UTF-8 uebergibt.
struct Text
{
    const char* utf8;
    operator juce::String() const { return juce::String::fromUTF8 (utf8); }
};

struct Texts
{
    Text title;
    Text subtitle;

    Text speakerDistance;
    Text roomWidth;
    Text roomDepth;
    Text area;
    Text listenerX;
    Text listenerY;

    Text bypassDelay;
    Text bypassGain;
    Text gainAmount;

    Text correction;     // Ueberschrift des Zahlenfelds
    Text delayRow;
    Text gainRow;
    Text sideLeft;
    Text sideRight;
    Text centred;        // wenn beide Wege gleich lang sind
    Text off;            // Wert bei gesetztem Bypass

    // Hilfetexte (erscheinen als Sprechblase, wenn "?" eingeschaltet ist)
    Text helpHints;
    Text helpLang;
    Text helpRoomPlan;
    Text helpSpeaker;
    Text helpListener;
    Text helpSpeakerDistance;
    Text helpRoomWidth;
    Text helpRoomDepth;
    Text helpListenerX;
    Text helpListenerY;
    Text helpBypassDelay;
    Text helpBypassGain;
    Text helpGainAmount;
    Text helpReadout;
};

const Texts& texts (Lang lang);
