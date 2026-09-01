#include "TestTone.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double pulsePeriod = 0.40;    // Abstand der Impulse
    constexpr double pulseAttack = 0.00015; // Anstieg: praktisch senkrecht
    constexpr double pulseDecay  = 0.030;   // Abfall im Betrieb - ein kurzes Tack
    constexpr float  pulseLevel  = 0.45f;   // Spitze rund -7 dBFS

    constexpr double widenTime = 0.40;      // Anschwellen der Auslenkung:
                                            // der erste Tropfen steht noch
                                            // in der Mitte, die folgenden
                                            // gehen immer weiter hinaus
    constexpr double fadeTau   = 1.05;      // Abklingen beim Verduennen
    constexpr double fadeMax   = 3.20;      // danach ist Ruhe

        constexpr double tailDecay = 0.055;     // Abfall am Ende des Verduennens:
                                            // kurz genug, dass die Tropfen
                                            // einzeln stehen bleiben

    // Wieviel eigenes Rauschen die Seiten am Ende hoechstens bekommen. Ginge
    // es bis eins, waere von der Ortung nichts mehr uebrig: der gemeinsame,
    // im Bild verteilte Anteil verschwaende gerade dann, wenn es am
    // breitesten sein soll - uebrig bliebe diffuses Rauschen, das im
    // Kopfhoerer im Kopf steht statt aussen. Der eigene Anteil fuellt nur
    // die Zwischenraeume.
    constexpr double maxSides = 0.35;

    constexpr double baseDelayMs = 1.6;     // Sockel der Verzoegerungsleitung
    constexpr double maxItdMs    = 1.1;     // Versatz ganz aussen - mehr als
                                            // zwischen zwei echten Ohren, das
                                            // traegt im Kopfhoerer bis an den
                                            // Rand und darueber hinaus

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

void TestTone::Ring::prepare (int size)
{
    data.assign ((size_t) std::max (4, size), 0.0f);
    reset();
}

void TestTone::Ring::reset()
{
    std::fill (data.begin(), data.end(), 0.0f);
    writePos = 0;
}

void TestTone::Ring::push (float v)
{
    data[(size_t) writePos] = v;
    if (++writePos >= (int) data.size())
        writePos = 0;
}

float TestTone::Ring::read (float delaySamples) const
{
    const int size = (int) data.size();
    const float clamped = std::max (0.0f, std::min ((float) (size - 2), delaySamples));

    const int whole = (int) clamped;
    const float frac = clamped - (float) whole;

    int index = writePos - 1 - whole;
    while (index < 0) index += size;
    int next = index - 1;
    while (next < 0) next += size;

    return data[(size_t) index] * (1.0f - frac) + data[(size_t) next] * frac;
}

void TestTone::prepare (double sampleRate)
{
    sr = sampleRate;

    baseDelaySamples = (float) (baseDelayMs * 0.001 * sr);
    maxItdSamples    = (float) (maxItdMs    * 0.001 * sr);
    sharedDelay.prepare ((int) (baseDelaySamples + maxItdSamples) + 8);

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
    pulsePan = 0.0f;
    sharedDelay.reset();

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
            // Beim Verduennen ruecken die Impulse zusammen. Sie bleiben aber
            // deutlich kuerzer als ihr Abstand - ueberlappen sie, mittelt
            // sich ihre Streuung wieder zur Mitte und alles klingt nur noch
            // diffus.
            const double period = fading ? pulsePeriod - (pulsePeriod - 0.13)
                                           * std::min (1.0, fadeTime / widenTime)
                                         : pulsePeriod;
            pulsePhase += dt;
            if (pulsePhase >= period)
            {
                pulsePhase -= period;

                // Beim Verduennen wechseln die Tropfen die Seite: links,
                // rechts, links - und dabei jedesmal weiter hinaus, solange
                // die Auslenkung anschwillt. Im Betrieb bleibt alles in der
                // Mitte, dort soll ja nichts wandern.
                if (fading)
                {
                    pulseSide = -pulseSide;
                    pulsePan = pulseSide;
                }
                else
                {
                    pulseSide = 1.0f;   // der erste Tropfen geht nach links
                    pulsePan = 0.0f;
                }
            }

            // Senkrechte Flanke, dann exponentiell zurueck. Die Flanke ist
            // das eigentliche Werkzeug: an ihr haengt, ob man einen
            // Laufzeitunterschied ueberhaupt hoeren kann.
            const float env = pulsePhase < pulseAttack
                            ? (float) (pulsePhase / pulseAttack)
                            : (float) std::exp (-(pulsePhase - pulseAttack) / decay);

            // Von der Mitte in die Breite: der gemeinsame Anteil weicht
            // zwei eigenen. Ueber Sinus und Kosinus gemischt, damit die
            // Lautstaerke dabei gleich bleibt.
            const double angle = width * std::asin (maxSides);
            const float centre = (float) std::cos (angle);
            const float sides  = (float) std::sin (angle);

            sharedDelay.push (common.next());

            // Der Ort des Impulses: Laufzeit zuerst, Pegel dazu. Die
            // Laufzeit traegt die Ortung - ein Pegelunterschied allein
            // klingt im Kopfhoerer nur diffus, nicht weit.
            const float pan = pulsePan * (float) width;
            const float itd = pan * maxItdSamples * 0.5f;
            const float sharedL = sharedDelay.read (baseDelaySamples + itd);
            const float sharedR = sharedDelay.read (baseDelaySamples - itd);

            const double panAngle = (pan + 1.0) * M_PI * 0.25;
            const float panL = (float) (std::cos (panAngle) * M_SQRT2);
            const float panR = (float) (std::sin (panAngle) * M_SQRT2);

            const float ownL = leftOnly.next();
            const float ownR = rightOnly.next();

            // Dichtere und laengere Impulse tragen mehr Energie. Ohne
            // Ausgleich wuerde das Verduennen lauter beginnen als der
            // Betrieb - der Faktor haelt die mittlere Leistung gleich, den
            // Rueckgang besorgt allein das Abklingen.
            const double density = std::sqrt ((period / pulsePeriod) * (pulseDecay / decay));
            const float gain = env * pulseLevel * (float) (level * density);
            sampleL = (sharedL * centre * panL + ownL * sides) * gain;
            sampleR = (sharedR * centre * panR + ownR * sides) * gain;

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
