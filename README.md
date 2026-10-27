# Power Collage

Schräger Splitscreen für DaVinci Resolve. Die Bedienung folgt dem nativen Resolve FX **Video Collage**: Jeder Clip der Collage bekommt denselben Effekt mit einer eigenen **Active Tile**. Optional liegt darüber ein Clip im Modus **Create Background**. Anders als bei Video Collage trennen hier parallele Trennlinien mit einstellbarem Winkel die Kacheln, statt eines geraden Rasters.

Getestet mit DaVinci Resolve Studio 21.1.1 (macOS, Metal). Die Pixelberechnung läuft als GPU-Kernel in einer Fusion-Fuse, deshalb funktioniert der Effekt auch in Echtzeit.

## Installation

Am einfachsten mit dem Installer-Skript, das direkt in Resolve läuft:

1. `install.lua` herunterladen. Das ganze Repository ist nicht nötig, fehlende Dateien lädt das Skript selbst von GitHub.
2. In Resolve **Workspace > Console** öffnen, oben auf **Lua** umschalten und eingeben:
   ```lua
   dofile("/Pfad/zu/install.lua")
   ```
3. Im Dialog auswählen, was installiert werden soll: den Fusion-Effekt, das OpenFX-Plugin (nur macOS, fragt nach dem Admin-Passwort) oder beides.
4. Resolve neu starten.

### Fusion-Effekt per Terminal

```bash
./install.sh
```

Das Skript kopiert zwei Dateien in den Fusion-Ordner des Benutzers:

| Datei | Ziel |
| --- | --- |
| `Fuses/PowerCollage.fuse` | `.../Fusion/Fuses/` |
| `Templates/Edit/Effects/Power Collage.setting` | `.../Fusion/Templates/Edit/Effects/` |

Mit `./install.sh --link` werden die Dateien stattdessen verlinkt, sodass Änderungen im Repository direkt greifen. Danach Resolve **neu starten**, denn neue Fuses werden nur beim Start geladen.

Pfade für die manuelle Installation:

