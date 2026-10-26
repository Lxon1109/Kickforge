#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <atomic>

/*
    TransientShaper — 2-Envelope-Follower (fast + slow).
    attack >  0 : Transient wird betont
    attack <  0 : Transient wird abgeschwächt
    sustain > 0 : Sustain-Body wird betont
    sustain < 0 : Sustain-Body wird abgeschwächt

    Implementierung: die Differenz zwischen schneller und langsamer
    Hüllkurve bestimmt wo wir gerade sind: positive Differenz = Attack,
    negative = Decay/Sustain. Beide werden separat verstärkt.
*/
namespace kf
{
    class TransientShaper
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            fastCoeffAtk = msToCoeff (1.0f);
            fastCoeffRel = msToCoeff (10.0f);
            slowCoeffAtk = msToCoeff (20.0f);
            slowCoeffRel = msToCoeff (200.0f);
            reset();
        }

        void reset() { fastEnv = slowEnv = 0.0f; }

        void setOn      (bool b)   { on.store (b); }
        void setAttack  (float a)  { attackAmt .store (a); } // -1 .. +1
        void setSustain (float s)  { sustainAmt.store (s); }

        void processBlock (float* data, int numSamples)
        {
            if (! on.load())
                return;

            const float a = attackAmt .load();
            const float s = sustainAmt.load();

            for (int i = 0; i < numSamples; ++i)
            {
                const float x = data[i];
                const float absX = std::fabs (x);

                // Fast envelope
                const float fc = (absX > fastEnv) ? fastCoeffAtk : fastCoeffRel;
                fastEnv = fastEnv + (1.0f - fc) * (absX - fastEnv);
                // Slow envelope
                const float sc = (absX > slowEnv) ? slowCoeffAtk : slowCoeffRel;
                slowEnv = slowEnv + (1.0f - sc) * (absX - slowEnv);

                // Differenz: positiv = Attack, negativ = Sustain
                const float diff = fastEnv - slowEnv;
                // Gain-Modulation
                float gain = 1.0f;
                if (diff > 0.0f) gain += a * (diff * 8.0f);
                else             gain += s * (-diff * 4.0f);
                gain = juce::jlimit (0.1f, 4.0f, gain);

                data[i] = x * gain;
            }
        }

    private:
        float msToCoeff (float ms) const
        {
            // 1-Pol-Smoothing-Koeffizient (0..1): näher an 1 = langsamer
            const float t = juce::jmax (0.001f, ms * 0.001f);
            return std::exp (-1.0f / (t * (float) sr));
        }

        double sr = 44100.0;
        float fastEnv = 0.0f, slowEnv = 0.0f;
        float fastCoeffAtk = 0.0f, fastCoeffRel = 0.0f;
        float slowCoeffAtk = 0.0f, slowCoeffRel = 0.0f;

        std::atomic<bool>  on         { false };
        std::atomic<float> attackAmt  { 0.0f };
        std::atomic<float> sustainAmt { 0.0f };
    };
}
