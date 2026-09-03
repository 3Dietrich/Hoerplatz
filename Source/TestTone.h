#pragma once

#include <atomic>
#include <vector>

// Testgeraeusch zum Einrichten - und sein Ausklang.
//
// Waehrend es laeuft: ein kurzes, trockenes Klopfen, auf beiden Kanaelen
// identisch. Ein Impuls regt vier gedaempfte Schwingungen an, die zusammen
// ein Tock ergeben - Flanke praktisch senkrecht, dahinter ein paar
// Hundertstelsekunden Ausschwingen. Zwei Dinge machen es zum Werkzeug fuer
// die Mitte: die steile Flanke, an der ueberhaupt erst ein Laufzeit-
// unterschied hoerbar wird, und dass jeder Schlag derselbe ist. Rauschen
// klingt bei jedem Impuls anders; das Ohr beurteilt dann jedesmal ein neues
// Klangbild, statt sich auf einen stehenden Punkt einzuhoeren. Zwischen den
// Schlaegen ist Ruhe, damit man es lange nebenher laufen lassen kann.
//
// Beim Ausschalten hoert dasselbe Geraeusch nicht auf, sondern verduennt
// sich: die Schlaege gehen abwechselnd nach links und rechts, aus der Mitte
// heraus immer weiter hinaus - mit Laufzeit- und Pegelunterschied, wie es
// einem wirklich seitlich stehenden Geraeusch entspricht. Dazu bekommen die
// Kanaele zunehmend ein eigenes, leicht verstimmtes Klopfen statt des
// gemeinsamen, die Schlaege werden laenger und weicher, ein Tiefpass macht
// zu. Aus dem Punkt in der Mitte wird eine breite Wolke, die verschwindet.
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
    // Ein Schlag: vier gedaempfte Schwingungen, mit einem Impuls angeregt.
    // Sie liegen ueber dem Bereich, in dem sich am schaerfsten orten laesst,
    // und sind gegeneinander unharmonisch - so wird ein Klopfen daraus und
    // kein Ton. Drei davon: eines gemeinsam fuer die Mitte, je eines pro
    // Seite fuers Verduennen, die beiden letzten leicht verstimmt, damit die
    // Seiten dabei auseinandergehen.
    struct Click
    {
        void prepare (double sampleRate, double detune);
        void reset();

        // Neuer Schlag. stretch verlaengert das Ausschwingen - im Betrieb 1,
        // beim Verduennen mehr.
        void hit (double stretch);
        float next();

        struct Mode
        {
            // Zeiger, der sich pro Sample dreht und dabei kleiner wird: der
            // Realteil traegt die Drehung, der Imaginaerteil ist die
            // Ausgabe. Nach jedem Schlag faengt er neu an, deshalb sammelt
            // sich kein Rechenfehler an.
            double re = 0.0, im = 0.0;
            double cosw = 1.0, sinw = 0.0;
            double decay = 0.0;
            double tau = 0.0;
            float amp = 0.0f;
        };

        Mode modes[4];
        double sr = 44100.0;
        float norm = 1.0f;
    };

    double sr = 44100.0;
    std::atomic<bool> active { false };

    bool sounding = false;      // Geraeusch oder Ausklang laeuft
    bool fading = false;        // nur noch das Verduennen
    float mix = 0.0f;           // aktueller Anteil am Ausgang
    double pulsePhase = 0.0;    // Zeit seit dem letzten Schlag
    double fadeTime = 0.0;      // Zeit seit Beginn des Verduennens

    Click common, leftOnly, rightOnly;

    // Kurze Verzoegerungsleitung fuer den gemeinsamen Schlag. Beide Seiten
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

    // Ort des laufenden Schlages: -1 ganz links, +1 ganz rechts. Die
    // Tropfen gehen abwechselnd zur einen und zur anderen Seite, wie weit,
    // sagt die Auslenkung, die aus der Mitte heraus anschwillt.
    float pulseSide = 1.0f;
    float pulsePan = 0.0f;

    // Tiefpass ueber dem Verduennen, der dabei langsam zumacht.
    float tailLpL = 0.0f, tailLpR = 0.0f;
};
