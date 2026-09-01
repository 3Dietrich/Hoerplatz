#include "TestTone.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double pulsePeriod = 0.55;   // Abstand der Impulse
    constexpr double pulseAttack = 0.020;  // Anstieg
    constexpr double pulseDecay  = 0.090;  // Abfall, danach Ruhe
    constexpr float  pulseLevel  = 0.09f;  // rund -21 dBFS in der Spitze

    constexpr double tailOpen = 0.8;       // Zeit, in der sich das Bild oeffnet
    constexpr double tailMax  = 3.2;       // laenger als der laengste Ton

    // Schritt in Richtung Ziel, ohne darueber hinauszuschiessen.
    float towards (float distance, float maxStep)
    {
        return std::max (-maxStep, std::min (maxStep, distance));
    }

    float onePole (float in, float& state, float coeff)
    {
        state += coeff * (in - state);
        return state;
    }
}

void TestTone::prepare (double sampleRate)
{
    sr = sampleRate;

    // Grenzfrequenzen als einfache Einpol-Koeffizienten.
    const auto coeff = [this] (double hz)
    {
        return (float) (1.0 - std::exp (-2.0 * M_PI * hz / sr));
    };
    highpassCoeff = coeff (150.0);
    lowpassCoeff  = coeff (7000.0);

    // Ein A-Dur-Akkord ueber drei Oktaven. Tiefe Toene klingen laenger als
    // hohe und stehen naeher an der Mitte - so faechert der Ausklang von
    // unten nach oben auf, statt gleichmaessig auseinanderzulaufen.
    const double freqs[]  = { 110.00, 164.81, 220.00, 277.18, 329.63, 440.00 };
    const double taus[]   = {   3.00,   2.60,   2.30,   2.00,   1.80,   1.55 };
    const double pans[]   = {   0.00,  -0.55,   0.50,  -0.88,   0.80,  -0.40 };

    for (size_t i = 0; i < partials.size(); ++i)
    {
        partials[i].freq   = freqs[i];
        // Beide Seiten laufen leicht gegeneinander - daraus entsteht die
        // Breite. Der Versatz steigt mit der Lage, unten bleibt es ruhig.
        partials[i].detune = 0.60 + 0.15 * (double) i;
        partials[i].amp    = 0.30 / (1.0 + 0.5 * (double) i);
        partials[i].tau    = taus[i] / 6.9;              // aus der Zeit bis -60 dB
        partials[i].pan    = pans[i];
    }

    reset();
}

void TestTone::reset()
{
    sounding = false;
    decaying = false;
    mix = 0.0f;
    pulsePhase = 0.0;
    decayTime = 0.0;
    noiseFade = 0.0f;
    b0 = b1 = b2 = 0.0f;
    highpassState = lowpassState = 0.0f;
    tailLpL = tailLpR = 0.0f;

    for (auto& p : partials)
    {
        p.phaseL = 0.0;
        p.phaseR = 0.0;
    }
}

void TestTone::setActive (bool shouldBeActive)
{
    active.store (shouldBeActive);
}

void TestTone::startDecay()
{
    decaying = true;
    decayTime = 0.0;

    // Die Toene setzen mit zufaelliger Phase ein, damit sich ihre
    // Nulldurchgaenge nicht zu einem Knack addieren.
    for (auto& p : partials)
    {
        randomState = randomState * 1664525u + 1013904223u;
        p.phaseL = (double) (randomState >> 8) / 16777216.0 * 2.0 * M_PI;
        p.phaseR = p.phaseL;
    }
}

float TestTone::pinkNoise()
{
    randomState = randomState * 1664525u + 1013904223u;
    const float white = (float) ((int) (randomState >> 8) - 8388608) / 8388608.0f;

    b0 = 0.99765f * b0 + white * 0.0990460f;
    b1 = 0.96300f * b1 + white * 0.2965164f;
    b2 = 0.57000f * b2 + white * 1.0526913f;
    const float pink = (b0 + b1 + b2 + white * 0.1848f) * 0.18f;

    // Hochpass als Differenz zum Tiefpassanteil, danach der Tiefpass.
    const float lows = onePole (pink, highpassState, highpassCoeff);
    return onePole (pink - lows, lowpassState, lowpassCoeff);
}

