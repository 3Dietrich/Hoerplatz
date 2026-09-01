#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class HoerplatzProcessor;

// Grundriss des Raumes von oben: zwei Boxen an der vorderen Wand, dazwischen
// der frei verschiebbare Hoerplatz. Gezogen wird direkt am Kopf; geschrieben
// werden dabei dieselben Parameter, die auch die Regler bedienen.
class RoomComponent : public juce::Component
{
public:
    explicit RoomComponent (HoerplatzProcessor& p);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    // Abstand der Boxenebene zur vorderen Wand (m). Rein zeichnerisch - der
    // Schall interessiert sich nicht dafuer, gerechnet wird ohne Waende.
    static constexpr float frontGap = 0.35f;

    struct View
    {
        float scale = 1.0f;             // Pixel pro Meter
        juce::Point<float> origin;      // Bildschirmpunkt von Welt (0,0)
        juce::Rectangle<float> room;    // Raumrechteck in Pixeln
    };

    View makeView() const;
    juce::Point<float> worldToScreen (const View&, float wx, float wy) const;
    juce::Point<float> screenToWorld (const View&, juce::Point<float>) const;

    void setListenerFromMouse (juce::Point<float> screenPos);
    void drawSpeaker (juce::Graphics&, juce::Point<float> pos, float angleToListener, float sizePx) const;

    HoerplatzProcessor& processor;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomComponent)
};
