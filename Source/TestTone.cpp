#include "TestTone.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double pulsePeriod = 0.40;    // Abstand der Impulse
    constexpr double pulseAttack = 0.00015; // Anstieg: praktisch senkrecht
    constexpr double pulseDecay  = 0.030;   // Abfall im Betrieb - ein kurzes Tack
    constexpr float  pulseLevel  = 0.45f;   // Spitze rund -7 dBFS

    constexpr double widenTime = 0.35;      // Zeit, in der sich das Bild oeffnet
    constexpr double fadeTau   = 1.05;      // Abklingen beim Verduennen
    constexpr double fadeMax   = 3.20;      // danach ist Ruhe

    constexpr double tailDecay = 0.35;      // Abfall am Ende des Verduennens

    float towards (float distance, float maxStep)
    {
        return std::max (-maxStep, std::min (maxStep, distance));
    }

    float onePole (float in, float& state, float coeff)
    {
        state += coeff * (in - state);
        return state;
    }

    float coeffFor (double hz, double sr)
    {
        return (float) (1.0 - std::exp (-2.0 * M_PI * hz / sr));
    }
}

void TestTone::Noise::prepare (float highpass, float lowpass, unsigned int seed)
{
    highCoeff = highpass;
    lowCoeff = lowpass;
    state = seed;
    reset();
}

void TestTone::Noise::reset()
{
    b0 = b1 = b2 = 0.0f;
    highState = lowState = 0.0f;
}

float TestTone::Noise::next()
{
    state = state * 1664525u + 1013904223u;
    const float white = (float) ((int) (state >> 8) - 8388608) / 8388608.0f;

    // Rosa Faerbung nach Paul Kellett.
    b0 = 0.99765f * b0 + white * 0.0990460f;
    b1 = 0.96300f * b1 + white * 0.2965164f;
    b2 = 0.57000f * b2 + white * 1.0526913f;
    const float pink = (b0 + b1 + b2 + white * 0.1848f) * 0.18f;

    // Hochpass als Differenz zum Tiefpassanteil, danach der Tiefpass. Was
    // ganz unten und ganz oben liegt, hilft beim Orten nicht und macht das
    // Geraeusch nur anstrengender.
    const float lows = onePole (pink, highState, highCoeff);
    return onePole (pink - lows, lowState, lowCoeff);
}

void TestTone::prepare (double sampleRate)
{
    sr = sampleRate;

    const float hp = coeffFor (120.0, sr);
    const float lp = coeffFor (12000.0, sr);

    // Drei unabhaengige Quellen: eine gemeinsame fuer die Mitte und je eine
    // pro Seite. Ihre Mischung bestimmt, wie breit das Geraeusch steht.
    common.prepare (hp, lp, 0x13579bdfu);
    leftOnly.prepare (hp, lp, 0x2468ace0u);
    rightOnly.prepare (hp, lp, 0x0f1e2d3cu);

    reset();
}

void TestTone::reset()
{
    sounding = false;
    fading = false;
    mix = 0.0f;
    pulsePhase = 0.0;
    fadeTime = 0.0;
    tailLpL = tailLpR = 0.0f;

    common.reset();
    leftOnly.reset();
    rightOnly.reset();
}

void TestTone::setActive (bool shouldBeActive)
{
    active.store (shouldBeActive);
}

void TestTone::render (float* left, float* right, int numSamples, float* mixOut)
{
    const bool wantsActive = active.load();

    if (wantsActive && (! sounding || fading))
    {
        // Frischer Start, oder wieder eingeschaltet, waehrend es noch
        // ausklang: die Impulse setzen sofort wieder ein.
        sounding = true;
        fading = false;
        pulsePhase = pulsePeriod;
        fadeTime = 0.0;
    }
    else if (! wantsActive && sounding && ! fading)
    {
        fading = true;
        fadeTime = 0.0;
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

    for (int i = 0; i < numSamples; ++i)
    {
        float sampleL = 0.0f;
        float sampleR = 0.0f;

        if (sounding)
        {
            // Beim Verduennen wandert alles gleichzeitig: das Bild geht auf,
            // die Impulse werden laenger, der Klang dunkler, das Ganze leiser.
            double width = 0.0;
            double decay = pulseDecay;
            double level = 1.0;
            double cutoff = 12000.0;

            if (fading)
            {
                fadeTime += dt;
                const double t = std::min (1.0, fadeTime / widenTime);
                width  = 0.5 - 0.5 * std::cos (M_PI * t);           // weich von 0 auf 1
                decay  = pulseDecay + (tailDecay - pulseDecay) * t;
                level  = std::exp (-fadeTime / fadeTau);
                cutoff = 900.0 + 11100.0 * std::exp (-fadeTime * 1.1);

                if (fadeTime > fadeMax)
                {
                    sounding = false;
                    fading = false;
                }
            }

            // Beim Verduennen ruecken die Impulse zusammen und laufen mit
            // ihrem laengeren Abfall ineinander - aus einzelnen Schlaegen
            // wird eine Wolke.
            const double period = fading ? pulsePeriod - (pulsePeriod - 0.20)
                                           * std::min (1.0, fadeTime / widenTime)
                                         : pulsePeriod;
            pulsePhase += dt;
            if (pulsePhase >= period)
                pulsePhase -= period;

            // Senkrechte Flanke, dann exponentiell zurueck. Die Flanke ist
            // das eigentliche Werkzeug: an ihr haengt, ob man einen
            // Laufzeitunterschied ueberhaupt hoeren kann.
            const float env = pulsePhase < pulseAttack
                            ? (float) (pulsePhase / pulseAttack)
                            : (float) std::exp (-(pulsePhase - pulseAttack) / decay);

            // Von der Mitte in die Breite: der gemeinsame Anteil weicht
            // zwei eigenen. Ueber Sinus und Kosinus gemischt, damit die
            // Lautstaerke dabei gleich bleibt.
            const double angle = width * M_PI * 0.5;
            const float centre = (float) std::cos (angle);
            const float sides  = (float) std::sin (angle);

            const float shared = common.next();
            const float ownL = leftOnly.next();
            const float ownR = rightOnly.next();

            // Dichtere und laengere Impulse tragen mehr Energie. Ohne
            // Ausgleich wuerde das Verduennen lauter beginnen als der
            // Betrieb - der Faktor haelt die mittlere Leistung gleich, den
            // Rueckgang besorgt allein das Abklingen.
            const double density = std::sqrt ((period / pulsePeriod) * (pulseDecay / decay));
            const float gain = env * pulseLevel * (float) (level * density);
            sampleL = (shared * centre + ownL * sides) * gain;
            sampleR = (shared * centre + ownR * sides) * gain;

            if (fading)
            {
                const float c = coeffFor (cutoff, sr);
                sampleL = onePole (sampleL, tailLpL, c);
                sampleR = onePole (sampleR, tailLpR, c);
            }
        }

        left[i] = sampleL;
        right[i] = sampleR;

        // Solange etwas klingt, gehoert der Ausgang dem Testgeraeusch. Am
        // Ende laeuft der Anteil in einem Zehntel einer Sekunde zurueck.
        const float mixTarget = sounding ? 1.0f : 0.0f;
        mix += towards (mixTarget - mix, (float) (dt / 0.10));
        mixOut[i] = mix;
    }
}
