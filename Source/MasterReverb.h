#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>

/*
    MasterReverb — Wrapper um juce::dsp::Reverb mit atomic-settern.
    Parameter werden pro Block in die Reverb-Struktur geschrieben.
*/
namespace kf
{
    class MasterReverb
    {
    public:
        MasterReverb() = default;

        void prepare (double sampleRate, int blockSize)
        {
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
            reverb.prepare (spec);
            reverb.reset();
            dirty = true;
        }

        void reset() { reverb.reset(); }

        void setOn       (bool b)   { on.store (b); }
        void setRoomSize (float v)  { roomSize.store (v); dirty.store (true); }
        void setDamping  (float v)  { damping .store (v); dirty.store (true); }
        void setWidth    (float v)  { width   .store (v); dirty.store (true); }
        void setWet      (float v)  { wet     .store (v); dirty.store (true); }

        void processBlock (float* data, int numSamples)
        {
            if (! on.load())
                return;

            if (dirty.exchange (false))
            {
                juce::Reverb::Parameters p;
                p.roomSize   = juce::jlimit (0.0f, 1.0f, roomSize.load());
                p.damping    = juce::jlimit (0.0f, 1.0f, damping.load());
                p.width      = juce::jlimit (0.0f, 1.0f, width.load());
                p.wetLevel   = juce::jlimit (0.0f, 1.0f, wet.load());
                p.dryLevel   = 1.0f;
                p.freezeMode = 0.0f;
                reverb.setParameters (p);
            }

            juce::dsp::AudioBlock<float> block (&data, 1, (size_t) numSamples);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            reverb.process (ctx);
        }

    private:
        juce::dsp::Reverb reverb;

        std::atomic<bool>  on       { false };
        std::atomic<bool>  dirty    { true };
        std::atomic<float> roomSize { 0.4f };
        std::atomic<float> damping  { 0.5f };
        std::atomic<float> width    { 1.0f };
        std::atomic<float> wet      { 0.2f };
    };
}
