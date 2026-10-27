#pragma once

#include <juce_core/juce_core.h>
#include "IndustrialEngine.h"

/*
    Zentrale Parameter-Tabelle: jede Zeile verbindet eine APVTS-ID mit einem
    Feld der IndustrialParams, dem Wertebereich und der Anzeige-Einheit.
    Processor (Layout + Audio-Snapshot), Editor (Knobs) und Presets nutzen
    dieselbe Tabelle → IDs können nicht auseinanderlaufen.

    Die Default-Werte kommen aus dem Factory-Preset "Industrial Heavy"
    (FactoryPresets.h), damit eine frische Instanz sofort nach Hardtechno klingt.
*/
namespace kf::params
{
    enum class Unit { Pct, Hz, Ms, Num, Beats, Semi, Gain, Key, Bpm, Int, Mult, MsBip, DecayMs };

    struct Def
    {
        const char* id;
        const char* name;        // Name in der DAW-Automation
        float min, max, skewCentre;   // skewCentre <= 0 → linear
        Unit  unit;
        float IndustrialParams::* field;
    };

    using P = IndustrialParams;

    // Reihenfolge = Reihenfolge in der DAW-Parameterliste
    inline const std::vector<Def>& engineDefs()
    {
        static const std::vector<Def> defs {
            // ---- Kick (global) ----
            { "fEnd",          "Key",            30.0f,   120.0f,   55.0f,  Unit::Key,   &P::fEnd },
            { "lengthBeats",   "Length",         0.25f,   2.0f,     0.0f,   Unit::Beats, &P::lengthBeats },
            { "bpm",           "Tempo (manual)", 60.0f,   220.0f,   0.0f,   Unit::Bpm,   &P::bpm },
            { "cutMs",         "Cut",            0.5f,    40.0f,    6.0f,   Unit::Ms,    &P::cutMs },
            { "releaseMs",     "Release",        0.0f,    80.0f,    20.0f,  Unit::Ms,    &P::releaseMs },
            { "releaseLevel",  "Release Level",  0.0f,    1.5f,     0.0f,   Unit::Pct,   &P::releaseLevel },
            { "releaseDrop",   "Release Drop",   0.2f,    1.0f,     0.0f,   Unit::Pct,   &P::releaseDrop },
            { "seed",          "Variation",      1.0f,    64.0f,    0.0f,   Unit::Int,   &P::seed },

            // ---- Click ----
            { "clickLevel",    "Click Level",    0.0f,    1.5f,     0.0f,   Unit::Pct,   &P::clickLevel },
            { "clickDecayMs",  "Click Decay",    0.3f,    20.0f,    3.0f,   Unit::Ms,    &P::clickDecayMs },
            { "clickTone",     "Click Tone",     500.0f,  12000.0f, 3000.0f, Unit::Hz,   &P::clickTone },

            // ---- Punch ----
            { "punchLevel",    "Punch Level",    0.0f,    1.5f,     0.0f,   Unit::Pct,   &P::punchLevel },
            { "punchDrive",    "Punch Drive",    0.0f,    1.0f,     0.0f,   Unit::Pct,   &P::punchDrive },
            { "punchLenMs",    "Punch Length",   30.0f,   400.0f,   150.0f, Unit::Ms,    &P::punchLenMs },
            { "fStart",        "Punch Start",    100.0f,  4000.0f,  900.0f, Unit::Hz,    &P::fStart },
            { "fMid",          "Punch Mid",      60.0f,   800.0f,   200.0f, Unit::Hz,    &P::fMid },
            { "t1Ms",          "Punch Snap",     0.5f,    30.0f,    6.0f,   Unit::Ms,    &P::t1Ms },
            { "t2Ms",          "Punch Sweep",    2.0f,    150.0f,   35.0f,  Unit::Ms,    &P::t2Ms },
            { "punchAsym",     "Punch Asym",     0.0f,    1.0f,     0.0f,   Unit::Pct,   &P::punchAsym },
            { "punchDirt",     "Punch Dirt",     0.0f,    3.0f,     0.6f,   Unit::Pct,   &P::punchDirt },
            { "punchTone",     "Punch Tone",     500.0f,  20000.0f, 4000.0f, Unit::Hz,   &P::punchTone },
            { "punchHpFrom",   "Punch HP",       20.0f,   1000.0f,  150.0f, Unit::Hz,    &P::punchHpFrom },
            { "punchHpMs",     "Punch HP Time",  5.0f,    200.0f,   50.0f,  Unit::Ms,    &P::punchHpMs },

            // ---- Rumble ----
            { "rumbleLevel",   "Rumble Level",   0.0f,    2.0f,     0.0f,   Unit::Pct,   &P::rumbleLevel },
            { "rumbleDrive",   "Rumble Drive",   0.0f,    1.0f,     0.0f,   Unit::Pct,   &P::rumbleDrive },
            { "rumbleSize",    "Rumble Size",    0.5f,    0.99f,    0.0f,   Unit::Pct,   &P::rumbleSize },
            { "rumbleStartMs", "Rumble Start",   0.0f,    200.0f,   0.0f,   Unit::Ms,    &P::rumbleStartMs },
            { "rumbleFeedMs",  "Rumble Feed",    5.0f,    400.0f,   100.0f, Unit::Ms,    &P::rumbleFeedMs },
            { "rumbleTone",    "Rumble Tone",    60.0f,   1000.0f,  250.0f, Unit::Hz,    &P::rumbleTone },
            { "rumbleBright",  "Rumble Bright",  1.0f,    8.0f,     3.0f,   Unit::Mult,  &P::rumbleBright },
            { "rumbleDirt",    "Rumble Dirt",    0.0f,    1.0f,     0.0f,   Unit::Pct,   &P::rumbleDirt },
            { "rumbleDamp",    "Rumble Damp",    0.0f,    1.0f,     0.0f,   Unit::Pct,   &P::rumbleDamp },
            { "rumbleFadeMs",  "Rumble Fade",    1.0f,    50.0f,    10.0f,  Unit::Ms,    &P::rumbleFadeMs },

            // ---- Grit ----
            { "gritLevel",     "Grit Level",     0.0f,    1.5f,     0.0f,   Unit::Pct,   &P::gritLevel },
            { "gritDrive",     "Grit Drive",     0.0f,    1.0f,     0.0f,   Unit::Pct,   &P::gritDrive },
            { "gritNoise",     "Grit Noise",     0.0f,    1.0f,     0.15f,  Unit::Pct,   &P::gritNoise },
            { "gritStartMs",   "Grit Start",     0.0f,    200.0f,   0.0f,   Unit::Ms,    &P::gritStartMs },
            { "gritLenMs",     "Grit Length",    20.0f,   1000.0f,  250.0f, Unit::Ms,    &P::gritLenMs },
            { "gritTail",      "Grit Tail",      0.0f,    1.0f,     0.0f,   Unit::Pct,   &P::gritTail },
            { "gritTone",      "Grit Tone",      500.0f,  16000.0f, 3000.0f, Unit::Hz,   &P::gritTone },
            { "gritWidth",     "Grit Width",     0.0f,    1.5f,     0.0f,   Unit::Pct,   &P::gritWidth },

            // ---- Master ----
            { "masterDrive",   "Master Drive",   0.0f,    1.0f,     0.0f,   Unit::Pct,   &P::masterDrive },
            { "masterLowTone", "Master Bass Tone", 100.0f, 4000.0f, 800.0f, Unit::Hz,    &P::masterLowTone },
            { "outGain",       "Output",         0.0f,    1.5f,     0.0f,   Unit::Gain,  &P::outGain },
        };
        return defs;
    }

