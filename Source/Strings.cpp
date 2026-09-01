#include "Strings.h"

namespace
{
    const Texts german
    {
        "Hörplatz",
        "Laufzeit und Pegel beider Boxen auf den Sitzplatz ausgerichtet",

        "Boxenabstand",
        "Raumbreite",
        "Raumtiefe",
        "Fläche",
        "Hörplatz seitlich",
        "Hörplatz Abstand",

        "Bypass Laufzeit",
        "Bypass Pegel",

        "Korrektur",
        "Laufzeit",
        "Pegel",
        "links",
        "rechts",
        "mittig",
        "aus",

        "Hilfe ein- und ausschalten",
        "Sprache umschalten",
        "Raum von oben. Boxen und Hörplatz lassen sich ziehen.",
        "Box ziehen: so steht sie bei dir im Raum.",
        "Hörplatz ziehen. Der Kopf dreht sich in die beste Mittenstellung.",
        "Zieht beide Boxen um ihre Mitte auf diesen Abstand.",
        "Breite des Raumes.",
        "Tiefe des Raumes.",
        "Hörplatz nach links oder rechts, gemessen von der Mitte zwischen den Boxen.",
        "Abstand des Hörplatzes zur Boxenebene.",
        "Lässt die Laufzeit unangetastet - zum Vergleichen.",
        "Lässt den Pegel unangetastet - zum Vergleichen.",
        "Um wieviel die nähere Box verzögert und abgesenkt wird."
    };

    const Texts english
    {
        "Hörplatz",
        "Delay and level of both speakers aligned to your seat",

        "Speaker distance",
        "Room width",
        "Room depth",
        "Area",
        "Seat sideways",
        "Seat distance",

        "Bypass delay",
        "Bypass level",

        "Correction",
        "Delay",
        "Level",
        "left",
        "right",
        "centred",
        "off",

        "Show or hide help",
        "Switch language",
        "Room from above. Speakers and seat can be dragged.",
        "Drag the speaker to where it stands in your room.",
        "Drag your seat. The head turns to the best facing angle.",
        "Moves both speakers around their centre to this distance.",
        "Width of the room.",
        "Depth of the room.",
        "Seat left or right of the centre between the speakers.",
        "Distance from the seat to the speaker line.",
        "Leaves the delay untouched - for comparison.",
        "Leaves the level untouched - for comparison.",
        "How much the nearer speaker is delayed and turned down."
    };
}

const Texts& texts (Lang lang)
{
    return lang == Lang::de ? german : english;
}
