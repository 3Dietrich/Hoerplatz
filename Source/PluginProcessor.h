#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Geometry.h"
#include "TestTone.h"
#include "Params.h"

class HoerplatzProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::AsyncUpdater
{
public:
    HoerplatzProcessor();
    ~HoerplatzProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Hoerplatz"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Standard"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Uebersteuerung je Kanal, im Audiothread gesetzt und von der Anzeige
    // abgeholt. Wer die Korrektur weit aufdreht, hebt einen Kanal an - das
    // soll man sehen, bevor man es hoert.
    std::atomic<bool> clipped[2] { { false }, { false } };

    // Testgeraeusch zum Einrichten. Es wird vor der Korrektur eingespeist
    // und laeuft deshalb durch dieselbe Kette wie die Musik.
    TestTone testTone;

    // Erledigt eine anstehende Nachfuehrung zwischen Boxenabstand und
    // Standorten sofort. Im Betrieb besorgt das die Nachrichtenschleife;
    // Pruefungen ohne laufende Schleife rufen es selbst auf.
    void flushPendingUpdates() { handleUpdateNowIfNeeded(); }

    // Rechnung ohne Audio - der Editor braucht die Werte auch dann, wenn
    // gerade kein Block laeuft (Transport gestoppt, Fenster offen).
    Geometry::Alignment currentAlignment() const;

private:
    // Boxenabstand und Standorte halten sich gegenseitig auf dem Laufenden.
    // Die Meldung kann aus dem Audiothread kommen, das Setzen von Parametern
    // gehoert aber in den Nachrichtenthread - deshalb der Umweg ueber den
    // AsyncUpdater.
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void applyDistanceToSpeakers();
    void followSpeakersWithDistance();

    std::atomic<bool> distanceChanged { false };
    std::atomic<bool> speakersChanged { false };
    bool updatingPositions = false;

    // Grundverzoegerung beider Kanaele in Samples. Die Interpolation der
    // Delayline braucht ein paar Samples Anlauf, um sauber zu arbeiten;
    // weil beide Kanaele denselben Sockel bekommen, aendert er am Ergebnis
    // nichts und wird als Latenz gemeldet.
    static constexpr float baseDelaySamples = 4.0f;

    using Delay = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>;
    Delay delayL { 1 }, delayR { 1 };

    juce::SmoothedValue<float> smoothDelayL, smoothDelayR, smoothGainL, smoothGainR;

    double sampleRate = 44100.0;

    // Arbeitsspeicher fuer das Testgeraeusch, blockweise vorgehalten.
    juce::AudioBuffer<float> testBuffer;
    std::vector<float> testMix;

    std::atomic<float>* pLeftX = nullptr;
    std::atomic<float>* pLeftY = nullptr;
    std::atomic<float>* pRightX = nullptr;
    std::atomic<float>* pRightY = nullptr;
    std::atomic<float>* pListenerX = nullptr;
    std::atomic<float>* pListenerY = nullptr;
    std::atomic<float>* pBypassDelay = nullptr;
    std::atomic<float>* pBypassGain = nullptr;
    std::atomic<float>* pGainAmount = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HoerplatzProcessor)
};
