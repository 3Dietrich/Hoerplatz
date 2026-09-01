# Hörplatz

**Zwei Boxen, ein Sessel, und der steht selten in der Mitte.**
Hörplatz rechnet aus, wie weit jede Box von deinem Platz entfernt ist, und
gleicht den Unterschied aus: die nähere Box wird verzögert und leiser
gemacht, bis beide gleichzeitig und gleich laut bei dir ankommen. Die Mitte
sitzt dann wieder in der Mitte - auch schräg im Raum, auch auf dem Sofa an
der Seite.

![Die Oberfläche von Hörplatz](docs/screenshot.png)

Audio Unit, VST3 und Standalone für macOS. Gebaut mit JUCE.

## So geht's

Links siehst du deinen Raum von oben: die beiden Boxen an der vorderen Wand,
dazwischen dein Platz. Klick hin, wo du sitzt, oder zieh den Kopf dorthin.
Alles andere ergibt sich von selbst.

Der Kopf dreht sich dabei in die Richtung, in der die Phantommitte für
diesen Platz genau vor dir liegt - so herum sitzt du am besten. Die beiden
Linien zeigen die Weglängen, die dickere ist die längere: sie gibt den Takt
vor, an ihr richtet sich die andere aus.

**Die Regler**

| | |
|---|---|
| Boxenabstand | wie weit die Boxen auseinanderstehen, in Metern |
| Raumbreite, Raumtiefe | dein Raum; die Fläche steht darunter |
| Hörplatz seitlich, Abstand | dein Platz, gemessen von der Mitte zwischen den Boxen |
| Laufzeit umgehen | schaltet die Verzögerung ab |
| Pegel umgehen | schaltet die Lautstärke-Anpassung ab |

Die beiden Schalter sind zum Vergleichen da: einmal mit, einmal ohne. Der
Unterschied ist deutlicher, als man erwartet.

Unten rechts stehen die Zahlen dazu - Weglänge, Verzögerung und Absenkung je
Kanal, dazu die Basisbreite (60° ist das gleichseitige Dreieck, der übliche
Bezugspunkt) und der Blickwinkel.

Ausgelegt ist das Ganze auf Räume von 5 bis 90 m², die Regler reichen weiter,
falls du mehr brauchst.

## Was dahintersteckt

Gerechnet wird nur der Direktschall, ohne Wände und ohne Reflexionen - für
die Ortung ist der erste Schall entscheidend, alles danach kommt zu spät.

- **Weglänge** zu jeder Box, schlicht aus der Geometrie.
- **Verzögerung:** die nähere Box wartet, bis die weitere eingeholt hat,
  also `(dmax - d) / 343 m/s`. Verzögert wird immer nur - Schall vorziehen
  kann niemand.
- **Pegel:** der Schalldruck fällt mit `1/r`, die nähere Box wird also im
  Verhältnis der Weglängen abgesenkt (`d / dmax`). Der lautere Kanal bleibt
  bei 0 dB, übersteuern kann dabei nichts.

Der Laufzeitunterschied kann nie größer werden als der Boxenabstand geteilt
durch die Schallgeschwindigkeit - bei 12 m Abstand sind das 35 ms.

Die Verzögerung ist der Zweck des Plugins, keine Latenz zum Wegrechnen: die
DAW bekommt nur die vier Samples gemeldet, die die Interpolation als Sockel
braucht.

## Bauen

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
ctest --test-dir build --output-on-failure
```

JUCE wird aus `~/Documents/JUCE` eingebunden; ein anderer Ort geht über
`-DJUCE_DIR=...`. Die fertigen Bündel liegen danach unter
`build/Hoerplatz_artefacts/Release/`.

Audio Unit installieren und prüfen:

```
cp -R build/Hoerplatz_artefacts/Release/AU/Hoerplatz.component ~/Library/Audio/Plug-Ins/Components/
auval -v aufx Hp01 Dpnk
```

## Prüfungen

- `geometry_check` - die reine Rechnung ohne JUCE: Symmetrie, das
  1/r-Verhältnis, 60° beim gleichseitigen Dreieck und die Grenze des
  Laufzeitunterschieds über ein Raster von Positionen.
- `audio_check` - ein Impuls durch das fertige Plugin: Lage und Höhe in
  beiden Kanälen, auch mit den beiden Umgehungen.
- `ui_shot bild.png w=940 h=600 scale=2 spk=5.5 rw=6.5 rd=6.5 x=0 y=4.76` -
  zeichnet die Oberfläche in eine PNG-Datei, ohne dass ein Fenster aufgeht.
  Das Bild oben stammt daher.

## Lizenz

GPL-3.0, siehe [LICENSE](LICENSE).
