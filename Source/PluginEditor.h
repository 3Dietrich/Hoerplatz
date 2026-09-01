#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "Strings.h"
#include "UI/RoomComponent.h"

// Zahlenfeld rechts unten: welche Seite um wieviel verzoegert und abgesenkt
// wird. Es rechnet aus den Parametern, nicht aus dem Audiothread - so
// stimmen die Werte auch, wenn gerade nichts spielt.
class ReadoutPanel : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    explicit ReadoutPanel (HoerplatzProcessor& p) : processor (p) {}
    void setLang (Lang l) { lang = l; }
    void paint (juce::Graphics&) override;

private:
    HoerplatzProcessor& processor;
    Lang lang = Lang::de;
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
    void applyLanguage();
    void applyHelp();
    void updateArea();
    void updateSpeakerDistanceSlider();
    void setSpeakerDistance (float metres);

    // Ein Regler mit Beschriftung darueber - kompakt, damit rechts genug
    // Platz fuer alle bleibt.
    struct Row
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    void styleSlider (juce::Slider&);
    void setUpRow (Row&, const char* paramId, const juce::String& tooltip);

    HoerplatzProcessor& plugin;

    RoomComponent room;
    ReadoutPanel readout;

    // Der Boxenabstand hat keinen eigenen Parameter - die beiden Boxen
    // stehen einzeln im Raum. Der Regler zieht sie um ihre gemeinsame Mitte
    // auseinander und zeigt an, wie weit sie gerade auseinanderstehen.
    Row speakerDistance;
    Row roomWidth, roomDepth, listenerX, listenerY;

    juce::ToggleButton bypassDelay, bypassGain;

    // Wie weit der Pegelausgleich geht - steht beim Pegel-Bypass, weil beide
    // dasselbe betreffen.
    juce::Slider gainAmountKnob;
    juce::Label gainAmountLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAmountAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassDelayAttach, bypassGainAttach;

    juce::Label areaLabel;

    // Testgeraeusch zum Einrichten. Der Knopf leuchtet nach, solange der
    // Ausklang laeuft.
    juce::TextButton testToneButton;
    bool tailWasSounding = false;

    juce::TextButton langButton, helpButton;
    std::unique_ptr<juce::TooltipWindow> tooltips;

    Lang lang = Lang::de;
    bool showHelp = true;

    // Feste Zeichenbreite fuer die Zahlen in den Reglerfeldern: beim Ziehen
    // soll dort nur die Ziffer wechseln, nicht ihre Lage. Betroffen sind
    // allein die Textfelder der Regler, nicht die uebrigen Beschriftungen.
    struct MonoLookAndFeel : juce::LookAndFeel_V4
    {
        juce::Label* createSliderTextBox (juce::Slider&) override;

        // Schlanker Ring statt des dicken Bogens der Vorgabe - dieselbe
        // zurueckhaltende Linienstaerke wie im Rest der Oberflaeche.
        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float startAngle, float endAngle,
                               juce::Slider&) override;
    };
    MonoLookAndFeel monoLnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HoerplatzEditor)
};
