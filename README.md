# Kickforge

Kick-Builder-Plugin für **Hardtechno, Industrial Hardtechno und Schranz**.
VST3 + Standalone, Windows x64 — gebaut mit JUCE / C++20.

## Status: v0.6 — Industrial-Design

### Neu in v0.6

- **Eigenes LookAndFeel** (`KickforgeLookAndFeel`): Stahl-Knobs mit Skala,
  leuchtendem Wertbogen und Zeiger (bipolar bei EQ-Gain/Transient),
  LED-Schalter, kantige Buttons/Combos, dunkle Popup-Menüs.
- **Logo**: Amboss-Emblem mit aufsteigenden Kick-Peaks (`Resources/Logo/emblem.svg`,
  Generator-Skripte daneben) + Wordmark KICK/FORGE.
- **Schrift**: Chakra Petch (SIL Open Font License, `Resources/Fonts/OFL.txt`),
  eingebettet als BinaryData.
- **Hierarchie**: feste Knob-Größen — groß (Tune/Pitch/Decay), mittel
  (Drive/Level, EQ, FX), klein (Filter/Delay).
- **Visualizer**: Click/Body/Tail als farbige Flächen + Summe als Kontur,
  Zeitlineal in ms, Länge passt sich der Kick an. Rendert im Hintergrund-Thread.
- **Output-Meter** im Master (Segment-LEDs + Peak-Hold).
- **Werte mit Einheiten**: „2.50 kHz", „8.0 ms", „45 %", „+3.0 dB" — Eingabe
  per Tippen versteht „2.5k", „120ms", „1.2s".
- **Doppelklick** auf einen Knob = Default-Wert.

### Synthese (seit v0.5)

Synthese + Sample-Playback kombiniert. Jetzt kann der Click-Layer entweder
synthetisches Noise/Blip/Sine/Triangle sein — oder dein eigenes WAV-Sample
(Transient-Lib, Vinyl-Rip, aufgenommener Snare-Hit…).

```
                                             ┌──────────┐
  CLICK (osc/sample)   → [Shaper] → [Filter]─┤          │
  BODY  (osc)          → [Shaper] → [Filter]─┤   MIX    ├─► OvS↑→Drive→Shape→OvS↓→EQ→Trans→Reverb→SoftClip→Gain→OUT
  TAIL  (osc)          → [Shaper] → [Filter]─┤          │
         └── alle mit eigenem Layer-Delay ───┘
```

### Neu in v0.5

- **Sample-Playback-Modus** im Click-Layer: `OSC = Sample`.
  Load-Button lädt WAV/AIFF/FLAC/MP3/OGG, Pitch wirkt auf Playback-Rate
  (via `tune` als Referenz-Frequenz). Sample-Pfad wird mit dem Projekt
  gespeichert und beim Laden automatisch nachgeladen.

- **Master Reverb** (`juce::dsp::Reverb`): Room Size, Damping, Wet.
  Für Industrial-Räume und Schranz-Hall. Standard: Off.

- **Transient Shaper** auf Master: Attack & Sustain bipolar (-1 bis +1).
  Zwei-Follower-Detection (fast + slow) — positiver Diff = Attack-Boost,
  negativer Diff = Sustain-Boost. Klassisches SPL-Transient-Designer-Prinzip.

- **Layer-Delays** pro Layer (0–100 ms). Click vor Body, Body vor Tail —
  oder Tail minimal verzögert für mehr Breite im Low-End. Mit Phase-Alignment
  via Delay wirkt die Summe deutlich druckvoller.

Jetzt bei **55 Parametern** total.

### Layer-Charakter (Werks-Defaults, auf Schranz/Industrial getrimmt)

| Layer | Default-OSC | Default-Shaper | Default-Filter | Default-Delay |
|-------|-------------|----------------|----------------|---------------|
| CLICK | Noise       | HardClip       | HP @ 300 Hz    | 0 ms          |
| BODY  | Sine        | HardClip       | Off            | 0 ms          |
| TAIL  | Sine        | Tanh           | LP @ 180 Hz    | 0 ms          |

Click kann auf **Sample** umgestellt werden — dann lädst du dein eigenes
Transient-Sample (ein guter Vinyl-Click, ein akustischer Snare-Hit, ein
Industrial-Metal-Treffer) und spielst damit anstelle des synthetischen Noise-Bursts.

### Chain-Reihenfolge Master

```
Mix → OvS Up → Master Drive → Master Shaper → OvS Down
    → Master EQ → Transient Shaper → Reverb → Soft-Clip → Gain → Out
```

## Build (Windows x64)

**Einfachster Weg:** `build-windows.bat` doppelklicken. Das Skript findet
Visual Studio, lädt JUCE 8.0.4 herunter, baut VST3 + Standalone, legt alles
in `dist\` und installiert die VST3 auf Wunsch nach
`C:\Program Files\Common Files\VST3\`.

Voraussetzung: Visual Studio 2022 Community (kostenlos) mit dem Workload
„Desktopentwicklung mit C++".

**Ohne lokale Installation:** Das Repo enthält eine `appveyor.yml`. Auf
[ci.appveyor.com](https://ci.appveyor.com) mit GitHub anmelden (kostenlos für
öffentliche Repos), Projekt hinzufügen, Build starten. Das fertige Paket
liegt danach unter **Artifacts** als `Kickforge-Windows-x64.zip`.

Manuell (Developer Command Prompt):

```bat
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE external/JUCE
cmake -S . -B build -A x64
cmake --build build --config Release
```

## DSP-Tests (optional)

```bash
cmake -S . -B build -DKICKFORGE_BUILD_TESTS=ON
cmake --build build --config Release --target KickforgeTests
```

Prüft u. a.: keine NaN/Inf, Soft-Clip hält Peak < 1.0 bei allen Shapern auf
Max-Drive, Layer-Delay trifft den Onset sample-genau, Sample-Modus spielt und
endet korrekt, SVF-LP/HP dämpfen > 20 dB, ungerade Host-Blockgrößen und die
Render-Zeit des Visualizers.

## Roadmap

**Phase 6 — Workflow / Content**
- Factory-Preset-Erweiterung mit Sample-basierten Presets
- Drag-to-DAW direkt aus dem Visualizer
- Preset-Browser mit Kategorien (Stile als Tabs)
- Randomizer mit Style-Rules

**Phase 7 — DSP-Feinschliff**
- Pre-Filter *vor* dem Shaper (für andere Verzerrungs-Charaktere)
- Bass-Mono unter X Hz
- Side-Chain-Ducking-Ausgang für andere Spuren
- Convolution-Reverb als Alternative zum algorithmischen Reverb
