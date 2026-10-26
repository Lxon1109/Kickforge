#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>
#include "Saturation.h"
#include "SvfFilter.h"

/*
    KickLayer — ein einzelner Kick-Baustein.
    Rolle wird über `LayerRole` gesetzt. Die Rolle bestimmt die
    Default-Charakteristik (Oszillator-Modus & Sinnvolle Range),
    aber alle Parameter bleiben frei einstellbar.

      • Click: Noise- oder Blip-Oszillator, sehr kurzer Decay, dient
        dem Transient-Durchbruch (für Schranz lebenswichtig).
      • Body : Sinus mit Pitch-Env + eigenem Shaper (Boom).
      • Tail : Sub-Sinus mit langem Decay, optional leichtem Drop
        (Dancefloor-Rumble).
*/
namespace kf
{
    enum class LayerRole { Click, Body, Tail };

    enum class OscMode : int
    {
        Sine     = 0,
        Noise    = 1,   // weißes Rauschen (gefiltert via Shape-Decay)
        Blip     = 2,   // kurzer Hochton-Sinus-Puls (Click-Alternative)
        Triangle = 3,   // schärfer als Sine, mehr Obertöne
        Sample   = 4    // geladene WAV-Datei als Transient-Source
    };

    class KickLayer
    {
    public:
        explicit KickLayer (LayerRole role);

        void prepare (double sampleRate);
        void reset();
        void trigger (float velocity);

        // Mono-Addition in den Zielbuffer (akkumuliert!)
        void addToBuffer (float* out, int numSamples);

        // ---- Setter (atomar, GUI->Audio) ----
        void setMode        (int m)       { mode.store ((OscMode) m); }
        void setTune        (float hz)    { tuneHz.store (hz); }
        void setPitchAmount (float semis) { pitchSemis.store (semis); }
        void setPitchDecay  (float ms)    { pitchDecayMs.store (ms); }
        void setAmpDecay    (float ms)    { ampDecayMs.store (ms); }
        void setDrive       (float d)     { drive.store (d); }
        void setShaper      (int s)       { shaper.store ((SatMode) s); }
        void setLevel       (float lin)   { level.store (lin); }
        void setMute        (bool m)      { muted.store (m); }
        void setPan         (float p)     { pan.store (p); } // -1..1 (für zukünftige Stereo-Nutzung)

        // Filter
        void setFilterMode  (int m)       { fltModePending.store ((FilterMode) m); }
        void setFilterCutoff(float hz)    { fltCutoffPending.store (hz); }
        void setFilterReso  (float r)     { fltResoPending.store (r); }

        // Layer-Delay (ms, 0..100)
        void setDelayMs     (float ms)    { delayMs.store (ms); }

        // Sample-Playback: setzt den (shared) Sample-Buffer. Nullptr = kein Sample.
        // Wird von der GUI aufgerufen; der Audio-Thread übernimmt ihn beim nächsten trigger().
        void setSampleBuffer (std::shared_ptr<juce::AudioBuffer<float>> buf, double bufferSampleRate)
        {
            pendingSampleSr.store (bufferSampleRate);
            std::atomic_store (&pendingSample, std::move (buf));
        }

        LayerRole getRole() const noexcept { return role; }

    private:
        LayerRole role;
        double sr = 44100.0;

        double phase       = 0.0;
        float  ampEnv      = 0.0f;
        float  pitchEnv    = 0.0f;
        float  velGain     = 1.0f;
        float  noiseLast   = 0.0f;  // 1-Pol-HP-Zustand fürs Noise-Osz
        int    delaySamplesLeft = 0;

        // Sample-Playback-State
        // pending*: von der GUI gesetzt (atomic) · active*: nur Audio-Thread
        std::shared_ptr<juce::AudioBuffer<float>> pendingSample;
        std::atomic<double>                       pendingSampleSr { 44100.0 };
        std::shared_ptr<juce::AudioBuffer<float>> activeSample;
        double activeSampleSr = 44100.0;
        double samplePos      = 0.0;
        bool   samplePlaying  = false;

        std::atomic<float> delayMs { 0.0f };

        juce::Random rng;

        // Params
        std::atomic<OscMode> mode        { OscMode::Sine };
        std::atomic<float>   tuneHz      { 55.0f };
        std::atomic<float>   pitchSemis  { 36.0f };
        std::atomic<float>   pitchDecayMs{ 45.0f };
        std::atomic<float>   ampDecayMs  { 350.0f };
        std::atomic<float>   drive       { 0.0f };
        std::atomic<SatMode> shaper      { SatMode::Tanh };
        std::atomic<float>   level       { 1.0f };
        std::atomic<bool>    muted       { false };
        std::atomic<float>   pan         { 0.0f };

        // Filter (pending-Werte werden pro Block in den SVF appliziert)
        SvfFilter            filter;
        std::atomic<FilterMode> fltModePending   { FilterMode::Off };
        std::atomic<float>      fltCutoffPending { 1000.0f };
        std::atomic<float>      fltResoPending   { 0.707f };
        FilterMode              lastFltMode   = FilterMode::Off;
        float                   lastFltCutoff = -1.0f;
        float                   lastFltReso   = -1.0f;

        static float msToCoeff (float ms, double sampleRate) noexcept;
        float renderOsc (OscMode m, float curFreq);
        void  updateFilterIfNeeded();
        void  startEnvelopes();
    };
}
