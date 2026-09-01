#include "PluginProcessor.h"
#include "PluginEditor.h"

HoerplatzProcessor::HoerplatzProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Eingang", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Ausgang", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Hoerplatz", Params::createLayout())
{
    pLeftX  = apvts.getRawParameterValue (Params::leftX);
    pLeftY  = apvts.getRawParameterValue (Params::leftY);
    pRightX = apvts.getRawParameterValue (Params::rightX);
    pRightY = apvts.getRawParameterValue (Params::rightY);
    pListenerX       = apvts.getRawParameterValue (Params::listenerX);
    pListenerY       = apvts.getRawParameterValue (Params::listenerY);
    pBypassDelay     = apvts.getRawParameterValue (Params::bypassDelay);
    pBypassGain      = apvts.getRawParameterValue (Params::bypassGain);
    pGainAmount      = apvts.getRawParameterValue (Params::gainAmount);
}

bool HoerplatzProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == out;
}

void HoerplatzProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;

    // Der Laufzeitunterschied zwischen den Boxen kann nie groesser werden
    // als ihr Abstand geteilt durch die Schallgeschwindigkeit (Dreiecks-
    // ungleichung). Auch quer durch den groessten einstellbaren Raum sind
    // das keine 90 ms; 250 ms Puffer lassen reichlich Luft.
    const int maxSamples = (int) (0.25 * sampleRate) + 8;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    for (auto* d : { &delayL, &delayR })
    {
        d->setMaximumDelayInSamples (maxSamples);
        d->prepare (spec);
        d->reset();
    }

    for (auto* s : { &smoothDelayL, &smoothDelayR })
    {
        // Traege genug, dass das Ziehen des Hoerplatzes nicht knackt, und
        // schnell genug, dass es der Bewegung folgt.
        s->reset (sampleRate, 0.08);
        s->setCurrentAndTargetValue (baseDelaySamples);
    }
    for (auto* s : { &smoothGainL, &smoothGainR })
    {
        s->reset (sampleRate, 0.03);
        s->setCurrentAndTargetValue (1.0f);
    }

    testTone.prepare (sampleRate);
    testBuffer.setSize (2, samplesPerBlock);
    testMix.assign ((size_t) samplesPerBlock, 0.0f);

    setLatencySamples ((int) baseDelaySamples);
}

Geometry::Alignment HoerplatzProcessor::currentAlignment() const
{
    return Geometry::compute ({ pLeftX->load(),  pLeftY->load() },
                              { pRightX->load(), pRightY->load() },
                              { pListenerX->load(), pListenerY->load() },
                              Geometry::gainExponent (pGainAmount->load()));
}

void HoerplatzProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (buffer.getNumChannels() < 2)
        return;

    const auto a = currentAlignment();

    const bool noDelay = pBypassDelay->load() > 0.5f;
    const bool noGain  = pBypassGain->load()  > 0.5f;

    smoothDelayL.setTargetValue (baseDelaySamples + (noDelay ? 0.0f : (float) (a.delayL * sampleRate)));
    smoothDelayR.setTargetValue (baseDelaySamples + (noDelay ? 0.0f : (float) (a.delayR * sampleRate)));
    smoothGainL.setTargetValue (noGain ? 1.0f : (float) a.gainL);
    smoothGainR.setTargetValue (noGain ? 1.0f : (float) a.gainR);

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    // Testgeraeusch verdraengt das anliegende Signal, solange es klingt -
    // beim Einrichten will man nur es hoeren, und sein Ausklang gehoert ihm
    // allein. Danach kommt die Musik ueber eine kurze Rampe zurueck.
    if (testBuffer.getNumSamples() < numSamples)
    {
        testBuffer.setSize (2, numSamples, false, false, true);
        testMix.assign ((size_t) numSamples, 0.0f);
    }

    testTone.render (testBuffer.getWritePointer (0), testBuffer.getWritePointer (1),
                     numSamples, testMix.data());

    for (int i = 0; i < numSamples; ++i)
    {
        const float m = testMix[(size_t) i];
        if (m <= 0.0f)
            continue;

        left[i]  = left[i]  * (1.0f - m) + testBuffer.getSample (0, i) * m;
        right[i] = right[i] * (1.0f - m) + testBuffer.getSample (1, i) * m;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        delayL.setDelay (smoothDelayL.getNextValue());
        delayR.setDelay (smoothDelayR.getNextValue());

        delayL.pushSample (0, left[i]);
        delayR.pushSample (0, right[i]);

        left[i]  = delayL.popSample (0) * smoothGainL.getNextValue();
        right[i] = delayR.popSample (0) * smoothGainR.getNextValue();
    }
}

juce::AudioProcessorEditor* HoerplatzProcessor::createEditor()
{
    return new HoerplatzEditor (*this);
}

void HoerplatzProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HoerplatzProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HoerplatzProcessor();
}
