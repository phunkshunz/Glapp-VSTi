#!/bin/bash
# Installerar GLAPP. Kör från Terminal:  bash installera.command
set -e
cd "$(dirname "$0")"

VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"

rm -rf "$VST3/GLAPP.vst3" "$AU/GLAPP.component"
cp -R GLAPP.vst3 "$VST3/"
cp -R GLAPP.component "$AU/"

# Pluginet är osignerat, så macOS spärrar det. Detta tar bort spärren.
xattr -dr com.apple.quarantine "$VST3/GLAPP.vst3" "$AU/GLAPP.component" GLAPP.app 2>/dev/null || true

echo ""
echo "Klart! GLAPP ligger nu i:"
echo "  $VST3"
echo "Stäng Bitwig helt och starta igen. GLAPP finns bland instrumenten."
echo "Lägg BETONG direkt efter GLAPP i samma kedja, eller slå på PROVLJUD för att höra den direkt."
