#include "KickVoice.h"
#include <cstring>

namespace kf
{

KickVoice::KickVoice() = default;

void KickVoice::prepare (double sampleRate, int blockSize)
{
    sr = sampleRate;
    maxBlock = juce::jmax (blockSize, 32);

    clickL.prepare (sampleRate);
    bodyL .prepare (sampleRate);
    tailL .prepare (sampleRate);
    masterEq       .prepare (sampleRate, maxBlock);
    masterReverb   .prepare (sampleRate, maxBlock);
    masterTransient.prepare (sampleRate);

    // 2x Oversampling, 1 Kanal, IIR min latency (günstiger als FIR half-band
    // und für Shaper-Antialiasing völlig ausreichend)
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        1,                                            // numChannels
        1,                                            // factor as log2 → 2^1 = 2x
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        false, false);                                // integer latency, no max quality
    oversampler->initProcessing ((size_t) maxBlock);
    oversampler->reset();
}

void KickVoice::reset()
{
    clickL.reset();
    bodyL .reset();
    tailL .reset();
    masterEq       .reset();
    masterReverb   .reset();
    masterTransient.reset();
    if (oversampler)
        oversampler->reset();
}

void KickVoice::trigger (float velocity)
{
    clickL.trigger (velocity);
    bodyL .trigger (velocity);
    tailL .trigger (velocity);
}

void KickVoice::renderParts (float* c, float* b, float* t, int numSamples)
{
    const size_t bytes = sizeof (float) * (size_t) numSamples;
    std::memset (c, 0, bytes);
    std::memset (b, 0, bytes);
    std::memset (t, 0, bytes);
    clickL.addToBuffer (c, numSamples);
    bodyL .addToBuffer (b, numSamples);
    tailL .addToBuffer (t, numSamples);
}

void KickVoice::render (float* out, int numSamples)
{
    std::memset (out, 0, sizeof (float) * (size_t) numSamples);

    // Layer akkumulieren
    clickL.addToBuffer (out, numSamples);
    bodyL .addToBuffer (out, numSamples);
    tailL .addToBuffer (out, numSamples);

    const float drive  = masterDrive.load();
    const SatMode sm   = masterShaper.load();
    const float gain   = masterGain.load();
    const bool  lim    = limiterOn.load();
    const bool  useOvs = oversampleOn.load() && drive > 0.0001f;

    const float preGain = 1.0f + drive * 7.0f; // 0..+18 dB

    if (useOvs && oversampler != nullptr)
    {
        // AudioBlock-Wrapper über unseren Mono-Buffer
        juce::dsp::AudioBlock<float> block (&out, 1, (size_t) numSamples);
        auto upBlock = oversampler->processSamplesUp (block);

        auto* ovsPtr = upBlock.getChannelPointer (0);
        const int ovsN = (int) upBlock.getNumSamples();

        for (int i = 0; i < ovsN; ++i)
            ovsPtr[i] = applyShaper (ovsPtr[i] * preGain, sm, 8.0f) * 0.9f;

        oversampler->processSamplesDown (block);
    }
    else if (drive > 0.0001f)
    {
        // Shaper ohne OvS
        for (int i = 0; i < numSamples; ++i)
            out[i] = applyShaper (out[i] * preGain, sm, 8.0f) * 0.9f;
    }

    // Master-EQ (3-Band)
    masterEq.processBlock (out, numSamples);

    // Transient Shaper
    masterTransient.processBlock (out, numSamples);

    // Reverb
    masterReverb.processBlock (out, numSamples);

    // Gain + Soft-Clip-Limiter
    for (int i = 0; i < numSamples; ++i)
    {
        float x = out[i] * gain;
        if (lim) x = softClipOutput (x);
        out[i] = x;
    }
}

} // namespace kf