void TestTone::render (float* left, float* right, int numSamples, float* mixOut)
{
    const bool wantsActive = active.load();

    if (wantsActive && ! sounding)
    {
        // Frischer Start: Impulse von vorne.
        sounding = true;
        decaying = false;
        pulsePhase = pulsePeriod;   // gleich der erste Impuls
        noiseFade = 0.0f;
    }
    else if (wantsActive && decaying)
    {
        // Wieder eingeschaltet, waehrend es noch ausklang.
        decaying = false;
        pulsePhase = pulsePeriod;
    }
    else if (! wantsActive && sounding && ! decaying)
    {
        startDecay();
    }

    // Ist alles verklungen und der Anteil bei null, gibt es nichts zu tun.
    // Steht er noch nicht bei null, laeuft unten die Rampe zu Ende, damit
    // die Musik ohne Ruck zurueckkommt.
    if (! sounding && mix <= 0.0f)
    {
        for (int i = 0; i < numSamples; ++i)
            mixOut[i] = 0.0f;
        return;
    }

    const double dt = 1.0 / sr;
    const float noiseTarget = (! decaying) ? 1.0f : 0.0f;
    const float fadeStep = (float) (dt / 0.08);   // 80 ms fuer den Wechsel

    for (int i = 0; i < numSamples; ++i)
    {
        float sampleL = 0.0f;
        float sampleR = 0.0f;

        noiseFade += towards (noiseTarget - noiseFade, fadeStep);

        // Impulse, solange nicht ausgeklungen wird.
        if (noiseFade > 0.0001f)
        {
            pulsePhase += dt;
            if (pulsePhase >= pulsePeriod)
                pulsePhase -= pulsePeriod;

            float env = 0.0f;
            if (pulsePhase < pulseAttack)
                env = 0.5f - 0.5f * (float) std::cos (M_PI * pulsePhase / pulseAttack);
            else
                env = (float) std::exp (-(pulsePhase - pulseAttack) / pulseDecay);

            const float n = pinkNoise() * env * pulseLevel * noiseFade;
            sampleL += n;
            sampleR += n;
        }

        // Ausklang: die Toene wandern beim Verklingen nach aussen.
        if (decaying)
        {
            decayTime += dt;

            // Oeffnung von der Mitte nach aussen, weich anlaufend.
            const double spread = decayTime >= tailOpen
                                ? 1.0
                                : 0.5 - 0.5 * std::cos (M_PI * decayTime / tailOpen);

            float tailL = 0.0f;
            float tailR = 0.0f;

            for (auto& p : partials)
            {
                const double env = std::exp (-decayTime / p.tau);
                if (env < 1.0e-5)
                    continue;

                p.phaseL += 2.0 * M_PI * p.freq / sr;
                p.phaseR += 2.0 * M_PI * (p.freq + p.detune) / sr;
                if (p.phaseL > 2.0 * M_PI) p.phaseL -= 2.0 * M_PI;
                if (p.phaseR > 2.0 * M_PI) p.phaseR -= 2.0 * M_PI;

                // Gleiche Leistung ueber das Panorama, damit die Lautstaerke
                // beim Wandern nicht schwankt.
                const double pan = p.pan * spread;
                const double angle = (pan + 1.0) * M_PI * 0.25;
                const double gainL = std::cos (angle);
                const double gainR = std::sin (angle);

                const double amp = p.amp * env;
                tailL += (float) (std::sin (p.phaseL) * amp * gainL);
                tailR += (float) (std::sin (p.phaseR) * amp * gainR);
            }

            // Der Tiefpass macht beim Verklingen langsam zu.
            const double cutoff = 6000.0 * std::exp (-decayTime * 0.55) + 700.0;
            const float c = (float) (1.0 - std::exp (-2.0 * M_PI * cutoff / sr));
            // Der Ausklang bleibt auf der Lautstaerke der Impulse - er soll
            // das Ohr entlassen, nicht erschrecken.
            sampleL += onePole (tailL, tailLpL, c) * 0.14f;
            sampleR += onePole (tailR, tailLpR, c) * 0.14f;

            if (decayTime > tailMax)
            {
                sounding = false;
                decaying = false;
            }
        }

        left[i] = sampleL;
        right[i] = sampleR;

        // Solange etwas klingt, gehoert der Ausgang dem Testgeraeusch. Am
        // Ende laeuft der Anteil in einem Zehntel einer Sekunde zurueck, so
        // kommt die Musik ohne Ruck wieder.
        const float mixTarget = sounding ? 1.0f : 0.0f;
        mix += towards (mixTarget - mix, (float) (dt / 0.10));
        mixOut[i] = mix;
    }
}
