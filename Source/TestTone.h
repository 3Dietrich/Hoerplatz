#pragma once

#include <atomic>
#include <vector>

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
// sich: jeder Impuls blitzt an einer anderen Stelle auf, ueber die ganze
// Breite verstreut - mit Laufzeit- und Pegelunterschied, wie es einem
// wirklich seitlich stehenden Geraeusch entspricht. Dazu bekommen die
// Kanaele zunehmend eigenes Rauschen statt des gemeinsamen, die Impulse
// werden laenger und weicher, ein Tiefpass macht zu. Aus dem Punkt in der
// Mitte wird eine breite Wolke, die verschwindet.
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

    // Kurze Verzoegerungsleitung fuer das gemeinsame Rauschen. Beide Seiten
    // lesen daraus mit unterschiedlichem Versatz - daher kommt die Ortung
    // in der Breite; ein Pegelunterschied allein klingt im Kopfhoerer nur
    // diffus, nicht weit.
    struct Ring
    {
        void prepare (int size);
        void reset();
        void push (float v);
        float read (float delaySamples) const;

        std::vector<float> data;
        int writePos = 0;
    };
    Ring sharedDelay;

    float baseDelaySamples = 0.0f;   // gemeinsamer Sockel, damit beide Seiten Luft haben
    float maxItdSamples = 0.0f;      // groesster Versatz im Verduennen

    // Ort des laufenden Impulses: -1 ganz links, +1 ganz rechts.
    float pulsePan = 0.0f;
    unsigned int panState = 0x5eed1234u;

    // Tiefpass ueber dem Verduennen, der dabei langsam zumacht.
    float tailLpL = 0.0f, tailLpR = 0.0f;
};
