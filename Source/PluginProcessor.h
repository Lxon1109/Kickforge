#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "KickVoice.h"
#include "Presets.h"

class KickforgeAudioProcessor : public juce::AudioProcessor,
                                public juce::AudioProcessorValueTreeState::Listener
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
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override                     { return 1; }
    int getCurrentProgram() override                  { return 0; }
    void setCurrentProgram (int) override             {}
    const juce::String getProgramName (int) override  { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // APVTS-Listener
    void parameterChanged (const juce::String& id, float newValue) override;

    void triggerAudition() { auditionPending.store (true); }

    // Rendert eine ganze Kick offline in `out` mit der zweiten Voice-Instanz.
    // Verwendet die aktuellen Parameter aus APVTS. Thread-safe für GUI-Aufrufe.
    void renderOfflineKick (juce::AudioBuffer<float>& out, double sampleRate);

    // Rendert Click/Body/Tail getrennt (3 Kanäle, vor der Master-Chain) — für den Visualizer.
    void renderOfflineParts (juce::AudioBuffer<float>& parts, double sampleRate);

    // Peak des zuletzt gerenderten Audio-Blocks (für die Output-Anzeige)
    // Liest den Peak seit dem letzten Aufruf und setzt ihn zurück
    float takeOutputPeak() { return outputPeak.exchange (0.0f); }

    // Lädt eine WAV-Datei und routet sie in den Click-Layer beider Voices.
    bool loadClickSample (const juce::File& file);
    void clearClickSample();
    juce::String getClickSamplePath() const { return clickSamplePath; }

    // Flag für den Editor: wurde ein Parameter seit dem letzten reset geändert?
    bool pollParamsChanged() { return paramsChanged.exchange (false); }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    kf::KickVoice&     getVoice()         { return voice; }
    kf::PresetManager& getPresetManager() { return presets; }

private:
    kf::KickVoice voice;         // Live-Instanz (für processBlock)
    kf::KickVoice offlineVoice;  // Parallel-Instanz (nur für Offline-Render)
    kf::PresetManager presets;

    // Schützt offlineVoice: Visualizer rendert im Hintergrund-Thread,
    // WAV-Export im Message-Thread — nie gleichzeitig.
    juce::CriticalSection offlineLock;

    std::atomic<bool>  auditionPending { false };
    std::atomic<bool>  paramsChanged   { false };
    std::atomic<float> outputPeak      { 0.0f };

    // Click-Sample (shared zwischen den Voices)
    std::shared_ptr<juce::AudioBuffer<float>> clickSampleBuffer;
    double       clickSampleSr   = 44100.0;
    juce::String clickSamplePath;

    void attachAll();
    void detachAll();
    void syncParamToEngine (const juce::String& id, float v, bool toBothVoices);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickforgeAudioProcessor)
};
