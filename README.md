# Kickforge

Kick-Builder-Plugin für **Hardtechno, Industrial Hardtechno und Schranz**.
VST3 + Standalone, Windows x64. Gebaut mit JUCE und C++20.

## Status: v0.7 — Industrial-Engine

Die Engine ist neu und nach dem Bauplan echter Industrial-Hardtechno-Kicks aufgebaut.
Referenzen waren die Teknovault Industrial Kick Vault (Punchy / Heavy / Noisy / Raw /
Heavy Raw) und Harsh-Ripped-Kicks bei 160 BPM. Die Parameter der Factory-Presets
wurden per Analyse und Optimierung an diese Kicks angepasst: Bandpegel über die Zeit,
Hüllkurve, spektrale Flachheit, Unruhe im Tail und Nachklang.

```
CLICK   0–10 ms     Rausch-Knack, Hochpass
PUNCH   0–~250 ms   Sinus mit 2-stufigem Pitch-Sweep → Dirt → asymmetrische Sättigung
                    → Clipper → Tone → Hochpass-Sweep (Bass kommt später rein)
RUMBLE  ab ~50 ms   Anfang des Punch → langer Hall → Drive → Tiefpass → Drive → Tiefpass
                    (der unruhige, verzerrte Hall-Tail), bis Beat-Ende gehalten
GRIT    ab ~25 ms   verzerrte Mitten + Rauschen, Stereo (Haas + Breite), mit Länge/Tail
KICK                Länge in Beats (DAW-Tempo), harter Schnitt, optional Nachklang
                    mit fallender Tonhöhe, Variation (Seed)
MASTER              2-Band-Clipper: Bass < 200 Hz clippen + runden, Höhen sättigen,
                    weiche Grenze bei 0 dBFS. Alles 2× Oversampling.
```

### Bedienung

- **20 Factory-Presets**: *Industrial* Heavy / Punchy / Noisy / Raw / Heavy Raw (150 BPM)
  und *Harsh* Ripped + 14 Varianten (160 BPM).
- **Host Sync**: Die Kick ist genau so lang wie eingestellt (z. B. 1 Beat) beim Tempo
  der DAW. Ohne DAW-Tempo gilt der TEMPO-Knob.
- **Key**: Grundton der Kick (Hz + Notenname). **Key Track**: Die gespielte MIDI-Note
  bestimmt den Grundton, gefaltet auf 40–80 Hz.
- **Sample-Slots**: In jeder Schicht kann statt der Synthese ein eigenes Sample laufen.
  Dafür LOAD drücken oder eine WAV direkt auf die Schicht ziehen, GAIN/PITCH pro Slot.
  Der Pfad wird mit dem Projekt gespeichert.
- **Visualizer**: Click, Punch, Rumble und Grit als farbige Flächen, die Summe als
  Kontur, dazu eine Markierung am Beat-Ende.
- **Velocity** steuert die Lautstärke, der Klangcharakter bleibt gleich.
- **EXPORT WAV** speichert die Kick als 24-Bit-WAV inkl. Nachklang.

## Build (Windows x64)

**Ohne lokale Installation:** Das Repo enthält eine `appveyor.yml`. Auf
[ci.appveyor.com](https://ci.appveyor.com) läuft der Build kostenlos (öffentliches Repo).
Das fertige Paket liegt unter **Artifacts** als `Kickforge-Windows-x64.zip`.

**Lokal:** `build-windows.bat` doppelklicken. Voraussetzung ist Visual Studio 2022
Community mit dem Workload „Desktopentwicklung mit C++“.

Manuell (Developer Command Prompt):

```bat
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE external/JUCE
cmake -S . -B build -A x64
cmake --build build --config Release
```

## Tests

```bash
cmake -S . -B build -DKICKFORGE_BUILD_TESTS=ON
cmake --build build --config Release --target KickforgeTests KickforgeRender
```

`KickforgeTests` prüft:
- alle Presets: endlich, ≤ 0 dBFS, Lautheit wie Hardtechno-Kicks, Stille nach dem Ende
- Länge folgt dem Tempo
- jede Kick im Loop identisch, unabhängig von der Blockgröße
- Offline-Render = Live-Render
- 44.1 vs. 96 kHz klingen gleich
- Velocity, Rolls und Mono-Ausgang
- Sample-Slot und Speichern/Laden
- Key Track
- Parameter-Fuzz über alle Bereiche, Abtastraten und Blockgrößen
- CPU-Last

`KickforgeRender` rendert die Engine ohne DAW, z. B. für Klangvergleiche:

```bash
KickforgeRender out.raw fEnd=55 punchDrive=0.8 beats=16 bpm=150
```

Die Factory-Presets entstehen mit `Tools/gen_presets.py` aus den Fit-Ergebnissen
(`Tools/make_presets.sh`).

## Roadmap

- Preset-Browser mit Kategorien als Tabs, Favoriten
- Randomizer innerhalb eines Stils
- weitere Stile: Schranz, Bounce, Hardgroove (Referenzen analysieren → fitten)
- Drag-to-DAW direkt aus dem Visualizer
