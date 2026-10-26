#include "KickLayer.h"
#include <cmath>

namespace kf
{

KickLayer::KickLayer (LayerRole r) : role (r)
{
    // Rolle-abhängige Defaults — klingen bei Werkseinstellung
    // schon in Richtung Hardtechno/Schranz
    switch (role)
    {
        case LayerRole::Click:
            mode.store (OscMode::Noise);
            tuneHz.store (2500.0f);          // "Hz" dient hier als Pulsfrequenz
            pitchSemis.store (0.0f);
            pitchDecayMs.store (2.0f);
            ampDecayMs.store (8.0f);         // sehr kurz — nur der Attack
            drive.store (0.4f);
            shaper.store (SatMode::HardClip);
            level.store (0.6f);
            break;

        case LayerRole::Body:
            mode.store (OscMode::Sine);
            tuneHz.store (55.0f);
            pitchSemis.store (36.0f);        // großer Drop für den "Boom"
            pitchDecayMs.store (40.0f);
            ampDecayMs.store (320.0f);
            drive.store (0.45f);
            shaper.store (SatMode::HardClip);// Schranz-Default
            level.store (1.0f);
            break;

        case LayerRole::Tail:
            mode.store (OscMode::Sine);
            tuneHz.store (42.0f);            // tiefer Sub
            pitchSemis.store (4.0f);         // nur minimaler Drop
            pitchDecayMs.store (120.0f);
            ampDecayMs.store (900.0f);       // langes Dancefloor-Rumble
            drive.store (0.1f);
            shaper.store (SatMode::Tanh);    // weich, damit Sub nicht zerflattert
            level.store (0.7f);
            break;
    }
}

void KickLayer::prepare (double sampleRate)
{
    sr = sampleRate;
    filter.prepare (sampleRate);
    reset();
}

void KickLayer::updateFilterIfNeeded()
{
    const auto m = fltModePending.load();
    if (m != lastFltMode) { filter.setMode (m); lastFltMode = m; }

    const float c = fltCutoffPending.load();
    if (std::abs (c - lastFltCutoff) > 0.01f) { filter.setCutoff (c); lastFltCutoff = c; }

    const float r = fltResoPending.load();
    if (std::abs (r - lastFltReso) > 0.001f) { filter.setResonance (r); lastFltReso = r; }
}

void KickLayer::reset()
{
    phase = 0.0;
    ampEnv = pitchEnv = 0.0f;
    noiseLast = 0.0f;
    delaySamplesLeft = 0;
    samplePos = 0.0;
    samplePlaying = false;
    filter.reset();
}

void KickLayer::trigger (float velocity)
{
    velGain  = juce::jlimit (0.0f, 1.0f, velocity);

    // Layer-Delay: Envelopes starten erst nach delaySamplesLeft Samples
    const float ms = juce::jlimit (0.0f, 100.0f, delayMs.load());
    delaySamplesLeft = (int) (ms * 0.001f * sr);

    // Sample-Buffer, den die GUI evtl. neu gesetzt hat, threadsicher übernehmen
    activeSample = std::atomic_load (&pendingSample);
    activeSampleSr = pendingSampleSr.load();

    if (delaySamplesLeft <= 0)
        startEnvelopes();
    else
        ampEnv = pitchEnv = 0.0f;
}

void KickLayer::startEnvelopes()
{
    ampEnv   = 1.0f;
    pitchEnv = 1.0f;
    phase    = 0.0;
    samplePos = 0.0;
    samplePlaying = (activeSample != nullptr && activeSample->getNumSamples() > 0);
}

float KickLayer::msToCoeff (float ms, double sampleRate) noexcept
{
    const float t = juce::jmax (0.001f, ms * 0.001f);
    return std::exp (-6.9077553f / (t * (float) sampleRate));
}

float KickLayer::renderOsc (OscMode m, float curFreq)
{
    const double inc = juce::MathConstants<double>::twoPi * curFreq / sr;
    phase += inc;
    if (phase >= juce::MathConstants<double>::twoPi)
        phase -= juce::MathConstants<double>::twoPi;

    switch (m)
    {
        case OscMode::Sine:
            return std::sin ((float) phase);

        case OscMode::Noise:
            // Rauschen ignoriert Phase — dafür wird's extrem gut vom Amp-Env
            // zu einem Click geformt. curFreq steuert HF-Anteil über ein
            // simples 1-Pol-Hochpass-Verhalten (approximiert via diff).
            {
                const float n = rng.nextFloat() * 2.0f - 1.0f;
                // Hochpass-Mix: höhere curFreq -> mehr Attack-Anteil
                const float mix = juce::jlimit (0.0f, 1.0f, curFreq / 8000.0f);
                const float hp = n - noiseLast;
                noiseLast = n;
                return n * (1.0f - mix) + hp * mix;
            }

        case OscMode::Blip:
            // kurzer Hochton-Puls (Sinus bei curFreq)
            return std::sin ((float) phase);

        case OscMode::Triangle:
            // Dreieck aus Phase ableiten
            {
                const float p01 = (float) (phase / juce::MathConstants<double>::twoPi);
                return 4.0f * std::fabs (p01 - 0.5f) - 1.0f;
            }

        case OscMode::Sample:
            {
                if (! samplePlaying || activeSample == nullptr) return 0.0f;

                // curFreq moduliert die Playback-Rate relativ zur Original-Pitch.
                // tuneHz dient hier als Referenz-"Base-Rate": ohne Pitch-Env
                // spielt das Sample mit Original-Speed; pitchEnv beschleunigt es.
                const double rate = (activeSampleSr / sr) * (curFreq / juce::jmax (10.0f, tuneHz.load()));

                const int   nSamp = activeSample->getNumSamples();
                const int   i0    = (int) samplePos;
                if (i0 >= nSamp - 1) { samplePlaying = false; return 0.0f; }

                const float frac  = (float) (samplePos - i0);
                const float* d    = activeSample->getReadPointer (0);
                const float s     = d[i0] * (1.0f - frac) + d[i0 + 1] * frac;

                samplePos += rate;
                return s;
            }
    }
    return 0.0f;
}

void KickLayer::addToBuffer (float* out, int numSamples)
{
    updateFilterIfNeeded();

    const float ampCoeff   = msToCoeff (ampDecayMs.load(),   sr);
    const float pitchCoeff = msToCoeff (pitchDecayMs.load(), sr);

    // Wenn stumm oder praktisch ausgeklungen (und kein Delay mehr aussteht):
    // Envelopes + Delay-Zähler trotzdem weiterlaufen lassen, damit beim
    // Unmute kein Rest-Puls austritt, aber nichts in den Buffer schreiben.
    const bool silent = muted.load() || level.load() < 0.0001f;
    if (silent || (ampEnv < 1e-6f && delaySamplesLeft <= 0))
    {
        for (int i = 0; i < numSamples; ++i)
        {
            if (delaySamplesLeft > 0)
            {
                if (--delaySamplesLeft == 0)
                    startEnvelopes();
                continue;
            }
            ampEnv   *= ampCoeff;
            pitchEnv *= pitchCoeff;
        }
        return;
    }

    const float baseHz     = tuneHz.load();
    const float pitchRange = pitchSemis.load();
    const float lvl        = level.load() * velGain;
    const float dr         = drive.load();
    const SatMode sm       = shaper.load();
    const OscMode om       = mode.load();

    // Pre-Gain für Shaper (bis +18 dB)
    const float preGain = 1.0f + dr * 7.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Layer-Delay: wenn noch nicht Zeit, warten (und ggf. jetzt starten)
        if (delaySamplesLeft > 0)
        {
            if (--delaySamplesLeft == 0)
                startEnvelopes();
            // Filter trotzdem laufen lassen, damit sein Zustand sauber bleibt
            filter.processSample (0.0f);
            continue;
        }

        // exp2 statt pow(2, x): gleiches Ergebnis, deutlich günstiger pro Sample
        const float curFreq = baseHz * std::exp2 (pitchEnv * pitchRange * (1.0f / 12.0f));

        float s = renderOsc (om, curFreq) * ampEnv;

        if (dr > 0.0001f)
            s = applyShaper (s * preGain, sm, 8.0f) * 0.9f;

        // Filter (Mode::Off bypasst intern)
        s = filter.processSample (s);

        out[i] += s * lvl;

        ampEnv   *= ampCoeff;
        pitchEnv *= pitchCoeff;
    }
}

} // namespace kf
