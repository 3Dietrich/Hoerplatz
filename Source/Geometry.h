#pragma once

#include <algorithm>
#include <cmath>

// Reine Rechnung, kein JUCE, kein Zustand: aus den Standorten der beiden
// Boxen und dem Hoerplatz folgen die beiden Laufzeiten und die beiden Pegel,
// mit denen die Boxen am Hoerplatz wieder gleichzeitig und gleich laut
// ankommen.
//
// Weltkoordinaten (Draufsicht, Meter):
//   Ursprung = Mitte der vorderen Wand
//   +x = nach rechts,  +y = in den Raum hinein
//
// Die Waende bleiben aussen vor - gerechnet wird nur der Direktschall
// ("Waende rausgekuerzt"). Der Raum dient der Darstellung und begrenzt,
// wohin sich Boxen und Hoerplatz schieben lassen.
namespace Geometry
{
    // Schallgeschwindigkeit in Luft bei rund 20 Grad.
    inline constexpr double speedOfSound = 343.0;

    // Der Pegelausgleich rechnet mit einem Exponenten: (d/dmax)^n, was den
    // Dezibelwert von 1/r mit n vervielfacht. n = 1 ist die Rechnung fuers
    // Freie, im Raum ist sie zu scharf - dort traegt der Diffusschall dazu
    // bei, dass der Unterschied zwischen nah und fern viel kleiner ausfaellt.
    // Am Hoerplatz nachgemessen: bei Wegen von 0,9 m und 4,6 m verlangt 1/r
    // -14,0 dB, gehoert richtig waren -4,2 dB.
    inline constexpr double roomExponent = 0.30;

    // Der Regler zeigt Prozent von genau dieser Rechnung: 100 % ist 1/r,
    // 0 % laesst die Pegel in Ruhe, 200 % verdoppelt den Dezibelwert. Die
    // Skala bleibt damit geradlinig - der gehoerte Wert steckt in der
    // Vorgabe des Parameters, nicht in einer verbogenen Achse.
    inline constexpr double maxExponent = 2.0;

    inline double gainExponent (double percent)
    {
        return std::max (0.0, percent) * 0.01;
    }

    struct Point { double x = 0.0, y = 0.0; };

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

        // Vom Platz aus gesehen steht die als "links" gefuehrte Box rechts -
        // so ist es, sobald man hinter der Aufstellung sitzt. Der Kopf dreht
        // sich dann herum, und mit ihm tauschen die Seiten.
        bool mirrored = false;

        // Welche Seite korrigiert wird - die naehere. Bei gleichen Wegen
        // steht der Hoerplatz mittig und es gibt nichts zu tun.
        bool leftIsNearer() const { return distL < distR; }
        bool centred (double tolerance = 0.005) const { return std::fabs (distL - distR) < tolerance; }

