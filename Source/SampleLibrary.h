#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <vector>

/*
    SampleLibrary — Sample-Bibliothek für die vier Schichten.

    Liest einen oder mehrere Ordner ein (z. B. den Teknovault Kick Builder) und
    ordnet jede Datei einer Schicht zu:
        CLICK   …_CLICK, Ordner "Clicks"
        PUNCH   …_PUNCH, Ordner "Punches"; komplette Kicks (…KICK…) als "Full Kicks"
        RUMBLE  …_RUMBLE / …_TAIL / …_CRUNCH, Ordner "Crunches"/"Rumbles"
        TOP     …_TOP_LAYER, Ordner "Top Layers"
    Zusammengehörige Schichten (KICK_7_PUNCH, KICK_7_TAIL, …) werden zu Kits.
    Für Punch/Rumble wird der Grundton erkannt (für KEY LOCK); Ergebnisse werden
    gecacht. Die Bibliothek wird zwischen allen Plugin-Instanzen geteilt
    (SharedResourcePointer) und im Hintergrund gescannt.

    Die Samples bleiben, wo sie sind — Kickforge speichert nur Pfade.
*/
namespace kf
{
    struct LibEntry
    {
        juce::File   file;
        int          layer = -1;        // 0 Click, 1 Punch, 2 Rumble, 3 Top
        bool         fullKick = false;  // komplette Kick (im Punch-Slot nutzbar)
        juce::String group;             // Anzeige-Gruppe (Unterordner)
        juce::String display;           // Anzeigename ohne Hersteller-Präfix
        juce::String kit;               // "KICK 7", "HEAVY RAW 3" oder leer
        juce::String role;              // PUNCH / RUMBLE / TAIL / CRUNCH / CLICK / TOP
        float        rootHz = 0.0f;     // erkannter Grundton (0 = unbekannt)
    };

    struct LibKit
    {
        juce::String name;
        int entry[4] { -1, -1, -1, -1 };   // Index in getEntries() je Schicht, -1 = fehlt
        float rootHz = 0.0f;
    };

    class SampleLibrary : private juce::Thread
    {
    public:
        SampleLibrary();
        ~SampleLibrary() override;

        // ---- Ordner ----
        juce::StringArray getRoots() const;
        void setRoots (const juce::StringArray& roots);   // speichert + scannt neu
        void addRoot (const juce::File& dir);
        void rescan();

        // ---- Zustand ----
        bool  isScanning() const          { return scanning.load(); }
        juce::uint32 getVersion() const   { return version.load(); }
        juce::String getStatusText() const;

        // ---- Abfragen (Kopien, thread-sicher) ----
        std::vector<LibEntry> getEntries() const;
        std::vector<LibKit>   getKits() const;
        std::vector<int>      entriesForLayer (int layer) const;   // sortiert nach Gruppe + Name
        int  indexOfFile (const juce::File& f) const;
        int  findByFileName (const juce::String& fileName) const;
        LibEntry getEntry (int index) const;

        // ---- Hilfen (auch für Tests) ----
        static bool classify (const juce::File& f, LibEntry& out);
        static float detectRootHz (const juce::AudioBuffer<float>& buf, double sampleRate);
        static juce::StringArray autoDetectRoots();

    private:
        void run() override;
        void loadSettings();
        void saveSettings();

        mutable juce::CriticalSection lock;
        juce::StringArray roots;
        std::vector<LibEntry> entries;
        std::vector<LibKit>   kits;
        std::atomic<bool>     scanning { false };
        std::atomic<bool>     needsAutoDetect { false };
        std::atomic<juce::uint32> version { 1 };
        juce::String          lastError;

        std::unique_ptr<juce::PropertiesFile> settings;
        juce::File cacheFile;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleLibrary)
    };
}
