#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "IndustrialEngine.h"
#include "ParamTable.h"
#include "Presets.h"

/*
    Kickforge v0.7 — Processor
      • IndustrialEngine (Click / Punch / Rumble / Grit / Master)
      • Länge in Beats, synchron zum Host-Tempo (oder manuelles Tempo)
      • MIDI: jede Note triggert, sample-genau; optional bestimmt die Note den Grundton
      • Pro Schicht ein Sample-Slot (Synth ↔ Sample)
*/
class KickforgeAudioProcessor : public juce::AudioProcessor
{
public:
    KickforgeAudioProcessor();
    ~KickforgeAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Kickforge"; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override                     { return 1; }
    int getCurrentProgram() override                  { return 0; }
    void setCurrentProgram (int) override             {}
    const juce::String getProgramName (int) override  { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Bedienung aus dem Editor ----
    void triggerAudition() { auditionPending.store (true); }

    // Aktueller Parameter-Snapshot (Tempo = Host-Tempo, falls Sync an und bekannt)
    kf::IndustrialParams buildParams() const;
    double getEffectiveBpm() const;
    bool isUsingHostTempo() const { return syncRaw->load() >= 0.5f && hostBpm.load() > 20.0; }

    // Offline-Render (GUI-Thread oder Hintergrund-Thread, nie parallel):
    // eine Kick ab Sample 0, Stereo. soloLayer = -1 → komplette Kick inkl. Master.
    void renderOffline (juce::AudioBuffer<float>& out, double sampleRate, int soloLayer = -1);
    // Länge einer Kick inkl. Nachklang in Samples
    int kickLengthSamples (double sampleRate) const;

    // ---- Sample-Slots ----
    bool loadLayerSample (int layer, const juce::File& file);   // schaltet die Schicht auf SAMPLE
    void clearLayerSample (int layer);                          // schaltet zurück auf SYNTH
    juce::String getLayerSamplePath (int layer) const;

    // Peak seit dem letzten Aufruf (Output-Anzeige)
    float takeOutputPeak() { return outputPeak.exchange (0.0f); }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    kf::PresetManager& getPresetManager() { return presets; }

    // Wird bei jeder Änderung hochgezählt (Parameter, Samples) → Editor rendert Visualizer neu
    juce::uint32 getChangeCounter() const { return changeCounter.load(); }

private:
    kf::IndustrialEngine engine;          // Live (Audio-Thread)
    kf::IndustrialEngine offlineEngine;   // Visualizer / Export
    juce::CriticalSection offlineLock;
    double offlineSr = 0.0;

    kf::PresetManager presets;

    std::vector<std::atomic<float>*> engineRaw;   // parallel zu kf::params::engineDefs()
    std::atomic<float>* srcRaw[4] {};
    std::atomic<float>* sGainRaw[4] {};
    std::atomic<float>* sPitchRaw[4] {};
    std::atomic<float>* syncRaw = nullptr;
    std::atomic<float>* keytrackRaw = nullptr;

    std::atomic<double> hostBpm { 0.0 };
    std::atomic<bool>   auditionPending { false };
    std::atomic<float>  outputPeak { 0.0f };
    std::atomic<juce::uint32> changeCounter { 1 };

    // Sample-Slots (geschrieben im Message-Thread, gelesen auch vom Visualizer-Thread)
    kf::LayerSamplePtr layerSamples[4];
    juce::String       layerSamplePaths[4];
    mutable juce::SpinLock samplesLock;

    std::vector<float> monoScratch;   // rechter Kanal bei Mono-Ausgang

    struct ParamWatcher;
    std::unique_ptr<ParamWatcher> watcher;

    void setParamValue (const juce::String& id, float value);
    bool loadLayerSampleInternal (int layer, const juce::File& file, bool switchToSample);

public:
    // Zustand als XML (auch für User-Presets)
    std::unique_ptr<juce::XmlElement> createStateXml();
    void applyStateXml (const juce::XmlElement& xml);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickforgeAudioProcessor)
};
