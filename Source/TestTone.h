#pragma once

#include <array>
#include <atomic>

// Testgeraeusch zum Einrichten - und sein Ausklang.
//
// Waehrend es laeuft: weiche Impulse aus rosa Rauschen, auf beiden Kanaelen
// gleich. Stimmen Laufzeit und Pegel, stehen sie als ein Punkt zwischen den
// Boxen; stimmt etwas nicht, wandert oder verschmiert er. Die Impulse
// bringen beides mit, was man dafuer braucht: einen Einschwinger fuer die
// Laufzeit und ein breites Band fuer den Pegel. Zwischen den Impulsen ist
// Ruhe, damit man es lange nebenher laufen lassen kann.
//
// Beim Ausschalten hoert das Rauschen auf, und aus ihm treten Toene hervor,
// die ueber knapp drei Sekunden ausklingen. Dabei wandern sie aus der Mitte
// nach aussen und gehen leicht gegeneinander verstimmt, sodass sich das Bild
// beim Verklingen oeffnet.
//
// Das Signal wird vor der Laufzeit- und Pegelkorrektur eingespeist, laeuft
// also durch dieselbe Kette wie die Musik.
class TestTone
{
public:
    void prepare (double sampleRate);
    void reset();

    // Vom Bedienfeld aus geschaltet. Das Ausschalten beendet nichts abrupt,
    // es startet den Ausklang.
    void setActive (bool shouldBeActive);
    bool isActive() const { return active.load(); }

    // Solange hier true steht, gehoert der Ausgang dem Testgeraeusch.
    bool isSounding() const { return sounding; }

    // Schreibt das Geraeusch nach l/r und liefert zurueck, wie weit es das
    // anliegende Signal verdraengt (0 = nur Musik, 1 = nur Testgeraeusch).
    void render (float* left, float* right, int numSamples, float* mixOut);

private:
    float pinkNoise();
    void startDecay();

    struct Partial
    {
        double freq = 0.0;      // Grundfrequenz des Tons
        double detune = 0.0;    // Versatz der rechten Seite, erzeugt Schwebung
        double phaseL = 0.0, phaseR = 0.0;
        double amp = 0.0;       // Anteil im Klang
        double tau = 1.0;       // Abklingzeit in Sekunden
        double pan = 0.0;       // Ziel im Panorama, -1 links, +1 rechts
    };

    double sr = 44100.0;
    std::atomic<bool> active { false };

    bool sounding = false;      // Geraeusch oder Ausklang laeuft
    bool decaying = false;      // nur noch Ausklang
    float mix = 0.0f;           // aktueller Anteil am Ausgang
    double pulsePhase = 0.0;    // Zeit seit dem letzten Impuls
    double decayTime = 0.0;     // Zeit seit Beginn des Ausklangs
    float noiseFade = 0.0f;     // blendet das Rauschen beim Umschalten weg

    std::array<Partial, 6> partials;

    // Rosa Rauschen nach Paul Kellett, dazu ein Hoch- und ein Tiefpass, die
    // dem Rauschen die aeussersten Lagen nehmen - dort ortet das Gehoer
    // ohnehin schlecht, und leise bleibt es dadurch angenehmer.
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
    float highpassState = 0.0f, lowpassState = 0.0f;
    float lowpassCoeff = 0.5f, highpassCoeff = 0.02f;

    // Tiefpass ueber dem Ausklang, der beim Verklingen langsam zumacht.
    float tailLpL = 0.0f, tailLpR = 0.0f;

    unsigned int randomState = 0x1234567u;
};
