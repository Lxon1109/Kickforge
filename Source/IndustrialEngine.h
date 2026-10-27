#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>

/*
    IndustrialEngine — Kick-Synthese nach dem Bauplan echter
    Industrial-Hardtechno-Kicks (gemessen an Teknovault-Referenzen):

      CLICK   0–10 ms   breitbandiger Knack
      PUNCH   0–~200 ms Sinus mit 2-stufigem Pitch-Sweep → Drive-Stack
                        (fast Rechteck), am Ende hart abgeschnitten
      RUMBLE  ab ~70 ms Punch → langer Hall → Drive → Tiefpass → Drive,
                        eingeblendet und bis Beat-Ende konstant gehalten (mono)
      GRIT    ab ~25 ms Rauschen + verzerrte Mitten (200 Hz – 4 kHz), Stereo
      MASTER            Drive + Clipper, Länge = n Beats bei BPM, harter Schnitt

    Läuft intern mit 2x Oversampling. Zwei Stimmen für klickfreies Retriggern.
*/
namespace kf
{
    struct IndustrialParams
    {
        // Global
        float bpm          = 150.0f;
        float lengthBeats  = 1.0f;     // Kick-Länge in Beats
        float cutMs        = 5.0f;     // Ausblende am Ende
        float releaseMs    = 0.0f;     // 0 = harter Schnitt; sonst klingt ein sauberer Sinus nach (Zeitkonstante in ms)
        float releaseLevel = 0.9f;     // Startpegel des Nachklangs
        float releaseDrop  = 0.5f;     // Tonhöhe fällt im Nachklang auf fEnd × Drop
        float seed         = 1.0f;     // Zufalls-Seed für Rauschen → jede Kick identisch, Seed ändern = neue Variante
        float fEnd         = 49.0f;    // Grundton (Hz), z. B. G1

        // Click
        float clickLevel   = 0.45f;
        float clickDecayMs = 2.5f;
        float clickTone    = 2500.0f;  // Hochpass-Frequenz

        // Punch
        float fStart       = 900.0f;   // Start-Pitch
        float fMid         = 200.0f;   // Zwischenstufe
        float t1Ms         = 4.0f;     // Zeitkonstante fStart → fMid
        float t2Ms         = 22.0f;    // Zeitkonstante fMid → fEnd
        float punchLenMs   = 190.0f;
        float punchDrive   = 0.75f;    // 0..1
        float punchAsym    = 0.25f;    // Asymmetrie der Sättigung
        float punchDirt    = 0.0f;     // Rauschen VOR der Verzerrung (Industrial-Dreck)
        float punchLevel   = 1.0f;
        float punchTone    = 16000.0f; // Tiefpass nach der Verzerrung (Hz) – rundet den Punch
        float punchHpFrom  = 20.0f;    // Hochpass am Anfang (Hz), fällt auf 20 Hz → Bass kommt später rein (Harsh-Kicks)
        float punchHpMs    = 60.0f;    // Dauer bis der Hochpass unten ist

        // Rumble
        float rumbleLevel   = 0.85f;
        float rumbleStartMs = 75.0f;
        float rumbleFeedMs  = 60.0f;   // wie lange der Punch in den Hall läuft → kurz = unruhiger Hall-Tail
        float rumbleFadeMs  = 6.0f;
        float rumbleDrive   = 0.7f;
        float rumbleDirt    = 0.0f;    // Rauschen vor Drive 1 → körniger, unruhiger Tail
        float rumbleTone    = 280.0f;  // Tiefpass (Hz)
        float rumbleBright  = 3.0f;    // 2. Tiefpass = Tone × Bright (Obertöne nach Drive 2)
        float rumbleSize    = 0.93f;   // Hallgröße 0..1
        float rumbleDamp    = 0.45f;

