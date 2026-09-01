#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/RoomComponent.h"

// Zahlenfeld rechts unten: Abstaende, Laufzeiten, Pegel, Winkel. Es rechnet
// aus den Parametern, nicht aus dem Audiothread - so stimmen die Werte auch,
// wenn gerade nichts spielt.
class ReadoutPanel : public juce::Component
{
public:
    explicit ReadoutPanel (HoerplatzProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;

private:
    HoerplatzProcessor& processor;
};

class HoerplatzEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit HoerplatzEditor (HoerplatzProcessor&);
    ~HoerplatzEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateArea();

    // Ein Regler mit Beschriftung darueber - kompakt, damit rechts genug
    // Platz fuer alle bleibt.
    struct Row
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    void setUpRow (Row&, const char* paramId, const juce::String& text);

    HoerplatzProcessor& plugin;

    RoomComponent room;
    ReadoutPanel readout;

    Row speakerDistance, roomWidth, roomDepth, listenerX, listenerY;

    juce::ToggleButton bypassDelay { "Laufzeit umgehen" };
    juce::ToggleButton bypassGain  { "Pegel umgehen" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassDelayAttach, bypassGainAttach;

    juce::Label areaLabel;
    juce::Label hintLabel;

    // Feste Zeichenbreite fuer die Zahlen in den Reglerfeldern: beim Ziehen
    // soll dort nur die Ziffer wechseln, nicht ihre Lage. Betroffen sind
    // allein die Textfelder der Regler, nicht die uebrigen Beschriftungen.
    struct MonoLookAndFeel : juce::LookAndFeel_V4
    {
        juce::Label* createSliderTextBox (juce::Slider&) override;
    };
    MonoLookAndFeel monoLnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HoerplatzEditor)
};
