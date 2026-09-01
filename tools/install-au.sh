#!/bin/bash
# Installiert die gebaute Audio Unit und signiert sie ad-hoc.
#
# Das Ueberkopieren eines schon installierten Bundles laesst ein
# _CodeSignature zurueck, das nicht mehr zum Inhalt passt. macOS schiesst den
# ladenden Host dann mit "Code Signature Invalid" ab - im Host sieht man nur
# einen Ladekreisel. Deshalb: erst weg, dann kopieren, dann signieren.
set -e

BUNDLE="$(cd "$(dirname "$0")/.." && pwd)/build/Hoerplatz_artefacts/Release/AU/Hoerplatz.component"
ZIEL="$HOME/Library/Audio/Plug-Ins/Components"

if [ ! -d "$BUNDLE" ]; then
    echo "Nicht gebaut: $BUNDLE" >&2
    exit 1
fi

rm -rf "$ZIEL/Hoerplatz.component"
cp -R "$BUNDLE" "$ZIEL/"
codesign --force --deep --sign - "$ZIEL/Hoerplatz.component"
codesign --verify --strict "$ZIEL/Hoerplatz.component"

echo "installiert und signiert: $ZIEL/Hoerplatz.component"
echo "Der Host muss das Plugin neu laden - in Audio Hijack den Block entfernen und neu einsetzen."
