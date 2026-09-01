#pragma once

#include <atomic>

// Testgeraeusch zum Einrichten - und sein Ausklang.
//
// Waehrend es laeuft: kurze, scharfe Impulse aus rosa Rauschen, auf beiden
// Kanaelen identisch. Die Flanke steht praktisch senkrecht - nur so laesst
// sich die Laufzeit ueberhaupt beurteilen: ein weicher Einsatz verschmiert
// alles unterhalb weniger Millisekunden, und genau darum geht es hier. Der
// kurze Nachhauch dahinter traegt das breite Band fuer den Pegel. Zwischen
// den Impulsen ist Ruhe, damit man es lange nebenher laufen lassen kann.
//
// Beim Ausschalten hoert dasselbe Geraeusch nicht auf, sondern verduennt
// sich: die beiden Kanaele bekommen zunehmend eigenes Rauschen statt des
// gemeinsamen, die Impulse werden immer laenger und weicher, ein Tiefpass
// macht zu - aus dem Punkt in der Mitte wird eine breite Wolke, die
// verschwindet.
//
// Das Signal wird vor der Laufzeit- und Pegelkorrektur eingespeist, laeuft
// also durch dieselbe Kette wie die Musik.
class TestTone
{
public:
    void prepare (double sampleRate);
    void reset();

    // Vom Bedienfeld aus geschaltet. Das Ausschalten beendet nichts abrupt,
    // es startet das Verduennen.
    void setActive (bool shouldBeActive);
    bool isActive() const { return active.load(); }

    // Solange hier true steht, gehoert der Ausgang dem Testgeraeusch.
    bool isSounding() const { return sounding; }

    // Schreibt das Geraeusch nach l/r und liefert zurueck, wie weit es das
    // anliegende Signal verdraengt (0 = nur Musik, 1 = nur Testgeraeusch).
    void render (float* left, float* right, int numSamples, float* mixOut);

private:
    // Eine Rauschquelle: weisses Rauschen, rosa gefaerbt, oben und unten
    // beschnitten. Drei davon - eine gemeinsame und je eine pro Seite -
    // ergeben den Uebergang von der Mitte in die Breite.
    struct Noise
    {
        void prepare (float highpass, float lowpass, unsigned int seed);
        void reset();
        float next();

        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
        float highState = 0.0f, lowState = 0.0f;
        float highCoeff = 0.02f, lowCoeff = 0.5f;
        unsigned int state = 1u;
    };

    double sr = 44100.0;
    std::atomic<bool> active { false };

    bool sounding = false;      // Geraeusch oder Ausklang laeuft
    bool fading = false;        // nur noch das Verduennen
    float mix = 0.0f;           // aktueller Anteil am Ausgang
    double pulsePhase = 0.0;    // Zeit seit dem letzten Impuls
    double fadeTime = 0.0;      // Zeit seit Beginn des Verduennens

    Noise common, leftOnly, rightOnly;

    // Tiefpass ueber dem Verduennen, der dabei langsam zumacht.
    float tailLpL = 0.0f, tailLpR = 0.0f;
};