        // Grit
        float gritLevel    = 0.35f;
        float gritStartMs  = 25.0f;
        float gritTail     = 0.0f;     // Grit-Pegel nach gritLenMs (0 = aus, 1 = bleibt voll)
        float gritLenMs    = 1000.0f;  // Ende der Grit-Schicht (danach 10 ms Blende) – wie der Top-Layer bei Raw-Kicks
        float gritNoise    = 0.5f;     // 0 = nur verzerrte Mitten, 1 = nur Rauschen
        float gritTone     = 4000.0f;  // Tiefpass (Hz)
        float gritDrive    = 0.6f;
        float gritWidth    = 1.0f;     // Stereobreite 0..1.5

        // Master: 2-Band-Clipper (wie beim Hardtechno-Mastering)
        //   Bass < 200 Hz wird geclippt und danach mit masterLowTone gerundet → druckvoll statt Rechteck-Schnarren
        //   Höhen werden weich gesättigt; zum Schluss eine weiche Pegelgrenze bei 0 dBFS
        float masterDrive   = 0.4f;
        float masterLowTone = 900.0f;  // Tiefpass nach dem Bass-Clipper (Hz)
        float outGain       = 0.95f;

        // Sample-Modus pro Schicht (0 = Synth, 1 = Sample) + Pegel/Tonhöhe
        // Reihenfolge: Click, Punch, Rumble, Grit
        float layerMode[4]        { 0, 0, 0, 0 };
        float sampleGain[4]       { 1, 1, 1, 1 };
        float samplePitchSemi[4]  { 0, 0, 0, 0 };
        float layerOn[4]          { 1, 1, 1, 1 };   // Schicht an/aus (beide Modi)
        float sampleOffsetMs[4]   { 0, 0, 0, 0 };   // < 0: Anfang überspringen, > 0: verzögert starten
        float sampleDecayMs[4]    { 1000, 1000, 1000, 1000 };   // Ausklingen bis -60 dB; >= 1000 = aus
        float sampleDrive[4]      { 0, 0, 0, 0 };   // Sättigung pro Sample-Schicht
    };

    // Ein geladenes Sample (Stereo) mit seiner Original-Abtastrate
    struct LayerSample
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 44100.0;
    };
    using LayerSamplePtr = std::shared_ptr<const LayerSample>;

    class IndustrialEngine
    {
    public:
        IndustrialEngine();
        ~IndustrialEngine();

        void prepare (double sampleRate, int maxBlockSize);
        void reset();

        // Parameter-Snapshot (vom Audio-Thread zu Blockbeginn setzen)
        void setParams (const IndustrialParams& p) { params = p; }
        const IndustrialParams& getParams() const  { return params; }

        // noteHz <= 0 → params.fEnd verwenden
        void trigger (float velocity, float noteHz = 0.0f);

        // Stereo-Rendering, ersetzt den Inhalt von L/R
        void render (float* L, float* R, int numSamples);

        // Nur fürs Debuggen/Visualisieren: einzelne Schichten (vor Master)
        enum Layer { ClickL = 0, PunchL, RumbleL, GritL, NumLayers };
        void setLayerSolo (int layerOrMinus1) { solo = layerOrMinus1; }

        // Sample für eine Schicht setzen (GUI-Thread). nullptr = keins.
        // Wird beim nächsten trigger() vom Audio-Thread übernommen.
        void setLayerSample (int layer, LayerSamplePtr s)
        {
            if (layer >= 0 && layer < NumLayers)
                std::atomic_store (&pendingSamples[layer], std::move (s));
        }

    private:
        struct Voice;
        IndustrialParams params;
        double sr = 44100.0, srOS = 88200.0;
        int maxBlock = 512;
        int solo = -1;
        LayerSamplePtr pendingSamples[NumLayers];

        std::unique_ptr<Voice> voices[2];
        int currentVoice = 0;

        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
        // Master-Filterzustände (L/R): Bandsplit + Rundung nach dem Clipper
        float msIc[2][2] {}, mtIc[2][2] {};
        // Velocity → Ausgangspegel (geglättet), damit der Klangcharakter gleich bleibt
        float outVel = 1.0f, outVelTarget = 1.0f;
        juce::AudioBuffer<float> osScratch;
    };
}
