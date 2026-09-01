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
}

#define CHECK(cond) check ((cond), #cond, __LINE__)

int main()
{
    constexpr double spk = 5.5;

    // Mittig vor den Boxen: nichts zu tun.
    {
        const auto a = Geometry::compute (spk, 0.0, 3.0);
        CHECK (near (a.distL, a.distR, 1e-12));
        CHECK (near (a.delayL, 0.0) && near (a.delayR, 0.0));
        CHECK (near (a.gainL, 1.0) && near (a.gainR, 1.0));
        CHECK (near (a.headAngleDeg, 0.0, 1e-9));
    }

    // Nach links gerueckt: die linke Box ist naeher, wird also verzoegert
    // und abgesenkt; die weiter entfernte bleibt unangetastet.
    {
        const auto a = Geometry::compute (spk, -1.2, 3.0);
        CHECK (a.distL < a.distR);
        CHECK (a.delayL > 0.0 && near (a.delayR, 0.0));
        CHECK (a.gainL < 1.0 && near (a.gainR, 1.0));
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
        const auto l = Geometry::compute (spk, -2.0, 2.5);
        const auto r = Geometry::compute (spk,  2.0, 2.5);
        CHECK (near (l.distL, r.distR, 1e-12) && near (l.distR, r.distL, 1e-12));
        CHECK (near (l.delayL, r.delayR, 1e-12));
        CHECK (near (l.headAngleDeg, -r.headAngleDeg, 1e-9));
    }

    // Gleichseitiges Dreieck: Basisbreite 60 Grad.
    {
        const auto a = Geometry::compute (spk, 0.0, spk * std::sqrt (3.0) / 2.0);
        CHECK (near (a.baseAngleDeg, 60.0, 1e-9));
    }

    // Der Laufzeitunterschied kann den Boxenabstand nie ueberschreiten
    // (Dreiecksungleichung) - davon haengt die Groesse des Delaypuffers ab.
    {
        for (double x = -12.0; x <= 12.0; x += 0.37)
            for (double y = 0.1; y <= 20.0; y += 0.53)
            {
                const auto a = Geometry::compute (12.0, x, y);
                CHECK (std::fabs (a.delayL - a.delayR) <= 12.0 / Geometry::speedOfSound + 1e-9);
                CHECK (a.gainL <= 1.0 + 1e-12 && a.gainR <= 1.0 + 1e-12);
                CHECK (a.delayL >= 0.0 && a.delayR >= 0.0);
            }
    }

    if (failures == 0)
        std::printf ("geometry_check: alles in Ordnung\n");

    return failures == 0 ? 0 : 1;
}
