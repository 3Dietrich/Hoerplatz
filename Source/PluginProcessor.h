#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Geometry.h"
#include "Params.h"

class HoerplatzProcessor : public juce::AudioProcessor
{
public:
    HoerplatzProcessor();
    ~HoerplatzProcessor() override = default;

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

    // Rechnung ohne Audio - der Editor braucht die Werte auch dann, wenn
    // gerade kein Block laeuft (Transport gestoppt, Fenster offen).
    Geometry::Alignment currentAlignment() const;

private:
    // Grundverzoegerung beider Kanaele in Samples. Die Interpolation der
    // Delayline braucht ein paar Samples Anlauf, um sauber zu arbeiten;
    // weil beide Kanaele denselben Sockel bekommen, aendert er am Ergebnis
    // nichts und wird als Latenz gemeldet.
    static constexpr float baseDelaySamples = 4.0f;

    using Delay = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>;
    Delay delayL { 1 }, delayR { 1 };

    juce::SmoothedValue<float> smoothDelayL, smoothDelayR, smoothGainL, smoothGainR;

    double sampleRate = 44100.0;

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
