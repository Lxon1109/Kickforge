/*
    Kickforge Tests — Engine + Processor offline prüfen.
    Build: cmake -DKICKFORGE_BUILD_TESTS=ON …  → Target "KickforgeTests"

      KickforgeTests                          alle Tests
      KickforgeTests --render-preset <i> <out.raw> [bpm] [beats]
                                              Preset i über processBlock rendern (Float32 Stereo, 44.1 kHz)
      KickforgeTests --snapshot <out.png> [preset]
                                              Editor als PNG rendern (UI-Kontrolle)
      KickforgeTests --list-presets
*/
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond, ...) \
    do { char _m[256]; std::snprintf (_m, sizeof (_m), __VA_ARGS__); \
         if (cond) std::printf ("  [ OK ] %s\n", _m); else { std::printf ("  [FAIL] %s\n", _m); ++failures; } } while (0)

using Buf = juce::AudioBuffer<float>;

// ---------------------------------------------------------------- Hilfen
static float peakOf (const Buf& b, int from = 0, int to = -1)
{
    if (to < 0) to = b.getNumSamples();
    float p = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = from; i < to; ++i) p = std::max (p, std::abs (b.getSample (ch, i)));
    return p;
}
static float rmsDb (const Buf& b, int from = 0, int to = -1)
{
    if (to < 0) to = b.getNumSamples();
    double s = 0.0; int n = 0;
    for (int i = from; i < to; ++i, ++n) { const float m = 0.5f * (b.getSample (0, i) + b.getSample (1, i)); s += m * m; }
    return (float) (10.0 * std::log10 (s / std::max (1, n) + 1e-20));
}
static bool finite (const Buf& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (ch, i))) return false;
    return true;
}
// Pegel im Tiefband (Einpol-Tiefpass ~120 Hz) als RMS-dB
static float lowBandDb (const Buf& b, double sr, int from, int to)
{
    const float a = std::exp (-2.0f * juce::MathConstants<float>::pi * 120.0f / (float) sr);
    float y1 = 0, y2 = 0; double s = 0; int n = 0;
    for (int i = 0; i < to; ++i)
    {
        const float x = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
        y1 = x * (1 - a) + a * y1; y2 = y1 * (1 - a) + a * y2;
        if (i >= from) { s += y2 * y2; ++n; }
    }
    return (float) (10.0 * std::log10 (s / std::max (1, n) + 1e-20));
}

