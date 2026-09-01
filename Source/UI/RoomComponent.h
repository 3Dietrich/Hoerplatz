#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Geometry.h"
#include "../Strings.h"

class HoerplatzProcessor;

// Grundriss des Raumes von oben. Beide Boxen und der Hoerplatz lassen sich
// einzeln ziehen - so laesst sich die eigene Aufstellung nachbauen.
// Geschrieben werden dabei dieselben Parameter, die auch die Regler bedienen.
class RoomComponent : public juce::Component,
                      public juce::TooltipClient
{
public:
    explicit RoomComponent (HoerplatzProcessor& p);

    void setLang (Lang l) { lang = l; }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    juce::String getTooltip() override;

private:
    // Was gerade angefasst wird.
    enum class Handle { none, leftSpeaker, rightSpeaker, listener };

    struct View
    {
        float scale = 1.0f;             // Pixel pro Meter
        juce::Point<float> origin;      // Bildschirmpunkt von Welt (0,0)
        juce::Rectangle<float> room;    // Raumrechteck in Pixeln
    };

    View makeView() const;
    juce::Point<float> worldToScreen (const View&, float wx, float wy) const;
    juce::Point<float> worldToScreen (const View&, Geometry::Point) const;
    juce::Point<float> screenToWorld (const View&, juce::Point<float>) const;

    Geometry::Point paramPoint (const char* idX, const char* idY) const;
    Handle handleAt (juce::Point<float> screenPos) const;
    void dragTo (juce::Point<float> screenPos);
    void drawSpeaker (juce::Graphics&, juce::Point<float> pos, float angleToListener,
                      float sizePx, bool highlighted) const;

    HoerplatzProcessor& processor;
    Lang lang = Lang::de;

    Handle grabbed = Handle::none;
    Handle hovered = Handle::none;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomComponent)
};
