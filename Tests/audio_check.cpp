// Nachweis am Signal: ein Impuls durch das Plugin geschickt, danach wird
// geprueft, wo er in beiden Kanaelen wieder herauskommt und wie laut. Die
// erwarteten Werte kommen aus der Geometrie, nicht aus dem Code, der sie
// erzeugt hat.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

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

    // Die Interpolation der Delayline verteilt einen Impuls auf mehrere
    // Samples; ihre Koeffizienten summieren sich zu eins. Der Pegel steckt
    // deshalb in der Summe, nicht in der hoechsten Einzelspitze.
    float sumOf (const juce::AudioBuffer<float>& b, int channel)
    {
        float sum = 0.0f;
        for (int i = 0; i < b.getNumSamples(); ++i)
            sum += b.getSample (channel, i);
        return sum;
    }

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
        setParam (proc, Params::leftX, -2.75f);
        setParam (proc, Params::rightX, 2.75f);
        setParam (proc, Params::gainAmount, 141.0f);
        setParam (proc, Params::listenerX, -2.1f);
        setParam (proc, Params::listenerY,  1.6f);

        const auto a = Geometry::compute ({ -2.75, 0.35 }, { 2.75, 0.35 }, { -2.1, 1.6 },
                                          Geometry::gainExponent (141.0));
        const auto buffer = impulseResponse (proc, sr, 2048);

        const auto l = findPeak (buffer, 0);
        const auto r = findPeak (buffer, 1);

        // Der weiter entfernte Kanal laeuft ohne Zusatzverzoegerung durch,
        // der naehere kommt genau um den Laufzeitunterschied spaeter.
        const int expectedShift = (int) std::lround ((a.delayL - a.delayR) * sr);
        CHECK (std::abs ((l.index - r.index) - expectedShift) <= 1);

        // Pegel: der naehere Kanal ist im Verhaeltnis der Abstaende leiser.
        const float sumL = sumOf (buffer, 0);
        const float sumR = sumOf (buffer, 1);
        CHECK (std::abs (sumR - (float) a.gainR) < 0.02f);
        CHECK (std::abs (sumL - (float) a.gainL) < 0.02f);
        // Die Korrektur nimmt der einen Seite, was sie der anderen gibt.
        CHECK (std::abs (sumL * sumR - 1.0f) < 0.03f);

        std::printf ("links: Index %d, Pegel %.3f (erwartet %.3f)\n", l.index, sumL, a.gainL);
        std::printf ("rechts: Index %d, Pegel %.3f\n", r.index, sumR);
    }

    // Beide Umgehungen an: das Signal geht unveraendert durch, beide
    // Kanaele liegen wieder aufeinander und stehen auf vollem Pegel.
    {
        HoerplatzProcessor proc;
        setParam (proc, Params::leftX, -2.75f);
        setParam (proc, Params::rightX, 2.75f);
        setParam (proc, Params::gainAmount, 141.0f);
        setParam (proc, Params::listenerX, -2.1f);
        setParam (proc, Params::listenerY,  1.6f);
        setParam (proc, Params::bypassDelay, 1.0f);
        setParam (proc, Params::bypassGain,  1.0f);

        const auto buffer = impulseResponse (proc, sr, 1024);
        const auto l = findPeak (buffer, 0);
        const auto r = findPeak (buffer, 1);

        CHECK (l.index == r.index);
        CHECK (std::abs (sumOf (buffer, 0) - 1.0f) < 0.02f);
        CHECK (std::abs (sumOf (buffer, 1) - 1.0f) < 0.02f);
    }

    // Nur die Laufzeit umgangen: die Pegel stehen weiter im Verhaeltnis der
    // Abstaende, die Impulse liegen aber wieder gleichauf.
    {
        HoerplatzProcessor proc;
        setParam (proc, Params::leftX, -2.75f);
        setParam (proc, Params::rightX, 2.75f);
        setParam (proc, Params::gainAmount, 141.0f);
        setParam (proc, Params::listenerX, 1.8f);
        setParam (proc, Params::listenerY, 2.0f);
        setParam (proc, Params::bypassDelay, 1.0f);

        const auto a = Geometry::compute ({ -2.75, 0.35 }, { 2.75, 0.35 }, { 1.8, 2.0 },
                                          Geometry::gainExponent (141.0));
        const auto buffer = impulseResponse (proc, sr, 1024);
        const auto l = findPeak (buffer, 0);
        const auto r = findPeak (buffer, 1);

        CHECK (l.index == r.index);
        CHECK (std::abs (sumOf (buffer, 0) - (float) a.gainL) < 0.02f);
        CHECK (std::abs (sumOf (buffer, 1) - (float) a.gainR) < 0.02f);
    }

    // Der Regler wirkt in Dezibel: 30 % ergeben genau drei Zehntel des
    // vollen 1/r-Wertes.
    {
        HoerplatzProcessor proc;
        setParam (proc, Params::leftX, -2.75f);
        setParam (proc, Params::rightX, 2.75f);
        setParam (proc, Params::listenerX, -2.1f);
        setParam (proc, Params::listenerY,  1.6f);
        setParam (proc, Params::gainAmount, 30.0f);

        const auto full = Geometry::compute ({ -2.75, 0.35 }, { 2.75, 0.35 }, { -2.1, 1.6 }, 1.0);
        const auto buffer = impulseResponse (proc, sr, 2048);

        const double dbFull = 20.0 * std::log10 (full.gainL);
        const double dbHeard = 20.0 * std::log10 (sumOf (buffer, 0));
        CHECK (std::abs (dbHeard - Geometry::roomExponent * dbFull) < 0.2);

        std::printf ("30 %%: %.2f dB links (bei 100 %% waeren es %.2f dB)\n", dbHeard, dbFull);
    }

    // Ein eingetippter Boxenabstand muss die Boxen wirklich verschieben.
    // Der Regler hat keinen eigenen Parameter, er greift an beide Boxen -
    // frueher verwarf er die Eingabe, weil beim Tippen der Tastaturfokus im
    // Textfeld liegt und nicht auf dem Regler selbst.
    {
        HoerplatzProcessor proc;
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());

        auto* slider = dynamic_cast<juce::Slider*> (editor->findChildWithID ("speakerDistance"));
        CHECK (slider != nullptr);

        if (slider != nullptr)
        {
            const double gewuenscht = 3.20;
            slider->setValue (gewuenscht, juce::sendNotificationSync);

            const Geometry::Point l { proc.apvts.getRawParameterValue (Params::leftX)->load(),
                                      proc.apvts.getRawParameterValue (Params::leftY)->load() };
            const Geometry::Point r { proc.apvts.getRawParameterValue (Params::rightX)->load(),
                                      proc.apvts.getRawParameterValue (Params::rightY)->load() };

            const double erreicht = Geometry::speakerDistance (l, r);
            CHECK (std::abs (erreicht - gewuenscht) < 0.02);
            std::printf ("Boxenabstand eingetippt: %.2f m -> %.2f m\n", gewuenscht, erreicht);

            // Und derselbe Weg ueber den Text, so wie er im Feld ankommt.
            CHECK (std::abs (slider->getValueFromText ("3,20 m") - 3.20) < 1e-9);
        }
    }

    // Testgeraeusch: waehrend es laeuft, steht es genau in der Mitte -
    // beide Kanaele sind gleich. Beim Ausschalten klingt es aus und geht
    // dabei ins Stereobild auseinander; nach gut drei Sekunden ist Ruhe.
    {
        HoerplatzProcessor proc;
        setParam (proc, Params::bypassDelay, 1.0f);   // die Korrektur soll
        setParam (proc, Params::bypassGain,  1.0f);   // hier nichts verschieben

        const int block = 512;
        proc.prepareToPlay (sr, block);
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;

        double rms = 0.0;
        double maxLeftDb = 0.0, maxRightDb = 0.0;   // groesste Auslenkung je Seite
        auto runFor = [&] (double seconds, double& peak, double& correlation)
        {
            double sumLL = 0.0, sumRR = 0.0, sumLR = 0.0;
            int counted = 0;
            peak = 0.0;
            maxLeftDb = maxRightDb = 0.0;

            for (int done = 0; done < (int) (seconds * sr); done += block)
            {
                buffer.clear();
                proc.processBlock (buffer, midi);

                // Balance des einzelnen Blocks: bei einem Geraeusch, das
                // ueber die Breite verstreut aufblitzt, schwankt sie stark;
                // bei blossem diffusem Rauschen bleibt sie nahe null.
                double blockLL = 0.0, blockRR = 0.0;
                for (int i = 0; i < block; ++i)
                {
                    blockLL += buffer.getSample (0, i) * buffer.getSample (0, i);
                    blockRR += buffer.getSample (1, i) * buffer.getSample (1, i);
                }
                if (blockLL > 1.0e-9 && blockRR > 1.0e-9)
                {
                    const double balance = 10.0 * std::log10 (blockLL / blockRR);
                    maxLeftDb  = std::max (maxLeftDb,  balance);
                    maxRightDb = std::max (maxRightDb, -balance);
                }

                for (int i = 0; i < block; ++i)
                {
                    const double l = buffer.getSample (0, i);
                    const double r = buffer.getSample (1, i);
                    peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
                    sumLL += l * l; sumRR += r * r; sumLR += l * r;
                    ++counted;
                }
            }

            rms = counted > 0 ? std::sqrt (0.5 * (sumLL + sumRR) / counted) : 0.0;

            const double denom = std::sqrt (sumLL * sumRR);
            correlation = denom > 1.0e-12 ? sumLR / denom : 1.0;
        };

        double peak = 0.0, corr = 0.0;

        // Aus: nichts zu hoeren.
        runFor (0.5, peak, corr);
        CHECK (peak < 1.0e-6);

        // An: hoerbar, und beide Kanaele tragen dasselbe.
        proc.testTone.setActive (true);
        runFor (1.5, peak, corr);
        CHECK (peak > 0.02 && peak < 0.8);
        CHECK (corr > 0.9999);
        CHECK (maxLeftDb < 0.01 && maxRightDb < 0.01);   // im Betrieb fest in der Mitte
        std::printf ("Testgeraeusch an: Spitze %.3f, Mittelwert %.1f dB, Gleichlauf %.4f, Balance bis %.2f dB\n",
                     peak, 20.0 * std::log10 (rms), corr, std::max (maxLeftDb, maxRightDb));

        // Aus: der Uebergang laeuft noch aus der Mitte heraus.
        proc.testTone.setActive (false);
        runFor (0.4, peak, corr);
        CHECK (proc.testTone.isSounding());
        CHECK (peak > 0.02);

        // Und danach steht die Fahne - beide Seiten weitgehend unabhaengig.
        runFor (1.2, peak, corr);
        CHECK (peak > 0.005);
        // Die Ortung steckt in der Balance, nicht im Gleichlauf: beide
        // Seiten tragen dasselbe Rauschen, nur zeitversetzt und verschieden
        // laut. Der Gleichlauf darf dabei ruhig hoch bleiben.
        CHECK (corr < 0.95);
        // Die Tropfen gehen nach beiden Seiten hinaus, nicht nur diffus in
        // die Mitte - und zwar auf jede Seite gleichermassen.
        CHECK (maxLeftDb > 6.0 && maxRightDb > 6.0);
        std::printf ("Ausklang: Spitze %.3f, Mittelwert %.1f dB, Gleichlauf %.4f, Balance bis %.1f dB links / %.1f dB rechts\n",
                     peak, 20.0 * std::log10 (rms), corr, maxLeftDb, maxRightDb);

        // Und danach ist Ruhe.
        runFor (2.5, peak, corr);
        CHECK (! proc.testTone.isSounding());
        runFor (0.5, peak, corr);
        CHECK (peak < 1.0e-6);
    }

    // Bypass Pegel: der Schalter muss am Signal etwas aendern. Nebenbei
    // zeigt die Ausgabe, wie klein der Unterschied bei kleinem Ausgleich
    // ist - dass man ihn dort kaum hoert, liegt nicht am Schalter.
    {
        const auto messen = [&] (float amount, bool bypass)
        {
            HoerplatzProcessor proc;
            setParam (proc, Params::listenerX, -2.1f);
            setParam (proc, Params::listenerY,  1.6f);
            setParam (proc, Params::gainAmount, amount);
            setParam (proc, Params::bypassDelay, 1.0f);
            setParam (proc, Params::bypassGain, bypass ? 1.0f : 0.0f);

            const auto buffer = impulseResponse (proc, sr, 1024);
            return 20.0 * std::log10 (sumOf (buffer, 0) / sumOf (buffer, 1));
        };

        const double voll = messen (100.0f, false);
        const double leise = messen (30.0f, false);
        const double aus = messen (100.0f, true);

        CHECK (std::abs (aus) < 0.05);          // mit Bypass stehen beide gleich
        CHECK (std::abs (voll) > 5.0);          // ohne Bypass deutlich getrennt
        CHECK (std::abs (leise) < std::abs (voll));

        std::printf ("Bypass Pegel: bei 100 %% %.1f dB Unterschied, bei 30 %% %.1f dB, mit Bypass %.2f dB\n",
                     voll, leise, aus);
    }

    // Zahleneingabe: Komma wie Punkt, Einheit egal.
    {
        CHECK (std::abs (Params::parseNumber ("3.2")      - 3.2) < 1e-9);
        CHECK (std::abs (Params::parseNumber ("3,2")      - 3.2) < 1e-9);
        CHECK (std::abs (Params::parseNumber ("3,20 m")   - 3.2) < 1e-9);
        CHECK (std::abs (Params::parseNumber (" 3.20 m ") - 3.2) < 1e-9);
        CHECK (std::abs (Params::parseNumber ("3,2 Meter")- 3.2) < 1e-9);
        CHECK (std::abs (Params::parseNumber ("-2,00 m")  + 2.0) < 1e-9);
        CHECK (std::abs (Params::parseNumber (juce::String::fromUTF8 ("−2,00 m")) + 2.0) < 1e-9);
        CHECK (std::abs (Params::parseNumber ("120 %")    - 120.0) < 1e-9);
    }

    if (failures == 0)
        std::printf ("audio_check: alles in Ordnung\n");

    return failures == 0 ? 0 : 1;
}
