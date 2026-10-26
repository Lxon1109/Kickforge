#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>

/*
    MasterEQ — 3-Band parametrischer EQ:
      • Low Shelf   (freq + gain)
      • Mid Peak    (freq + gain + Q)
      • High Shelf  (freq + gain)

    Werte werden per std::atomic gesetzt und pro Block appliziert
    (Koeffizienten-Recompute nur bei tatsächlicher Änderung).
*/
namespace kf
{
    class MasterEQ
    {
    public:
        MasterEQ() = default;

        void prepare (double sampleRate, int blockSize)
        {
            sr = sampleRate;
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
            lowShelf.prepare (spec);
            midPeak .prepare (spec);
            highShelf.prepare (spec);
            dirty = true;
        }

        void reset()
        {
            lowShelf.reset();
            midPeak .reset();
            highShelf.reset();
        }

        void setOn       (bool b)   { on.store (b); }
        void setLowFreq  (float f)  { lowFreq.store (f);  markDirty(); }
        void setLowGain  (float dB) { lowGain.store (dB); markDirty(); }
        void setMidFreq  (float f)  { midFreq.store (f);  markDirty(); }
        void setMidGain  (float dB) { midGain.store (dB); markDirty(); }
        void setMidQ     (float q)  { midQ   .store (q);  markDirty(); }
        void setHighFreq (float f)  { highFreq.store (f); markDirty(); }
        void setHighGain (float dB) { highGain.store (dB);markDirty(); }

        void processBlock (float* data, int numSamples)
        {
            if (! on.load())
                return;

            if (dirty.exchange (false))
                recompute();

            juce::dsp::AudioBlock<float> block (&data, 1, (size_t) numSamples);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            lowShelf .process (ctx);
            midPeak  .process (ctx);
            highShelf.process (ctx);
        }

    private:
        void markDirty() { dirty.store (true); }

        void recompute()
        {
            using Coeffs = juce::dsp::IIR::Coefficients<float>;

            *lowShelf .state = *Coeffs::makeLowShelf  (sr, lowFreq.load(),  0.707f, juce::Decibels::decibelsToGain (lowGain.load()));
            *midPeak  .state = *Coeffs::makePeakFilter(sr, midFreq.load(),  midQ.load(), juce::Decibels::decibelsToGain (midGain.load()));
            *highShelf.state = *Coeffs::makeHighShelf (sr, highFreq.load(), 0.707f, juce::Decibels::decibelsToGain (highGain.load()));
        }

        double sr = 44100.0;

        using FilterType = juce::dsp::ProcessorDuplicator<
            juce::dsp::IIR::Filter<float>,
            juce::dsp::IIR::Coefficients<float>>;

        FilterType lowShelf, midPeak, highShelf;

        std::atomic<bool>  on       { true };
        std::atomic<bool>  dirty    { true };

        std::atomic<float> lowFreq  { 80.0f };
        std::atomic<float> lowGain  { 0.0f };
        std::atomic<float> midFreq  { 800.0f };
        std::atomic<float> midGain  { 0.0f };
        std::atomic<float> midQ     { 0.707f };
        std::atomic<float> highFreq { 6000.0f };
        std::atomic<float> highGain { 0.0f };
    };
}
