// Nachweis am Signal: ein Impuls durch das Plugin geschickt, danach wird
// geprueft, wo er in beiden Kanaelen wieder herauskommt und wie laut. Die
// erwarteten Werte kommen aus der Geometrie, nicht aus dem Code, der sie
// erzeugt hat.
#include "../Source/PluginProcessor.h"

#include <cmath>
#include <cstdio>

namespace
{
    int failures = 0;

    void check (bool ok, const char* what, int line)
    {
        if (! ok)
        {
            std::printf ("FEHLGESCHLAGEN (Zeile %d): %s\n", line, what);
            ++failures;
        }
    }

    struct Peak { int index = -1; float value = 0.0f; };

    Peak findPeak (const juce::AudioBuffer<float>& b, int channel)
    {
        Peak p;
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (std::abs (b.getSample (channel, i)) > std::abs (p.value))
                p = { i, b.getSample (channel, i) };
        return p;
    }

    void setParam (HoerplatzProcessor& proc, const char* id, float value)
    {
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    // Impulsantwort bei eingeschwungenen Parametern: erst laeuft Stille
    // durch, damit die Glaettung ihr Ziel erreicht hat, dann der Impuls.
    juce::AudioBuffer<float> impulseResponse (HoerplatzProcessor& proc, double sr, int tailSamples)
    {
        const int block = 512;
        proc.prepareToPlay (sr, block);

        juce::AudioBuffer<float> silence (2, block);
        juce::MidiBuffer midi;
        silence.clear();
        for (int i = 0; i < 40; ++i)
        {
            silence.clear();
            proc.processBlock (silence, midi);
        }

        juce::AudioBuffer<float> buffer (2, tailSamples);
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        buffer.setSample (1, 0, 1.0f);
        proc.processBlock (buffer, midi);
        return buffer;
    }
}

#define CHECK(cond) check ((cond), #cond, __LINE__)

int main()
{
    const juce::ScopedJuceInitialiser_GUI init;
    constexpr double sr = 48000.0;

    // Hoerplatz links vorne: die linke Box ist deutlich naeher.
    {
        HoerplatzProcessor proc;
        setParam (proc, Params::speakerDistance, 5.5f);
        setParam (proc, Params::listenerX, -2.1f);
        setParam (proc, Params::listenerY,  1.6f);

        const auto a = Geometry::compute (5.5, -2.1, 1.6);
        const auto buffer = impulseResponse (proc, sr, 2048);

        const auto l = findPeak (buffer, 0);
        const auto r = findPeak (buffer, 1);

        // Der weiter entfernte Kanal laeuft ohne Zusatzverzoegerung durch,
        // der naehere kommt genau um den Laufzeitunterschied spaeter.
        const int expectedShift = (int) std::lround ((a.delayL - a.delayR) * sr);
        CHECK (std::abs ((l.index - r.index) - expectedShift) <= 1);

        // Pegel: der naehere Kanal ist im Verhaeltnis der Abstaende leiser.
        CHECK (std::abs (r.value - 1.0f) < 0.02f);
        CHECK (std::abs (l.value - (float) a.gainL) < 0.02f);

        std::printf ("links: Index %d, Wert %.3f (erwartet %.3f)\n", l.index, l.value, a.gainL);
        std::printf ("rechts: Index %d, Wert %.3f\n", r.index, r.value);
    }

    // Beide Umgehungen an: das Signal geht unveraendert durch, beide
    // Kanaele liegen wieder aufeinander und stehen auf vollem Pegel.
    {
        HoerplatzProcessor proc;
        setParam (proc, Params::speakerDistance, 5.5f);
        setParam (proc, Params::listenerX, -2.1f);
        setParam (proc, Params::listenerY,  1.6f);
        setParam (proc, Params::bypassDelay, 1.0f);
        setParam (proc, Params::bypassGain,  1.0f);

        const auto buffer = impulseResponse (proc, sr, 1024);
        const auto l = findPeak (buffer, 0);
        const auto r = findPeak (buffer, 1);

        CHECK (l.index == r.index);
        CHECK (std::abs (l.value - 1.0f) < 0.02f && std::abs (r.value - 1.0f) < 0.02f);
    }

    // Nur die Laufzeit umgangen: die Pegel stehen weiter im Verhaeltnis der
    // Abstaende, die Impulse liegen aber wieder gleichauf.
    {
        HoerplatzProcessor proc;
        setParam (proc, Params::speakerDistance, 5.5f);
        setParam (proc, Params::listenerX, 1.8f);
        setParam (proc, Params::listenerY, 2.0f);
        setParam (proc, Params::bypassDelay, 1.0f);

        const auto a = Geometry::compute (5.5, 1.8, 2.0);
        const auto buffer = impulseResponse (proc, sr, 1024);
        const auto l = findPeak (buffer, 0);
        const auto r = findPeak (buffer, 1);

        CHECK (l.index == r.index);
        CHECK (std::abs (l.value - 1.0f) < 0.02f);
        CHECK (std::abs (r.value - (float) a.gainR) < 0.02f);
    }

    if (failures == 0)
        std::printf ("audio_check: alles in Ordnung\n");

    return failures == 0 ? 0 : 1;
}
