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

    for (auto* id : { Params::speakerDistance, Params::leftX, Params::leftY,
                      Params::rightX, Params::rightY })
        apvts.addParameterListener (id, this);
}

HoerplatzProcessor::~HoerplatzProcessor()
{
    for (auto* id : { Params::speakerDistance, Params::leftX, Params::leftY,
                      Params::rightX, Params::rightY })
        apvts.removeParameterListener (id, this);

    cancelPendingUpdate();
}

void HoerplatzProcessor::parameterChanged (const juce::String& parameterID, float)
{
    if (updatingPositions)
        return;

    if (parameterID == Params::speakerDistance)
        distanceChanged.store (true);
    else
        speakersChanged.store (true);

    triggerAsyncUpdate();
}

void HoerplatzProcessor::handleAsyncUpdate()
{
    // Wer zuletzt bewegt wurde, gibt vor. Wird beides im selben Zug
    // gemeldet - etwa beim Laden eines Projekts -, entscheidet der Abstand,
    // denn die Standorte bringen ihn ohnehin mit.
    const bool fromDistance = distanceChanged.exchange (false);
    const bool fromSpeakers = speakersChanged.exchange (false);

    const juce::ScopedValueSetter<bool> guard (updatingPositions, true);

    if (fromDistance)
        applyDistanceToSpeakers();
    else if (fromSpeakers)
        followSpeakersWithDistance();
}

void HoerplatzProcessor::applyDistanceToSpeakers()
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };

    const juce::Point<float> l { get (Params::leftX),  get (Params::leftY) };
    const juce::Point<float> r { get (Params::rightX), get (Params::rightY) };

    const auto centre = (l + r) * 0.5f;
    auto direction = r - l;
    const float length = direction.getDistanceFromOrigin();

    // Stehen beide Boxen aufeinander, gibt es keine Richtung, an der sich
    // das Auseinanderziehen orientieren koennte - dann waagerecht.
    direction = (length > 0.001f ? direction / length : juce::Point<float> { 1.0f, 0.0f });

    const float wanted = get (Params::speakerDistance);
    const float W = get (Params::roomWidth);
    const float D = get (Params::roomDepth);
    const auto clampToRoom = [&] (juce::Point<float> p)
    {
        return juce::Point<float> { juce::jlimit (-0.5f * W + 0.10f, 0.5f * W - 0.10f, p.x),
                                    juce::jlimit (0.10f, D - 0.10f, p.y) };
    };

    const auto newL = clampToRoom (centre - direction * (wanted * 0.5f));
    const auto newR = clampToRoom (centre + direction * (wanted * 0.5f));

    set (Params::leftX,  newL.x);
    set (Params::leftY,  newL.y);
    set (Params::rightX, newR.x);
    set (Params::rightY, newR.y);
}

void HoerplatzProcessor::followSpeakersWithDistance()
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const Geometry::Point l { get (Params::leftX),  get (Params::leftY) };
    const Geometry::Point r { get (Params::rightX), get (Params::rightY) };
    const auto distance = (float) Geometry::speakerDistance (l, r);

    if (auto* prm = apvts.getParameter (Params::speakerDistance))
        if (std::abs (prm->convertFrom0to1 (prm->getValue()) - distance) > 0.005f)
            prm->setValueNotifyingHost (prm->convertTo0to1 (distance));
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

        // Der Pegelausgleich hebt eine Seite an. Ob das zu weit geht, sieht
        // man sonst erst, wenn man es hoert.
        if (std::abs (left[i])  >= 1.0f) clipped[0].store (true);
        if (std::abs (right[i]) >= 1.0f) clipped[1].store (true);
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
