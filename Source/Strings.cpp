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
        "L/R folgen dem Kopf",
        "Ausgleich",
        "Testgeräusch",

        "Korrektur",
        "Laufzeit",
        "Pegel",
        "links",
        "rechts",
        "L",
        "R",
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
        "Hörplatz quer zur Achse, gemessen von der Mitte zwischen den Boxen.",
        "Abstand des Hörplatzes zur Achse zwischen den Boxen, senkrecht gemessen.",
        "Lässt die Laufzeit unangetastet - zum Vergleichen.",
        "Lässt den Pegel unangetastet - zum Vergleichen.",
        "Sitzt du hinter den Boxen, steht die linke von dort aus rechts. Angeschaltet tauschen die Kanäle dann mit.",
        "Wie weit der Pegelausgleich geht. 100 % ist die Rechnung fürs Freie, im Raum liegt es darunter.",
        "Weiche Impulse in der Mitte. Stimmt die Einstellung, stehen sie als ein Punkt zwischen den Boxen. Beim Ausschalten klingt es in Stereo aus.",
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
        "L/R follow the head",
        "Amount",
        "Test sound",

        "Correction",
        "Delay",
        "Level",
        "left",
        "right",
        "L",
        "R",
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
        "Seat along the axis, measured from the centre between the speakers.",
        "Distance from the seat to the axis between the speakers, measured at a right angle.",
        "Leaves the delay untouched - for comparison.",
        "Leaves the level untouched - for comparison.",
        "Sitting behind the speakers, the left one stands to your right. Switched on, the channels swap along.",
        "How far the level correction goes. 100 % is the free-field maths, in a room it sits below that.",
        "Soft pulses in the centre. When the setting is right, they stand as one point between the speakers. Switching off, it fades out in stereo.",
        "How much the nearer speaker is delayed and turned down."
    };
}

const Texts& texts (Lang lang)
{
    return lang == Lang::de ? german : english;
}
