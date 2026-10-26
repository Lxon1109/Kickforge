#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

/*
    Preset-Verwaltung für Kickforge.

    Factory-Presets sind als Code-definierte Parameter-Maps hinterlegt
    und werden beim Laden in den APVTS-State appliziert.

    User-Presets werden als XML-Dateien im System-Preset-Ordner gespeichert
    (~/AppData/Roaming/Forgeworks/Kickforge/Presets  bzw. Mac-Equivalent).
*/
namespace kf
{

struct ParamSet
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;
};

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& apvtsRef)
        : apvts (apvtsRef)
    {
        buildFactoryPresets();
        ensureUserDirExists();
    }

    // Namen aller Presets: Factory + User, in Reihenfolge
    juce::StringArray getAllPresetNames() const
    {
        juce::StringArray out;
        for (auto& fp : factoryPresets)
            out.add ("[F] " + fp.name);
        for (auto& f : getUserPresetFiles())
            out.add ("[U] " + f.getFileNameWithoutExtension());
        return out;
    }

    bool loadByIndex (int idx)
    {
        if (idx < 0) return false;
        const int nFactory = (int) factoryPresets.size();
        if (idx < nFactory)
            return loadFactory (idx);

        const int uIdx = idx - nFactory;
        auto files = getUserPresetFiles();
        if (uIdx < 0 || uIdx >= files.size()) return false;
        return loadFromFile (files[uIdx]);
    }

    bool saveUserPreset (const juce::String& name)
    {
        auto dir = getUserDir();
        if (! dir.isDirectory()) return false;
        auto file = dir.getChildFile (name + ".kfp");
        auto xml = apvts.copyState().createXml();
        if (xml == nullptr) return false;
        return xml->writeTo (file);
    }

    int getCurrentIndex() const { return currentIndex; }
    void setCurrentIndex (int i) { currentIndex = i; }

    static juce::File getUserDir()
    {
       #if JUCE_WINDOWS
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #else
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #endif
        return base.getChildFile ("Forgeworks").getChildFile ("Kickforge").getChildFile ("Presets");
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<ParamSet> factoryPresets;
    int currentIndex = 0;

    void ensureUserDirExists()
    {
        auto dir = getUserDir();
        if (! dir.exists()) dir.createDirectory();
    }

    juce::Array<juce::File> getUserPresetFiles() const
    {
        juce::Array<juce::File> files;
        auto dir = getUserDir();
        if (dir.isDirectory())
            dir.findChildFiles (files, juce::File::findFiles, false, "*.kfp");
        files.sort();
        return files;
    }

    bool loadFactory (int idx)
    {
        if (idx < 0 || idx >= (int) factoryPresets.size()) return false;
        for (auto& kv : factoryPresets[(size_t) idx].values)
        {
            if (auto* p = apvts.getParameter (kv.first))
            {
                auto range = p->getNormalisableRange();
                p->setValueNotifyingHost (range.convertTo0to1 (kv.second));
            }
        }
        currentIndex = idx;
        return true;
    }

    bool loadFromFile (const juce::File& f)
    {
        auto xml = juce::parseXML (f);
        if (xml == nullptr) return false;
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        return true;
    }

    // -------------------------------------------------------------
    //  FACTORY-PRESETS: Hardtechno / Industrial / Schranz
    //  (nur die wichtigsten Parameter werden gesetzt — der Rest
    //   bleibt auf dem aktuellen Wert)
    // -------------------------------------------------------------
    void buildFactoryPresets()
    {
        factoryPresets = {
            // --- 00  Default: Classic Hardtechno ------------------------
            { "Classic Hardtechno", {
                {"body_tune", 55.0f}, {"body_pamt", 36.0f}, {"body_pdec", 40.0f}, {"body_adec", 320.0f},
                {"body_drv", 0.45f},  {"body_shp", 1.0f},   {"body_lvl", 1.0f},
                {"click_on", 1.0f},   {"click_lvl", 0.6f},  {"click_drv", 0.4f},
                {"tail_on", 1.0f},    {"tail_lvl", 0.7f},
                {"master_drv", 0.25f}, {"master_shp", 1.0f}, {"master_lim", 1.0f}, {"master_ovs", 1.0f}
            }},

            // --- 01  Schranz Brutal ------------------------------------
            { "Schranz Brutal", {
                {"body_tune", 50.0f}, {"body_pamt", 42.0f}, {"body_pdec", 35.0f}, {"body_adec", 280.0f},
                {"body_drv", 0.75f},  {"body_shp", 1.0f},   {"body_lvl", 1.0f},
                {"click_on", 1.0f},   {"click_mode", 1.0f}, {"click_tune", 3500.0f}, {"click_dec", 6.0f},
                {"click_drv", 0.6f},  {"click_shp", 2.0f},  {"click_lvl", 0.8f},
                {"tail_on", 1.0f},    {"tail_tune", 48.0f}, {"tail_adec", 700.0f},  {"tail_lvl", 0.6f},
                {"master_drv", 0.5f}, {"master_shp", 1.0f}, {"master_gain", 1.0f},  {"master_lim", 1.0f}, {"master_ovs", 1.0f}
            }},

            // --- 02  Industrial Rumbler --------------------------------
            { "Industrial Rumbler", {
                {"body_tune", 45.0f}, {"body_pamt", 30.0f}, {"body_pdec", 55.0f}, {"body_adec", 450.0f},
                {"body_drv", 0.6f},   {"body_shp", 2.0f},   {"body_lvl", 0.9f},
                {"click_on", 1.0f},   {"click_drv", 0.5f},  {"click_shp", 2.0f}, {"click_lvl", 0.55f},
                {"tail_on", 1.0f},    {"tail_tune", 38.0f}, {"tail_adec", 1600.0f}, {"tail_lvl", 0.85f},
                {"tail_fmode", 1.0f}, {"tail_fcut", 150.0f},{"tail_fres", 1.4f},
                {"master_drv", 0.4f}, {"master_shp", 2.0f}, {"master_gain", 0.85f}, {"master_lim", 1.0f}, {"master_ovs", 1.0f}
            }},

            // --- 03  Hard Groove Punch ---------------------------------
            { "Hard Groove Punch", {
                {"body_tune", 58.0f}, {"body_pamt", 30.0f}, {"body_pdec", 50.0f}, {"body_adec", 290.0f},
                {"body_drv", 0.35f},  {"body_shp", 0.0f},   {"body_lvl", 1.0f},
                {"click_on", 1.0f},   {"click_tune", 2200.0f}, {"click_dec", 10.0f},
                {"click_drv", 0.3f},  {"click_lvl", 0.55f},
                {"tail_on", 1.0f},    {"tail_adec", 650.0f}, {"tail_lvl", 0.5f},
                {"master_drv", 0.2f}, {"master_shp", 0.0f}, {"master_gain", 0.95f}, {"master_lim", 1.0f}, {"master_ovs", 1.0f}
            }},

            // --- 04  Berghain Deep -------------------------------------
            { "Berghain Deep", {
                {"body_tune", 50.0f}, {"body_pamt", 24.0f}, {"body_pdec", 70.0f}, {"body_adec", 400.0f},
                {"body_drv", 0.2f},   {"body_shp", 0.0f},   {"body_lvl", 1.0f},
                {"click_on", 1.0f},   {"click_lvl", 0.35f}, {"click_drv", 0.2f},
                {"tail_on", 1.0f},    {"tail_tune", 40.0f}, {"tail_adec", 1800.0f}, {"tail_lvl", 0.9f},
                {"master_drv", 0.15f}, {"master_shp", 0.0f}, {"master_gain", 0.9f}, {"master_lim", 1.0f}, {"master_ovs", 1.0f}
            }},

            // --- 05  Lo-Fi Distorted (BitCrush) ------------------------
            { "Lo-Fi Distorted", {
                {"body_tune", 55.0f}, {"body_pamt", 36.0f}, {"body_pdec", 40.0f}, {"body_adec", 300.0f},
                {"body_drv", 0.7f},   {"body_shp", 3.0f},   {"body_lvl", 1.0f},
                {"click_on", 1.0f},   {"click_shp", 3.0f},  {"click_drv", 0.6f}, {"click_lvl", 0.7f},
                {"tail_on", 1.0f},    {"tail_lvl", 0.6f},
                {"master_drv", 0.5f}, {"master_shp", 3.0f}, {"master_gain", 0.85f}, {"master_lim", 1.0f}, {"master_ovs", 1.0f}
            }},

            // --- 06  Hoover Zap (self-oscillating filter) --------------
            { "Hoover Zap", {
                {"body_tune", 65.0f}, {"body_pamt", 50.0f}, {"body_pdec", 60.0f}, {"body_adec", 350.0f},
                {"body_drv", 0.4f},   {"body_shp", 1.0f},   {"body_lvl", 1.0f},
                {"body_fmode", 1.0f}, {"body_fcut", 400.0f},{"body_fres", 12.0f},
                {"click_on", 1.0f},   {"click_lvl", 0.4f},
                {"tail_on", 0.0f},
                {"master_drv", 0.3f}, {"master_shp", 2.0f}, {"master_gain", 0.9f}, {"master_lim", 1.0f}, {"master_ovs", 1.0f}
            }},

            // --- 07  Sub-Only Rumble (Tail-Fokus) ----------------------
            { "Sub-Only Rumble", {
                {"body_tune", 42.0f}, {"body_pamt", 18.0f}, {"body_pdec", 80.0f}, {"body_adec", 500.0f},
                {"body_drv", 0.1f},   {"body_shp", 0.0f},   {"body_lvl", 0.6f},
                {"click_on", 0.0f},
                {"tail_on", 1.0f},    {"tail_tune", 36.0f}, {"tail_adec", 2200.0f}, {"tail_lvl", 1.0f},
                {"master_drv", 0.1f}, {"master_lim", 1.0f}
            }}
        };
    }
};

} // namespace kf
