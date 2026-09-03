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

    // Ein Regler mit Beschriftung darueber - kompakt, damit rechts genug
    // Platz fuer alle bleibt.
    struct Row
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    // Nicht alles ist gleich wichtig: Raummasse und Zahlenfeld dienen der
    // Uebersicht, eingestellt wird mit den anderen. Das zeigen gedaempfte
    // Farben und kleinere Schrift - und dass sie als erste weichen, wenn das
    // Fenster klein wird.
    void styleSlider (juce::Slider&, bool primary);

    // Ohne Parameterkennung bleibt der Regler ohne Anbindung - so sind die
    // beiden Hoerplatz-Regler gebaut, die nicht den Parameter selbst zeigen,
    // sondern den Platz an der Aufstellung gemessen.
    void setUpRow (Row&, const char* paramId, bool primary);

    // Der Hoerplatz wird an der Achse zwischen den Boxen gemessen, die
    // Parameter fuehren dagegen Weltkoordinaten. Diese beiden rechnen
    // zwischen beidem hin und her.
    void writeSeat();
    void syncSeatSliders();

    HoerplatzProcessor& plugin;

    RoomComponent room;
    ReadoutPanel readout;

    Row speakerDistance, roomWidth, roomDepth, listenerX, listenerY;

    juce::ToggleButton bypassDelay, bypassGain, followHead;

    // Wie weit der Pegelausgleich geht - steht beim Pegel-Bypass, weil beide
    // dasselbe betreffen.
    juce::Slider gainAmountKnob;
    juce::Label gainAmountLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAmountAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassDelayAttach, bypassGainAttach,
                                                                          followHeadAttach;

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
