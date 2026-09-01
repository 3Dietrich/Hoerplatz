// Pruefungen der reinen Geometrie - ohne JUCE, ohne Audio. Gibt bei einem
// Fehlschlag die Zeile aus und endet mit Rueckgabewert 1.
#include "../Source/Geometry.h"

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

    bool near (double a, double b, double tol = 1e-9) { return std::fabs (a - b) <= tol; }

    // Aufstellung der Vorgabe: 5,50 m auseinander, 35 cm vor der Wand.
    constexpr Geometry::Point boxL { -2.75, 0.35 };
    constexpr Geometry::Point boxR {  2.75, 0.35 };
}

#define CHECK(cond) check ((cond), #cond, __LINE__)

int main()
{
    // Mittig vor den Boxen: nichts zu tun.
    {
        const auto a = Geometry::compute (boxL, boxR, { 0.0, 3.35 });
        CHECK (near (a.distL, a.distR, 1e-12));
        CHECK (near (a.delayL, 0.0) && near (a.delayR, 0.0));
        CHECK (near (a.gainL, 1.0) && near (a.gainR, 1.0));
        CHECK (near (a.headAngleDeg, 0.0, 1e-9));
        CHECK (a.centred());
    }

    // Nach links gerueckt: die linke Box ist naeher, wird also verzoegert
    // und abgesenkt; die weiter entfernte bleibt unangetastet.
    {
        const auto a = Geometry::compute (boxL, boxR, { -1.2, 3.35 });
        CHECK (a.distL < a.distR);
        CHECK (a.leftIsNearer() && ! a.centred());
        CHECK (a.delayL > 0.0 && near (a.delayR, 0.0));
        CHECK (a.gainL < 1.0 && near (a.gainR, 1.0));
        CHECK (near (a.delaySeconds(), a.delayL) && near (a.gainRatio(), a.gainL));
        // Die Mitte liegt von dort aus rechts, der Kopf dreht dorthin.
        CHECK (a.headAngleDeg > 0.0);
        // Pegel folgt 1/r: das Verhaeltnis der Pegel ist das der Abstaende.
        CHECK (near (a.gainL / a.gainR, a.distL / a.distR, 1e-12));
        // Ausgeglichen kommt beides gleichzeitig an.
        CHECK (near (a.distL / Geometry::speedOfSound + a.delayL,
                     a.distR / Geometry::speedOfSound + a.delayR, 1e-12));
    }

    // Spiegelbildlich rechts: gleiche Betraege, getauschte Seiten.
    {
        const auto l = Geometry::compute (boxL, boxR, { -2.0, 2.5 });
        const auto r = Geometry::compute (boxL, boxR, {  2.0, 2.5 });
        CHECK (near (l.distL, r.distR, 1e-12) && near (l.distR, r.distL, 1e-12));
        CHECK (near (l.delayL, r.delayR, 1e-12));
        CHECK (near (l.headAngleDeg, -r.headAngleDeg, 1e-9));
        CHECK (l.leftIsNearer() && ! r.leftIsNearer());
    }

    // Gleichseitiges Dreieck: Basisbreite 60 Grad.
    {
        const auto a = Geometry::compute (boxL, boxR, { 0.0, 0.35 + 5.5 * std::sqrt (3.0) / 2.0 });
        CHECK (near (a.baseAngleDeg, 60.0, 1e-9));
    }

    // Schiefe Aufstellung: eine Box steht weiter im Raum. Auf der
    // Mittelsenkrechten der Verbindungslinie bleibt es trotzdem symmetrisch.
    {
        const Geometry::Point l { -2.0, 0.30 };
        const Geometry::Point r {  2.4, 1.90 };
        const Geometry::Point centre { 0.5 * (l.x + r.x), 0.5 * (l.y + r.y) };

        // Senkrechte auf der Verbindung, vom Mittelpunkt aus in den Raum.
        const double dx = r.x - l.x, dy = r.y - l.y;
        const double len = std::sqrt (dx * dx + dy * dy);
        const Geometry::Point seat { centre.x - dy / len * 3.0, centre.y + dx / len * 3.0 };

        const auto a = Geometry::compute (l, r, seat);
        CHECK (near (a.distL, a.distR, 1e-9));
        CHECK (a.centred());
        CHECK (near (a.delayL, 0.0, 1e-12) && near (a.delayR, 0.0, 1e-12));
        CHECK (near (Geometry::speakerDistance (l, r), len, 1e-12));
    }

    // Anteil des Pegelausgleichs: 0 laesst die Pegel in Ruhe, 2 verdoppelt
    // die Absenkung in Dezibel. Die Laufzeit bleibt davon unberuehrt.
    {
        const Geometry::Point seat { -1.2, 3.35 };
        const auto full = Geometry::compute (boxL, boxR, seat, 1.0);
        const auto none = Geometry::compute (boxL, boxR, seat, 0.0);
        const auto over = Geometry::compute (boxL, boxR, seat, 2.0);

        CHECK (near (none.gainL, 1.0) && near (none.gainR, 1.0));
        CHECK (near (none.delayL, full.delayL) && near (none.delayR, full.delayR));
        CHECK (over.gainL < full.gainL);

        const double dbFull = 20.0 * std::log10 (full.gainL);
        const double dbOver = 20.0 * std::log10 (over.gainL);
        CHECK (near (dbOver, 2.0 * dbFull, 1e-9));
    }

    // Die Prozentskala des Reglers: 100 % ist der im Raum gehoerte
    // Normalfall und liegt in der Mitte, 141 % die Rechnung fuers Freie,
    // die Enden bleiben bei 0 und dem doppelten Dezibelwert.
    {
        CHECK (near (Geometry::gainExponent (0.0), 0.0));
        CHECK (near (Geometry::gainExponent (100.0), Geometry::roomExponent));
        CHECK (near (Geometry::gainExponent (200.0), Geometry::maxExponent));
        CHECK (std::fabs (Geometry::gainExponent (141.0) - 1.0) < 0.01);
        // Monoton und ohne Sprung an der Nahtstelle.
        CHECK (Geometry::gainExponent (99.9) < Geometry::gainExponent (100.1));
        CHECK (std::fabs (Geometry::gainExponent (99.9) - Geometry::gainExponent (100.1)) < 0.01);
        // Halber Weg unter 100 % ist auch halbe Korrektur.
        CHECK (near (Geometry::gainExponent (50.0), 0.5 * Geometry::roomExponent));
    }

    // Der Laufzeitunterschied kann den Abstand der Boxen nie ueberschreiten
    // (Dreiecksungleichung) - davon haengt die Groesse des Delaypuffers ab.
    {
        const Geometry::Point l { -6.0, 0.5 };
        const Geometry::Point r {  6.0, 2.0 };
        const double maxDelay = Geometry::speakerDistance (l, r) / Geometry::speedOfSound;

        for (double x = -12.0; x <= 12.0; x += 0.37)
            for (double y = -1.0; y <= 20.0; y += 0.53)
            {
                const auto a = Geometry::compute (l, r, { x, y });
                CHECK (std::fabs (a.delayL - a.delayR) <= maxDelay + 1e-9);
                CHECK (a.gainL <= 1.0 + 1e-12 && a.gainR <= 1.0 + 1e-12);
                CHECK (a.delayL >= 0.0 && a.delayR >= 0.0);
            }
    }

    if (failures == 0)
        std::printf ("geometry_check: alles in Ordnung\n");

    return failures == 0 ? 0 : 1;
}