        // Betrag der Korrektur, unabhaengig von der Seite: der Pegel-
        // unterschied zwischen den beiden Boxen, wie ihn die Anzeige zeigt.
        double delaySeconds() const { return std::max (delayL, delayR); }
        double gainRatio() const { return std::min (gainL, gainR) / std::max (gainL, gainR); }
    };

    // gainAmount steuert, wie weit der Pegelausgleich geht: 1.0 ist die
    // volle Rechnung nach 1/r, 0.0 laesst die Pegel in Ruhe, 2.0 verdoppelt
    // die Absenkung in Dezibel. Im Raum faellt der Pegel wegen des
    // Diffusschalls flacher ab als im Freien - dort liegt das Gehoerte
    // naeher an Werten unter 1.0.
    inline Alignment compute (Point left, Point right, Point listener, double gainAmount = 1.0)
    {
        Alignment a;

        const double lx = left.x  - listener.x;
        const double ly = left.y  - listener.y;
        const double rx = right.x - listener.x;
        const double ry = right.y - listener.y;

        // Der Mindestabstand haelt die Rechnung heil, wenn jemand den
        // Hoerplatz genau in eine Box schiebt.
        a.distL = std::max (0.05, std::sqrt (lx * lx + ly * ly));
        a.distR = std::max (0.05, std::sqrt (rx * rx + ry * ry));

        // Laufzeit: die naehere Box wird so lange aufgehalten, bis die
        // weiter entfernte eingeholt hat. Verzoegert wird also immer nur,
        // vorziehen kann man Schall nicht.
        const double far = std::max (a.distL, a.distR);
        a.delayL = (far - a.distL) / speedOfSound;
        a.delayR = (far - a.distR) / speedOfSound;

        // Pegel: der Schalldruck faellt mit 1/r. Damit beide Boxen am
        // Hoerplatz gleich laut ankommen, muss die naehere leiser sein - im
        // Verhaeltnis der Abstaende, als Potenz gerechnet, weil der Anteil in
        // Dezibel wirken soll.
        //
        // Verteilt wird der Unterschied auf beide Seiten: die naehere Box
        // geht um die halbe Spanne herunter, die fernere um dieselbe halbe
        // Spanne herauf. Das Produkt beider Faktoren bleibt dadurch 1, die
        // Gesamtlautstaerke aendert sich also nicht, wenn der Hoerplatz
        // wandert - anders als beim blossen Absenken, das alles leiser macht.
        // Der Preis ist Headroom: ein Kanal wird angehoben.
        const double half = 0.5 * gainAmount;
        a.gainL = std::pow (a.distL / a.distR, half);
        a.gainR = std::pow (a.distR / a.distL, half);

        // Seitenvertauschung: das Vorzeichen des Kreuzprodukts der beiden
        // Blickrichtungen sagt, ob die linke Box vom Platz aus links liegt.
        a.mirrored = (lx * ry - ly * rx) < 0.0;

        // Richtungen zu den Boxen, normiert.
        const double nlx = lx / a.distL, nly = ly / a.distL;
        const double nrx = rx / a.distR, nry = ry / a.distR;

        // Oeffnungswinkel zwischen beiden Richtungen (Stereobasis, wie sie
        // der Hoerer an diesem Platz sieht).
        const double dot = std::clamp (nlx * nrx + nly * nry, -1.0, 1.0);
        a.baseAngleDeg = std::acos (dot) * 180.0 / M_PI;

        // Beste Mittenstellung = Winkelhalbierende der beiden Richtungen.
        // Sie ist die Blickrichtung, in der die Phantommitte genau vorne
        // sitzt. Bezug ist "geradeaus", also die Richtung -y (zur vorderen
        // Wand hin).
        const double bx = nlx + nrx;
        const double by = nly + nry;
        a.headAngleDeg = (bx == 0.0 && by == 0.0)
                       ? 0.0
                       : std::atan2 (bx, -by) * 180.0 / M_PI;

        return a;
    }

    // Bezugssystem der Aufstellung: die Mitte zwischen den Boxen, die Achse
    // durch beide, und die Senkrechte darauf in den Raum hinein.
    struct Frame
    {
        Point mid    { 0.0, 0.0 };
        Point along  { 1.0, 0.0 };   // von der linken zur rechten Box
        Point across { 0.0, 1.0 };   // senkrecht dazu, in den Raum
    };

    inline Frame frameOf (Point left, Point right)
    {
        Frame f;
        f.mid = { (left.x + right.x) * 0.5, (left.y + right.y) * 0.5 };

        const double dx = right.x - left.x;
        const double dy = right.y - left.y;
        const double len = std::sqrt (dx * dx + dy * dy);

        // Stehen beide Boxen aufeinander, gibt es keine Achse - dann bleibt
        // es bei der waagerechten Vorgabe.
        if (len > 1.0e-6)
            f.along = { dx / len, dy / len };

        f.across = { -f.along.y, f.along.x };
        return f;
    }

    // Der Hoerplatz, gemessen an der Aufstellung statt am Raum: quer der
    // Versatz aus der Mitte zwischen den Boxen, laengs der Abstand zur Achse
    // zwischen ihnen. Gehoert wird die Aufstellung, nicht die Wand hinter
    // ihr - deshalb ist sie der Bezug. Ein negativer Abstand heisst: der
    // Platz liegt hinter der Achse.
    struct Seat
    {
        double sideways = 0.0;
        double distance = 0.0;
    };

    inline Seat seatOf (Point left, Point right, Point listener)
    {
        const auto f = frameOf (left, right);
        const double dx = listener.x - f.mid.x;
        const double dy = listener.y - f.mid.y;
        return { dx * f.along.x  + dy * f.along.y,
                 dx * f.across.x + dy * f.across.y };
    }

    inline Point seatToWorld (Point left, Point right, Seat seat)
    {
        const auto f = frameOf (left, right);
        return { f.mid.x + f.along.x * seat.sideways + f.across.x * seat.distance,
                 f.mid.y + f.along.y * seat.sideways + f.across.y * seat.distance };
    }

    // Abstand der beiden Boxen voneinander.
    inline double speakerDistance (Point left, Point right)
    {
        const double dx = right.x - left.x;
        const double dy = right.y - left.y;
        return std::sqrt (dx * dx + dy * dy);
    }
}