static void loadPreset (KickforgeAudioProcessor& p, int idx) { p.getPresetManager().load (idx); }
static void setParam (KickforgeAudioProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

// Kick(s) über processBlock rendern: eine Note pro Beat
static Buf renderViaProcessor (KickforgeAudioProcessor& p, double sr, int numSamples, int block = 480,
                               int noteEvery = 0, int channels = 2, float vel = 1.0f, int note = 36,
                               int maxNotes = 1 << 30)
{
    p.setPlayConfigDetails (0, channels, sr, block);
    p.prepareToPlay (sr, block);
    Buf out (channels, numSamples);
    Buf b (channels, block);
    for (int off = 0; off < numSamples; off += block)
    {
        const int n = std::min (block, numSamples - off);
        b.setSize (channels, n, false, false, true);
        b.clear();
        juce::MidiBuffer midi;
        for (int i = 0; i < n; ++i)
        {
            const int t = off + i;
            if ((t == 0 || (noteEvery > 0 && t % noteEvery == 0)) && (noteEvery <= 0 ? t == 0 : t / noteEvery < maxNotes))
                midi.addEvent (juce::MidiMessage::noteOn (1, note, vel), i);
        }
        p.processBlock (b, midi);
        for (int ch = 0; ch < channels; ++ch) out.copyFrom (ch, off, b, ch, 0, n);
    }
    return out;
}

// ---------------------------------------------------------------- Modi
static int renderPresetMode (int idx, const char* path, float bpm, int beats, int block = 512)
{
    KickforgeAudioProcessor p;
    loadPreset (p, idx);
    if (bpm > 0) setParam (p, "bpm", bpm);
    setParam (p, kf::params::syncId, 0.0f);
    const double sr = 44100.0;
    const auto prm = p.buildParams();
    const int beatLen = (int) std::lround (60.0 / prm.bpm * sr);
    const int total = beatLen * beats + (int) (0.25 * sr);
    auto out = renderViaProcessor (p, sr, total, block, beatLen, 2, 1.0f, 36, beats);
    std::vector<float> inter ((size_t) total * 2);
    for (int i = 0; i < total; ++i) { inter[(size_t) 2 * i] = out.getSample (0, i); inter[(size_t) 2 * i + 1] = out.getSample (1, i); }
    FILE* f = std::fopen (path, "wb");
    if (! f) return 3;
    std::fwrite (inter.data(), sizeof (float), inter.size(), f);
    std::fclose (f);
    return 0;
}

static int snapshotMode (const char* path, int preset)
{
    juce::ScopedJuceInitialiser_GUI gui;
    KickforgeAudioProcessor p;
    p.setPlayConfigDetails (0, 2, 44100.0, 512);
    p.prepareToPlay (44100.0, 512);
    if (preset >= 0) loadPreset (p, preset);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    // Visualizer-Thread fertig werden lassen + Nachrichten abarbeiten
    for (int i = 0; i < 40; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (25);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (path));
    out.deleteFile();
    juce::FileOutputStream fos (out);
    juce::PNGImageFormat png;
    const bool ok = fos.openedOk() && png.writeImageToStream (img, fos);
    std::printf ("%s %s (%dx%d)\n", ok ? "geschrieben" : "FEHLER", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return ok ? 0 : 1;
}

// ---------------------------------------------------------------- Tests
int main (int argc, char** argv)
{
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "--render-preset" && argc >= 4)
        return renderPresetMode (std::atoi (argv[2]), argv[3], argc > 4 ? (float) std::atof (argv[4]) : 0.0f,
                                 argc > 5 ? std::atoi (argv[5]) : 1, argc > 6 ? std::atoi (argv[6]) : 512);
    if (mode == "--snapshot" && argc >= 3)
        return snapshotMode (argv[2], argc > 3 ? std::atoi (argv[3]) : -1);
    if (mode == "--list-presets")
    {
        int i = 0;
        for (auto& fp : kf::factoryPresets()) std::printf ("%d %s\n", i++, fp.name);
        return 0;
    }

    juce::ScopedJuceInitialiser_GUI gui;   // APVTS/Editor brauchen den MessageManager
    std::printf ("Kickforge Tests\n");
    const auto& presets = kf::factoryPresets();

    // 1) Alle Factory-Presets: endlich, ≤ 0 dBFS, laut wie Hardtechno-Kicks, harter Schnitt am Beat-Ende
    std::printf ("\n-- Factory-Presets (%d) --\n", (int) presets.size());
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        KickforgeAudioProcessor p;
        loadPreset (p, i);
        setParam (p, kf::params::syncId, 0.0f);
        const double sr = 44100.0;
        const auto prm = p.buildParams();
        const int beat = (int) std::lround (60.0 / prm.bpm * sr);
        auto b = renderViaProcessor (p, sr, beat + (int) (0.3 * sr));
        const float pk = peakOf (b);
        const float rms = rmsDb (b, 0, beat);
        const int afterRelease = beat + (int) ((prm.releaseMs > 0.5f ? 7.0f * prm.releaseMs * 0.001f : 0.0f) * sr) + 400;
        const float rest = peakOf (b, std::min (afterRelease, b.getNumSamples() - 1));
        CHECK (finite (b) && pk <= 1.0f && pk > 0.7f && rms > -12.0f && rms < -2.0f && rest < 1.0e-3f,
               "%-22s peak %5.2f dBFS  rms %5.1f dB  Rest nach Ende %.1e", presets[(size_t) i].name,
               juce::Decibels::gainToDecibels (pk), rms, rest);
    }

    // 2) Länge folgt dem Tempo (Host-Sync über manuelles Tempo simuliert)
    std::printf ("\n-- Tempo / Länge --\n");
    for (float bpm : { 140.0f, 150.0f, 160.0f, 175.0f })
    {
        KickforgeAudioProcessor p;
        loadPreset (p, 0);
        setParam (p, kf::params::syncId, 0.0f);
        setParam (p, "bpm", bpm);
        setParam (p, "releaseMs", 0.0f);
        const double sr = 48000.0;
        const int beat = (int) std::lround (60.0 / bpm * sr);
        auto b = renderViaProcessor (p, sr, beat + 4800);
        const float before = peakOf (b, beat - 960, beat - 480);   // 20..10 ms vor dem Ende: noch laut
        const float after  = peakOf (b, beat + 480, beat + 4800);   // ab 10 ms nach dem Ende still (Master-Filter klingen ~5 ms aus)
        CHECK (before > 0.2f && after < 1.0e-3f, "%.0f BPM: Kick endet nach %.1f ms (vorher %.2f, danach %.1e)",
               bpm, 60000.0 / bpm, before, after);
    }

    // 3) Determinismus: jede Kick identisch (Seed)
    {
        KickforgeAudioProcessor p;
        loadPreset (p, 5);
        setParam (p, kf::params::syncId, 0.0f);
        const double sr = 44100.0;
        const int beat = (int) std::lround (60.0 / p.buildParams().bpm * sr);
        auto b = renderViaProcessor (p, sr, beat * 4, 512, beat);
        float diff = 0.0f;
        for (int i = 0; i < beat; ++i)
            for (int ch = 0; ch < 2; ++ch) diff = std::max (diff, std::abs (b.getSample (ch, beat * 2 + i) - b.getSample (ch, beat * 3 + i)));
        CHECK (diff < 1.0e-3f, "Kick 3 und 4 im Loop identisch (max. Abweichung %.1e)", diff);
    }

    // 4) Offline-Render == Live-Render (Visualizer/Export zeigen genau das, was man hört)
    {
        KickforgeAudioProcessor p;
        loadPreset (p, 1);
        setParam (p, kf::params::syncId, 0.0f);
        const double sr = 44100.0;
        const int n = p.kickLengthSamples (sr);
        auto live = renderViaProcessor (p, sr, n, 512);
        Buf off (2, n);
        p.renderOffline (off, sr, -1);
        float diff = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i) diff = std::max (diff, std::abs (live.getSample (ch, i) - off.getSample (ch, i)));
        CHECK (diff < 1.0e-4f, "Offline-Render = Live-Render (max. Abweichung %.1e)", diff);
    }

    // 5) Abtastraten-unabhängig: 44.1 vs 96 kHz
    {
        float rms[2], low[2];
        int k = 0;
        for (double sr : { 44100.0, 96000.0 })
        {
            KickforgeAudioProcessor p;
            loadPreset (p, 0);
            setParam (p, kf::params::syncId, 0.0f);
            const int beat = (int) std::lround (60.0 / p.buildParams().bpm * sr);
            auto b = renderViaProcessor (p, sr, beat, 512);
            rms[k] = rmsDb (b); low[k] = lowBandDb (b, sr, (int) (0.1 * sr), beat); ++k;
        }
        CHECK (std::abs (rms[0] - rms[1]) < 1.5f && std::abs (low[0] - low[1]) < 1.5f,
               "44.1 vs 96 kHz: RMS %.1f / %.1f dB, Bass %.1f / %.1f dB", rms[0], rms[1], low[0], low[1]);
    }

    // 6) Velocity
    {
        float r[2]; int k = 0;
        for (float vel : { 1.0f, 0.4f })
        {
            KickforgeAudioProcessor p;
            loadPreset (p, 0);
            setParam (p, kf::params::syncId, 0.0f);
            auto b = renderViaProcessor (p, 44100.0, 17640, 512, 0, 2, vel);
            r[k++] = rmsDb (b);
        }
        CHECK (r[1] < r[0] - 1.0f, "Velocity 0.4 leiser als 1.0 (%.1f vs %.1f dB)", r[1], r[0]);
    }

    // 7) Schnelles Retriggern (Rolls) und Mono-Ausgang: stabil, ≤ 0 dBFS
    {
        KickforgeAudioProcessor p;
        loadPreset (p, 9);
        setParam (p, kf::params::syncId, 0.0f);
        auto b = renderViaProcessor (p, 44100.0, 44100, 256, 1323);          // alle 30 ms
        CHECK (finite (b) && peakOf (b) <= 1.0f, "Rolls alle 30 ms: endlich, Peak %.2f", peakOf (b));
        KickforgeAudioProcessor pm;
        loadPreset (pm, 0);
        auto m = renderViaProcessor (pm, 44100.0, 17640, 512, 0, 1);
        CHECK (finite (m) && peakOf (m) > 0.5f && peakOf (m) <= 1.0f, "Mono-Ausgang: Peak %.2f", peakOf (m));
    }

    // 8) Sample-Slot: Sample ersetzt die Schicht, Entfernen schaltet zurück
    {
        auto tmp = juce::File::createTempFile (".wav");
        {
            Buf s (1, 22050);
            for (int i = 0; i < s.getNumSamples(); ++i)
                s.setSample (0, i, 0.8f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) i / 44100.0f)
                                   * std::exp (-(float) i / 6000.0f));
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (tmp), 44100.0, 1, 24, {}, 0));
            w->writeFromAudioSampleBuffer (s, 0, s.getNumSamples());
        }
        KickforgeAudioProcessor p;
        loadPreset (p, 0);
        setParam (p, kf::params::syncId, 0.0f);
        Buf synth (2, 8000), smp (2, 8000);
        p.renderOffline (synth, 44100.0, 1);
        const bool loaded = p.loadLayerSample (1, tmp);
        const bool modeOn = p.apvts.getRawParameterValue (kf::params::srcId (1))->load() > 0.5f;
        p.renderOffline (smp, 44100.0, 1);
        float d = 0.0f;
        for (int i = 0; i < 8000; ++i) d = std::max (d, std::abs (synth.getSample (0, i) - smp.getSample (0, i)));
        CHECK (loaded && modeOn && d > 0.1f && finite (smp), "Punch-Sample geladen, Schicht auf SAMPLE, Klang ändert sich (Δ %.2f)", d);

        // Zustand inkl. Sample-Pfad wiederherstellen
        juce::MemoryBlock state;
        p.getStateInformation (state);
        KickforgeAudioProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (q.getLayerSamplePath (1) == tmp.getFullPathName()
               && std::abs (q.apvts.getRawParameterValue ("fEnd")->load() - p.apvts.getRawParameterValue ("fEnd")->load()) < 1e-4f,
               "Zustand speichern/laden: Parameter + Sample-Pfad");

        p.clearLayerSample (1);
        CHECK (p.apvts.getRawParameterValue (kf::params::srcId (1))->load() < 0.5f && p.getLayerSamplePath (1).isEmpty(),
               "Sample entfernen schaltet zurück auf SYNTH");
        tmp.deleteFile();
    }

    // 9) Key Track: Note bestimmt den Grundton
    {
        auto tailHz = [] (const Buf& b, double sr, int from, int to)
        {
            // Nulldurchgänge des tiefpassgefilterten Signals zählen
            const float a = std::exp (-2.0f * juce::MathConstants<float>::pi * 90.0f / (float) sr);
            float y1 = 0, y2 = 0, prev = 0; int zc = 0, first = -1, last = -1;
            for (int i = 0; i < to; ++i)
            {
                const float x = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
                y1 = x * (1 - a) + a * y1; y2 = y1 * (1 - a) + a * y2;
                if (i > from && prev < 0 && y2 >= 0) { ++zc; if (first < 0) first = i; last = i; }
                prev = y2;
            }
            return zc > 2 ? (float) ((zc - 1) * sr / (last - first)) : 0.0f;
        };
        float hz[2]; int k = 0;
        for (int note : { 36, 41 })   // C2 = 65.4 Hz, F2 = 87.3 → gefaltet 43.7 Hz
        {
            KickforgeAudioProcessor p;
            loadPreset (p, 1);
            setParam (p, kf::params::syncId, 0.0f);
            setParam (p, kf::params::keytrackId, 1.0f);
            setParam (p, "rumbleLevel", 0.0f);
            setParam (p, "gritLevel", 0.0f);
            setParam (p, "punchLenMs", 380.0f);
            auto b = renderViaProcessor (p, 44100.0, 17640, 512, 0, 2, 1.0f, note);
            hz[k++] = tailHz (b, 44100.0, 8000, 16500);
        }
        CHECK (std::abs (hz[0] - 65.4f) < 4.0f && std::abs (hz[1] - 43.65f) < 4.0f,
               "Key Track: C2 → %.1f Hz (65.4), F2 → %.1f Hz (43.7)", hz[0], hz[1]);
    }

    // 10) Parameter-Fuzz: zufällige Extremwerte für ALLE Parameter, wechselnde Abtastraten/Blockgrößen
    {
        juce::Random rng (0x6b66);
        int bad = 0; float worst = 0.0f;
        KickforgeAudioProcessor p;
        const double rates[] { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
        const int blocks[] { 32, 64, 128, 441, 512, 1024, 2048 };
        for (int it = 0; it < 120; ++it)
        {
            for (auto* prm : p.getParameters())
                prm->setValueNotifyingHost (rng.nextFloat() < 0.3f ? (rng.nextBool() ? 0.0f : 1.0f) : rng.nextFloat());
            setParam (p, kf::params::syncId, 0.0f);
            const double sr = rates[it % 5];
            const int blk = blocks[rng.nextInt (7)];
            auto b = renderViaProcessor (p, sr, (int) (0.25 * sr), blk, (int) (0.05 * sr) + rng.nextInt (500));
            const float pk = peakOf (b);
            worst = std::max (worst, std::isfinite (pk) ? pk : 99.0f);
            if (! finite (b) || pk > 1.0f) ++bad;
        }
        CHECK (bad == 0, "Fuzz: 120 Zufalls-Einstellungen, 5 Abtastraten, 7 Blockgrößen - %d Ausreißer, max. Peak %.3f", bad, worst);
    }

    // 11) Performance
    {
        KickforgeAudioProcessor p;
        loadPreset (p, 0);
        setParam (p, kf::params::syncId, 0.0f);
        const double sr = 48000.0;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        renderViaProcessor (p, sr, (int) (sr * 10.0), 256, 19200);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        const double load = ms / 10000.0 * 100.0;
        const bool ci = juce::SystemStats::getEnvironmentVariable ("CI", {}).isNotEmpty();
        if (ci) std::printf ("  [INFO] 10 s Audio in %.0f ms (%.1f %% CPU eines Kerns)\n", ms, load);
        else    CHECK (load < 25.0, "10 s Audio in %.0f ms (%.1f %% CPU eines Kerns)", ms, load);
    }

    std::printf ("\n%s - %d Fehler\n", failures == 0 ? "ALLE TESTS GRUEN" : "TESTS FEHLGESCHLAGEN", failures);
    return failures == 0 ? 0 : 1;
}
