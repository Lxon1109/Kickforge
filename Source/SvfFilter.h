#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

/*
    SvfFilter — Topology-Preserving-Transform State-Variable-Filter
    (Zavalishin). Zero-delay-feedback, stabil auch bei hoher Resonanz.
    Mono, sample-accurate.

    Modi: OFF, LP, HP, BP
    Resonance: 0.1 (sehr leicht) .. 20 (self-resonierend).
*/
namespace kf
{
    enum class FilterMode : int { Off = 0, LowPass = 1, HighPass = 2, BandPass = 3 };

    class SvfFilter
    {
    public:
        SvfFilter() = default;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            reset();
            setCutoff (lastCutoff);
            setResonance (lastReso);
        }

        void reset() noexcept { ic1eq = ic2eq = 0.0f; }

        void setCutoff (float hz) noexcept
        {
            lastCutoff = juce::jlimit (10.0f, (float) (sr * 0.49), hz);
            const float wd = 2.0f * juce::MathConstants<float>::pi * lastCutoff;
            const float T  = 1.0f / (float) sr;
            const float wa = (2.0f / T) * std::tan (wd * T * 0.5f);
            g = wa * T * 0.5f;
            recompute();
        }

        void setResonance (float r) noexcept
        {
            lastReso = juce::jlimit (0.1f, 20.0f, r);
            k = 1.0f / lastReso;
            recompute();
        }

        void setMode (FilterMode m) noexcept { mode = m; }
        FilterMode getMode() const noexcept  { return mode; }

        // Prozessiert ein einzelnes Sample. Bei Mode::Off bypasst.
        float processSample (float x) noexcept
        {
            if (mode == FilterMode::Off)
                return x;

            // TPT-SVF nach Zavalishin: v3 = x - ic2eq ; v1 = a1*ic1eq + a2*v3 ; v2 = ic2eq + a2*ic1eq + a3*v3
            const float v3 = x - ic2eq;
            const float v1 = a1 * ic1eq + a2 * v3;
            const float v2 = ic2eq + a2 * ic1eq + a3 * v3;

            ic1eq = 2.0f * v1 - ic1eq;
            ic2eq = 2.0f * v2 - ic2eq;

            switch (mode)
            {
                case FilterMode::LowPass:  return v2;
                case FilterMode::BandPass: return v1;
                case FilterMode::HighPass: return x - k * v1 - v2;
                default: return x;
            }
        }

    private:
        void recompute() noexcept
        {
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        double sr = 44100.0;

        // Koeffizienten
        float g = 0.0f, k = 1.0f;
        float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;

        // Zustand
        float ic1eq = 0.0f, ic2eq = 0.0f;

        // Gemerkte Werte fürs Re-prepare
        float lastCutoff = 1000.0f;
        float lastReso   = 0.707f;

        FilterMode mode = FilterMode::Off;
    };
}
