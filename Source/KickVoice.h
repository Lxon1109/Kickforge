#pragma once

#include "KickLayer.h"
#include "Saturation.h"
#include "MasterEQ.h"
#include "MasterReverb.h"
#include "TransientShaper.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace kf
{
    /*
        KickVoice — orchestriert die drei Layer (Click, Body, Tail)
        und fügt die Master-FX-Chain hinzu:
          Mixer -> OvS(Up) -> Master-Drive -> Master-Shaper -> OvS(Down) -> Soft-Clip -> Gain

        Oversampling (2x, IIR-min-latency) umschließt nur den Shaper —
        das ist die Stelle, an der Hard-Clip/Fold/BitCrush Aliasing erzeugt.
    */
    class KickVoice
    {
    public:
        KickVoice();

        void prepare (double sampleRate, int blockSize);
        void reset();
        void trigger (float velocity);

        // Rendert Mono in `out` (ersetzt den Inhalt).
        void render (float* out, int numSamples);

        // Nur für die Visualisierung: rendert jeden Layer getrennt (vor der
        // Master-Chain) in eigene Buffer. Ersetzt jeweils den Inhalt.
        void renderParts (float* clickOut, float* bodyOut, float* tailOut, int numSamples);

        KickLayer& click() { return clickL; }
        KickLayer& body()  { return bodyL;  }
        KickLayer& tail()  { return tailL;  }

        // Master
        void setMasterDrive  (float d)  { masterDrive.store (d); }
        void setMasterShaper (int s)    { masterShaper.store ((SatMode) s); }
        void setMasterGain   (float g)  { masterGain.store (g); }
        void setLimiterOn    (bool on)  { limiterOn.store (on); }
        void setOversampleOn (bool on)  { oversampleOn.store (on); }

        MasterEQ&         eq()        { return masterEq; }
        MasterReverb&     reverb()    { return masterReverb; }
        TransientShaper&  transient() { return masterTransient; }

    private:
        double sr = 44100.0;
        int    maxBlock = 512;

        KickLayer clickL { LayerRole::Click };
        KickLayer bodyL  { LayerRole::Body  };
        KickLayer tailL  { LayerRole::Tail  };

        std::atomic<float>   masterDrive  { 0.25f };
        std::atomic<SatMode> masterShaper { SatMode::HardClip };
        std::atomic<float>   masterGain   { 0.9f };
        std::atomic<bool>    limiterOn    { true };
        std::atomic<bool>    oversampleOn { true };

        MasterEQ         masterEq;
        MasterReverb     masterReverb;
        TransientShaper  masterTransient;

        // 2x Oversampling für Master-Shaper
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    };
}
