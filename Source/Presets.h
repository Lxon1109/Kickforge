#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "FactoryPresets.h"
#include "ParamTable.h"

/*
    Preset-Verwaltung.
      • Factory-Presets: aus den Referenz-Fits erzeugt (FactoryPresets.h), setzen ALLE
        Klangparameter und schalten alle Schichten auf SYNTH.
      • User-Presets: XML-Dateien (*.kfp) mit komplettem Plugin-Zustand inkl. Sample-Pfaden
        unter %APPDATA%/Forgeworks/Kickforge/Presets.
*/
namespace kf
{

class PresetManager
{
public:
    struct Entry
    {
        juce::String name, category;
        bool         factory = true;
        int          factoryIndex = -1;
        juce::File   file;
    };

    // Zugriff auf den Zustand des Processors (für User-Presets inkl. Samples)
    std::function<std::unique_ptr<juce::XmlElement>()> getStateXml;
    std::function<void (const juce::XmlElement&)>      setStateXml;

    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

    juce::Array<Entry> getEntries() const
    {
        juce::Array<Entry> out;
        const auto& fp = factoryPresets();
        for (int i = 0; i < (int) fp.size(); ++i)
        {
            Entry e;
            e.name = fp[(size_t) i].name;
            e.category = e.name.upToFirstOccurrenceOf (" ", false, false).toUpperCase();
            e.factoryIndex = i;
            out.add (e);
        }
        for (auto& f : getUserPresetFiles())
        {
            Entry e;
            e.name = f.getFileNameWithoutExtension();
            e.category = "USER";
            e.factory = false;
            e.file = f;
            out.add (e);
        }
        return out;
    }

    bool load (int entryIndex)
    {
        auto entries = getEntries();
        if (! juce::isPositiveAndBelow (entryIndex, entries.size())) return false;
        const auto& e = entries.getReference (entryIndex);
        const bool ok = e.factory ? loadFactory (e.factoryIndex) : loadFromFile (e.file);
        if (ok) { currentIndex = entryIndex; currentName = e.name; }
        return ok;
    }

    bool saveUserPreset (const juce::File& file)
    {
        if (getStateXml == nullptr) return false;
        auto xml = getStateXml();
        if (xml == nullptr || ! xml->writeTo (file)) return false;
        currentName = file.getFileNameWithoutExtension();
        auto entries = getEntries();
        for (int i = 0; i < entries.size(); ++i)
            if (! entries[i].factory && entries[i].file == file) currentIndex = i;
        return true;
    }

    int getCurrentIndex() const              { return currentIndex; }
    juce::String getCurrentName() const      { return currentName; }

    static juce::File getUserDir()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Forgeworks").getChildFile ("Kickforge").getChildFile ("Presets");
    }

    // Wert eines Factory-Presets (für Parameter-Defaults)
    static float factoryValue (int presetIndex, const juce::String& id, float fallback)
    {
        const auto& fp = factoryPresets();
        if (! juce::isPositiveAndBelow (presetIndex, (int) fp.size())) return fallback;
        for (auto& kv : fp[(size_t) presetIndex].values)
            if (id == kv.first) return kv.second;
        return fallback;
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    int currentIndex = 0;
    juce::String currentName { factoryPresets().empty() ? "" : factoryPresets().front().name };

    juce::Array<juce::File> getUserPresetFiles() const
    {
        juce::Array<juce::File> files;
        auto dir = getUserDir();
        if (dir.isDirectory())
            dir.findChildFiles (files, juce::File::findFiles, false, "*.kfp");
        files.sort();
        return files;
    }

    void set (const juce::String& id, float v)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    bool loadFactory (int idx)
    {
        const auto& fp = factoryPresets();
        if (! juce::isPositiveAndBelow (idx, (int) fp.size())) return false;
        for (auto& kv : fp[(size_t) idx].values)
            set (kv.first, kv.second);
        for (int l = 0; l < 4; ++l)          // Factory = reine Synthese
        {
            set (params::srcId (l), 0.0f);
            set (params::sGainId (l), 1.0f);
            set (params::sPitchId (l), 0.0f);
        }
        return true;
    }

    bool loadFromFile (const juce::File& f)
    {
        auto xml = juce::parseXML (f);
        if (xml == nullptr || setStateXml == nullptr) return false;
        setStateXml (*xml);
        return true;
    }
};

} // namespace kf
