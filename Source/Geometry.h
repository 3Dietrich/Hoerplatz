#pragma once

#include <algorithm>
#include <cmath>

// Reine Rechnung, kein JUCE, kein Zustand: aus der Aufstellung (Boxenabstand)
// und dem Hoerplatz folgen die beiden Laufzeiten und die beiden Pegel, mit
// denen die Boxen am Hoerplatz wieder gleichzeitig und gleich laut ankommen.
//
// Weltkoordinaten (Draufsicht, Meter):
//   Ursprung = Mitte zwischen den beiden Boxen
//   +x = nach rechts,  +y = in den Raum hinein (von der Boxenebene weg)
//   Boxen liegen auf y = 0, der Hoerer bei y > 0.
//
// Die Waende bleiben aussen vor - gerechnet wird nur der Direktschall
// ("Waende rausgekuerzt"). Der Raum dient der Darstellung und begrenzt,
// wohin sich der Hoerplatz schieben laesst.
namespace Geometry
{
    // Schallgeschwindigkeit in Luft bei rund 20 Grad.
    inline constexpr double speedOfSound = 343.0;

    struct Alignment
    {
        double distL = 0.0;     // Abstand Hoerplatz - linke Box (m)
        double distR = 0.0;     // Abstand Hoerplatz - rechte Box (m)
        double delayL = 0.0;    // Ausgleichs-Laufzeit links (s), >= 0
        double delayR = 0.0;    // Ausgleichs-Laufzeit rechts (s), >= 0
        double gainL = 1.0;     // Ausgleichs-Pegel links (linear), <= 1
        double gainR = 1.0;     // Ausgleichs-Pegel rechts (linear), <= 1
        double baseAngleDeg = 0.0;  // Oeffnungswinkel Box-Hoerer-Box (Grad), Ideal 60
        double headAngleDeg = 0.0;  // Blickrichtung gegen "geradeaus", + = nach rechts gedreht
    };

    // speakerDistance = Abstand der beiden Boxen (m), listenerX/Y = Hoerplatz.
    inline Alignment compute (double speakerDistance, double listenerX, double listenerY)
    {
        Alignment a;

        const double half = 0.5 * std::max (0.01, speakerDistance);

        // Vom Hoerplatz zur jeweiligen Box. Der Mindestabstand haelt die
        // Rechnung heil, wenn jemand den Hoerplatz genau in eine Box schiebt.
        const double lx = -half - listenerX;
        const double rx =  half - listenerX;
        const double dy = -listenerY;

        a.distL = std::max (0.05, std::sqrt (lx * lx + dy * dy));
        a.distR = std::max (0.05, std::sqrt (rx * rx + dy * dy));

        // Laufzeit: die naehere Box wird so lange aufgehalten, bis die
        // weiter entfernte eingeholt hat. Verzoegert wird also immer nur,
        // vorziehen kann man Schall nicht.
        const double far = std::max (a.distL, a.distR);
        a.delayL = (far - a.distL) / speedOfSound;
        a.delayR = (far - a.distR) / speedOfSound;

        // Pegel: der Schalldruck faellt mit 1/r. Damit beide Boxen am
        // Hoerplatz gleich laut ankommen, wird die naehere abgesenkt - im
        // Verhaeltnis der Abstaende. Angehoben wird nie, der lauteste Kanal
        // bleibt bei 1.0, so kann nichts uebersteuern.
        a.gainL = a.distL / far;
        a.gainR = a.distR / far;

        // Richtungen zu den Boxen, normiert.
        const double nlx = lx / a.distL, nly = dy / a.distL;
        const double nrx = rx / a.distR, nry = dy / a.distR;

        // Oeffnungswinkel zwischen beiden Richtungen (Stereobasis, wie sie
        // der Hoerer an diesem Platz sieht).
        const double dot = std::clamp (nlx * nrx + nly * nry, -1.0, 1.0);
        a.baseAngleDeg = std::acos (dot) * 180.0 / M_PI;

        // Beste Mittenstellung = Winkelhalbierende der beiden Richtungen.
        // Sie ist die Blickrichtung, in der die Phantommitte genau vorne
        // sitzt. Bezug ist "geradeaus", also die Richtung -y (zur Boxenebene).
        const double bx = nlx + nrx;
        const double by = nly + nry;
        a.headAngleDeg = (bx == 0.0 && by == 0.0)
                       ? 0.0
                       : std::atan2 (bx, -by) * 180.0 / M_PI;

        return a;
    }
}