    // Sample-Slots je Schicht (Index wie IndustrialEngine::Layer)
    inline const char* layerKey (int layer)
    {
        static const char* keys[4] { "click", "punch", "rumble", "grit" };
        return keys[juce::jlimit (0, 3, layer)];
    }
    inline juce::String srcId   (int layer) { return juce::String ("src_")    + layerKey (layer); }
    inline juce::String sGainId (int layer) { return juce::String ("sgain_")  + layerKey (layer); }
    inline juce::String sPitchId(int layer) { return juce::String ("spitch_") + layerKey (layer); }
    inline juce::String onId    (int layer) { return juce::String ("on_")     + layerKey (layer); }
    inline juce::String sOffId  (int layer) { return juce::String ("soff_")   + layerKey (layer); }
    inline juce::String sDecId  (int layer) { return juce::String ("sdec_")   + layerKey (layer); }
    inline juce::String sDrvId  (int layer) { return juce::String ("sdrv_")   + layerKey (layer); }

    // Weitere Schalter
    constexpr auto syncId     = "sync";       // Host-Tempo verwenden
    constexpr auto keytrackId = "keytrack";   // MIDI-Note bestimmt den Grundton
    constexpr auto keylockId  = "keylock";    // Punch-/Rumble-Samples auf KEY stimmen
    constexpr auto autoplayId = "autoplay";   // beim Blättern in der Bibliothek sofort abspielen

