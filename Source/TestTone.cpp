#include "TestTone.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double pulsePeriod = 0.40;    // Abstand der Schlaege
    constexpr float  pulseLevel  = 0.64f;   // Spitze rund -7 dBFS

    constexpr double widenTime = 0.40;      // Anschwellen der Auslenkung:
                                            // der erste Tropfen steht noch
                                            // in der Mitte, die folgenden
                                            // gehen immer weiter hinaus
    constexpr double fadeTau   = 1.05;      // Abklingen beim Verduennen
    constexpr double fadeMax   = 3.20;      // danach ist Ruhe

    // Wieviel eigenen Schlag die Seiten am Ende hoechstens bekommen. Ginge
    // es bis eins, waere von der Ortung nichts mehr uebrig: der gemeinsame,
    // im Bild verteilte Anteil verschwaende gerade dann, wenn es am
    // breitesten sein soll. Der eigene Anteil fuellt nur die Zwischenraeume.
    constexpr double maxSides = 0.35;

    constexpr double baseDelayMs = 1.6;     // Sockel der Verzoegerungsleitung
    constexpr double maxItdMs    = 1.1;     // Versatz ganz aussen - mehr als
                                            // zwischen zwei echten Ohren, das
                                            // traegt im Kopfhoerer bis an den
                                            // Rand und darueber hinaus

    // Ausschwingen am Ende des Verduennens, als Vielfaches des Betriebs:
    // laenger und weicher, aber kurz genug, dass die Tropfen einzeln stehen
    // bleiben.
    constexpr double tailStretch = 1.9;

    // Der Schlag. Die Frequenzen liegen dort, wo sich am schaerfsten orten
    // laesst: unten traegt der Laufzeitunterschied, oben die Flanke und der
    // Pegelunterschied. Die kurzen Zeitkonstanten der oberen Moden machen
    // daraus ein Klopfen und keinen Ton - schon nach wenigen
    // Hundertstelsekunden ist der Schlag vorbei, und was der Raum daraus
    // macht, kommt danach und nicht mittendrin.
    struct ModeSpec { double hz, tau; float amp; };

    constexpr ModeSpec modeSpecs[]
    {
        {  620.0, 0.030, 0.50f },
        { 1370.0, 0.018, 1.00f },
        { 2530.0, 0.010, 0.75f },
        { 4300.0, 0.005, 0.45f }
    };

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

void TestTone::Click::prepare (double sampleRate, double detune)
{
    sr = sampleRate;

    float sum = 0.0f;
    for (int i = 0; i < 4; ++i)
    {
        auto& m = modes[(size_t) i];
        const double w = 2.0 * M_PI * modeSpecs[i].hz * detune / sr;

        m.cosw = std::cos (w);
        m.sinw = std::sin (w);
        m.tau  = modeSpecs[i].tau;
        m.amp  = modeSpecs[i].amp;
        sum += m.amp;
    }

    norm = sum > 0.0f ? 1.0f / sum : 1.0f;
    reset();
}

void TestTone::Click::reset()
{
    for (auto& m : modes)
    {
        m.re = m.im = 0.0;
        m.decay = 0.0;
    }
}

void TestTone::Click::hit (double stretch)
{
    for (auto& m : modes)
    {
        // Der Zeiger startet auf der reellen Achse: die Ausgabe faengt bei
        // null an und steigt in einer Viertelperiode auf ihr Maximum. Bei
        // 1370 Hz sind das keine zwei Zehntelmillisekunden - die Flanke
        // steht damit praktisch senkrecht, ohne dass ein Sprung entsteht.
        m.re = 1.0;
        m.im = 0.0;
        m.decay = std::exp (-1.0 / (std::max (1.0e-6, m.tau * stretch) * sr));
    }
}

float TestTone::Click::next()
{
    double out = 0.0;

    for (auto& m : modes)
    {
        const double re = (m.re * m.cosw - m.im * m.sinw) * m.decay;
        const double im = (m.re * m.sinw + m.im * m.cosw) * m.decay;
        m.re = re;
        m.im = im;
        out += m.amp * im;
    }

    return (float) out * norm;
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

    // Der gemeinsame Schlag steht in der Mitte, die beiden anderen sind
    // gegeneinander verstimmt - beim Verduennen gehen die Seiten dadurch
    // auseinander, statt nur lauter und leiser zu werden.
    common.prepare (sr, 1.0);
    leftOnly.prepare (sr, 0.94);
    rightOnly.prepare (sr, 1.07);

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
        // ausklang: die Schlaege setzen sofort wieder ein.
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
            // die Schlaege werden laenger, der Klang dunkler, das Ganze
            // leiser.
            double width = 0.0;
            double stretch = 1.0;
            double level = 1.0;
            double cutoff = 12000.0;

            if (fading)
            {
                fadeTime += dt;
                const double t = std::min (1.0, fadeTime / widenTime);
                width   = 0.5 - 0.5 * std::cos (M_PI * t);          // weich von 0 auf 1
                stretch = 1.0 + (tailStretch - 1.0) * t;
                level   = std::exp (-fadeTime / fadeTau);
                cutoff  = 900.0 + 11100.0 * std::exp (-fadeTime * 1.1);

                if (fadeTime > fadeMax)
                {
                    sounding = false;
                    fading = false;
                }
            }

            // Beim Verduennen ruecken die Schlaege zusammen. Sie bleiben aber
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

                common.hit (stretch);
                leftOnly.hit (stretch);
                rightOnly.hit (stretch);
            }

            // Von der Mitte in die Breite: der gemeinsame Anteil weicht
            // zwei eigenen. Ueber Sinus und Kosinus gemischt, damit die
            // Lautstaerke dabei gleich bleibt.
            const double angle = width * std::asin (maxSides);
            const float centre = (float) std::cos (angle);
            const float sides  = (float) std::sin (angle);

            sharedDelay.push (common.next());

            // Der Ort des Schlages: Laufzeit zuerst, Pegel dazu. Die
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

            // Dichtere und laengere Schlaege tragen mehr Energie. Ohne
            // Ausgleich wuerde das Verduennen lauter beginnen als der
            // Betrieb - der Faktor haelt die mittlere Leistung gleich, den
            // Rueckgang besorgt allein das Abklingen.
            const double density = std::sqrt ((period / pulsePeriod) / stretch);
            const float gain = pulseLevel * (float) (level * density);
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