- macOS: `~/Library/Application Support/Blackmagic Design/DaVinci Resolve/Fusion/`
- Windows: `%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\Fusion\`
- Linux: `~/.local/share/DaVinciResolve/Fusion/`

## OpenFX-Plugin

Dieselbe Collage gibt es auch als OpenFX-Plugin. Es erscheint im Effects-Reiter unter **Open FX > Power Collage**, genau wie Video Collage, und funktioniert auch auf der Color-Page. Im Viewer (Overlay "Open FX") lassen sich die Trennlinien direkt bearbeiten: am Kreuz ziehen verschiebt die Linien, am Kreis daneben ziehen dreht sie.

**Fertiges Plugin installieren (ohne Xcode):** Das kompilierte Plugin liegt in `Plugin/PowerCollage.ofx.bundle` (macOS 11 oder neuer, Apple Silicon und Intel). Repository herunterladen, `install-openfx.command` per Doppelklick starten, Admin-Passwort eingeben und Resolve neu starten. Falls macOS den Doppelklick blockiert: Rechtsklick auf die Datei, dann *Öffnen*. Das Skript kopiert das Plugin nach `/Library/OFX/Plugins` und entfernt die Download-Sperre, die macOS sonst auf heruntergeladene Plugins legt.

Selbst bauen und installieren (macOS, braucht Xcode und das OpenFX-SDK, das Resolve unter `/Library/Application Support/Blackmagic Design/DaVinci Resolve/Developer/OpenFX` mitliefert):

```bash
cd OpenFX
make
sudo make install
```

Danach Resolve neu starten. Statt `install` verlinkt `sudo make link` den Systemordner einmalig auf den Build im Repository. Danach reicht nach jeder Änderung `make` und ein Neustart von Resolve. `make release` aktualisiert das fertige Plugin in `Plugin/` für die Weitergabe.

Das Plugin rechnet auf dem Mac per Metal auf der GPU. Auf anderen Systemen fällt es auf die CPU zurück, ein CUDA- oder OpenCL-Pfad fehlt noch. Ein Schlagschatten ist nicht eingebaut, dafür legst du in der Edit-Page zusätzlich das Resolve FX **Drop Shadow** auf den Clip.

## Workflow

1. Lege die Clips übereinander auf V1, V2, und so weiter. Die Reihenfolge der Spuren ist egal, weil jeder Clip auf seine Kachel maskiert wird.
2. Ziehe **Effects > Fusion Effects > Power Collage** auf den ersten Clip und stelle das Layout ein (Tiles, Angle, Split Center, Spacing ...).
3. Wende den Effekt auch auf die anderen Clips an und wähle dort jeweils eine andere **Active Tile**. Das Layout (Tiles, Angle, Split Center, Spacing, Margins, Rounding) muss in allen Clips identisch sein, genau wie bei Video Collage. Am schnellsten geht das, wenn du den fertig eingestellten Clip kopierst und die Attribute auf die anderen Clips überträgst.
4. Mit **Preview Layout** siehst du die Kacheln mit Nummern, die aktive Kachel ist gelb hervorgehoben.
5. Optional für Linien zwischen den Kacheln: **Spacing** > 0 setzen, einen Clip (z. B. Solid Color) auf die oberste Spur legen, Power Collage mit **Workflow = Create Background** darauf anwenden und dasselbe Layout einstellen. Mit **Background Uses Border Color** wird statt des Clips die Border Color als Rahmen verwendet.

Für den Look aus dem Referenz-Screenshot (9:16, eine Diagonale) reichen die Standardwerte: 2 Kacheln, 45°, Mittelpunkt in der Bildmitte.

Den Bildausschnitt eines Clips stellst du über **Resize Content** im Effekt ein, nicht über Transform im Inspector. Der Inspector-Transform wird erst nach dem Effekt angewendet und würde die Kachel mitverschieben.

## Parameter

Längenangaben (Spacing, Margins, Rounding, Softness, Border Width) beziehen sich auf die kürzere Bildseite, damit Hoch- und Querformat gleich reagieren.

**Workflow**

| Parameter | Funktion |
| --- | --- |
| Workflow | *Create Tile*: Clip in die aktive Kachel setzen. *Create Background*: Clip als Rahmen mit Löchern an den Kachelpositionen. |
| Preview Layout | Zeigt alle Kacheln mit Nummern statt des Bildes. |
| Active Tile | Kachel, in die dieser Clip gesetzt wird. Kachel 1 liegt oben bzw. links. |

**Layout**

| Parameter | Funktion |
| --- | --- |
| Tiles | Anzahl der Kacheln (2 bis 8), getrennt durch parallele Linien. |
| Angle | Winkel der Trennlinien in Grad. 0 = waagerecht (oben/unten), 90 = senkrecht (links/rechts). Lässt sich auch im Viewer ziehen. |
| Split Center | Verschiebt alle Trennlinien. Bei 2 Kacheln läuft die Linie durch diesen Punkt. Lässt sich auch im Viewer ziehen. |
| Spacing | Abstand zwischen den Kacheln. |
| Margins | Abstand zum Bildrand. |
| Rounding | Abrundung der Kachelecken. |
| Edge Softness | Weiche Kante an den Kachelrändern. |

**Resize Content**

| Parameter | Funktion |
| --- | --- |
| Move With Tile | *Position and Scale* (Standard): Clip auf den Schwerpunkt der Kachel zentrieren und so skalieren, dass er die Kachel immer ganz füllt. *Position Only*: nur zentrieren. *Off*: Clip bleibt, wo er ist. |
| Zoom | Zusätzlicher Zoom auf den Clip. |
| Left/Right Offset, Up/Down Offset | Verschiebt den Clip innerhalb der Kachel. |
| Content Edges | Verhalten außerhalb des Clips: Transparent, Mirror, Duplicate. |

**Tile Styling**

| Parameter | Funktion |
| --- | --- |
| Border Width, Border Color | Rand innerhalb der Kachel entlang der Trennlinien. |
| Border On Frame Edges | Den Rand auch an den Bildkanten zeichnen. |
| Background Uses Border Color | Im Modus *Create Background* die Border Color statt des Clips verwenden. |
| Draw Fill, Fill Color | Farbe hinter transparenten Bildbereichen innerhalb der Kachel. |
| Drop Shadow | Resolve FX Drop Shadow auf der Kachel, z. B. damit die obere Kachel einen Schatten auf die untere wirft. |

## Hinweise

- Ohne Spacing überlappen sich benachbarte Kacheln um etwa ein Pixel. So entsteht an der Diagonale keine dunkle Naht, wenn zwei geglättete Kanten auf verschiedenen Spuren übereinanderliegen.
- Alle Parameter lassen sich in Resolve 21 direkt in der Edit-Page keyframen, z. B. für eine animierte Trennlinie.
- In der Fusion-Page steht derselbe Effekt als Werkzeug **Power Collage** (Kategorie Fuses) zur Verfügung.

## Aufbau

- `Fuses/PowerCollage.fuse`: Lua berechnet pro Frame die Kachel-Polygone, Schwerpunkte und die Skalierung. Ein GPU-Kernel übernimmt Maskierung, Kantenglättung, Rundung, Rand und das Sampling des Clips.
- `Templates/Edit/Effects/Power Collage.setting`: Makro für die Effects Library der Edit-Page. Es enthält die Fuse plus Resolve FX Drop Shadow und veröffentlicht deren Parameter.
- `OpenFX/`: das OpenFX-Plugin. `PowerCollageGeometry.h` entspricht dem Lua-Teil der Fuse. `PowerCollageKernel.inc` ist der Pixel-Kernel, der einmal geschrieben und sowohl als C++ (CPU) als auch als Metal (GPU) kompiliert wird. `PowerCollagePlugin.cpp` enthält Parameter, Rendering und das Viewer-Overlay.
- `Testframes/`: zwei 9:16-Testbilder.