    // ---- Anzeige ----
    inline juce::String noteName (float hz)
    {
        static const char* n[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (hz <= 0.0f) return {};
        const int m = juce::roundToInt (69.0 + 12.0 * std::log2 (hz / 440.0));
        return juce::String (n[((m % 12) + 12) % 12]) + juce::String (m / 12 - 1);
    }

    // Zahl mit fester Nachkommastellen-Zahl (0 → ganze Zahl; juce::String (v, 0) würde volle Präzision schreiben)
    inline juce::String num (float v, int decimals)
    {
        return decimals > 0 ? juce::String (v, decimals) : juce::String (juce::roundToInt (v));
    }

    inline juce::String format (float v, Unit u)
    {
        using juce::String;
        switch (u)
        {
            case Unit::Pct:   return String (juce::roundToInt (v * 100.0f)) + " %";
            case Unit::Hz:    return v >= 1000.0f ? num (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                                                  : num (v, v < 100.0f ? 1 : 0) + " Hz";
            case Unit::Ms:    return v >= 1000.0f ? num (v / 1000.0f, 2) + " s"
                                                  : num (v, v < 10.0f ? 1 : 0) + " ms";
            case Unit::Num:   return num (v, 2);
            case Unit::Beats: return num (v, 2) + (std::abs (v - 1.0f) < 0.005f ? " beat" : " beats");
            case Unit::Semi:  return (v > 0.05f ? "+" : "") + num (std::abs (v) < 0.05f ? 0.0f : v, 1) + " st";
            case Unit::Gain:
            {
                if (v <= 0.0001f) return "-inf dB";
                const float db = juce::Decibels::gainToDecibels (v);
                return (db > 0.05f ? "+" : "") + num (std::abs (db) < 0.05f ? 0.0f : db, 1) + " dB";
            }
            case Unit::Key:   return num (v, 1) + " Hz " + noteName (v);
            case Unit::Bpm:   return num (v, 1) + " BPM";
            case Unit::Int:   return "#" + String (juce::roundToInt (v));
            case Unit::Mult:  return num (v, 2) + "x";
            case Unit::MsBip: return std::abs (v) < 0.05f ? String ("0 ms")
                                                          : (v > 0.0f ? "+" : "-") + num (std::abs (v), std::abs (v) < 10.0f ? 1 : 0) + " ms";
            case Unit::DecayMs: return v >= 999.0f ? String ("AUS") : num (v, 0) + " ms";
        }
        return num (v, 2);
    }

    inline float parse (const juce::String& txt, Unit u)
    {
        auto t = txt.trim().toLowerCase();
        // führende Zahl lesen ("55 Hz A1" → 55); "#3" → 3
        float v = (u == Unit::Int) ? t.retainCharacters ("0123456789").getFloatValue() : t.getFloatValue();
        if (u == Unit::Hz && t.contains ("k"))                        v *= 1000.0f;
        if (u == Unit::Ms && t.endsWith ("s") && ! t.contains ("ms")) v *= 1000.0f;
        if (u == Unit::DecayMs && (t.contains ("aus") || t.contains ("off"))) v = 1000.0f;
        if (u == Unit::Pct)                                           v /= 100.0f;
        if (u == Unit::Gain)                                          v = juce::Decibels::decibelsToGain (v);
        return v;
    }
}
