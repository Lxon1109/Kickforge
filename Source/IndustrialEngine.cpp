#include "IndustrialEngine.h"
#include <cmath>
#include <vector>

namespace kf
{
namespace
{
    constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

    // Zavalishin-TPT-SVF, liefert LP/BP/HP gleichzeitig
    struct Svf
    {
        float g = 0, k = 1.4142f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
        void set (float fc, float q, float sr)
        {
            fc = juce::jlimit (10.0f, sr * 0.45f, fc);
            g = std::tan (juce::MathConstants<float>::pi * fc / sr);
            k = 1.0f / juce::jmax (0.05f, q);
            a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        void reset() { ic1 = ic2 = 0; }
        // gibt LP zurück, bp/hp per Referenz
        inline float process (float x, float& bp, float& hp)
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            bp = v1; hp = x - k * v1 - v2;
            return v2;
        }
        inline float lp (float x) { float b, h; return process (x, b, h); }
        inline float hp (float x) { float b, h; process (x, b, h); return h; }
        inline float bp (float x) { float b, h; process (x, b, h); return b; }
    };

    // weicher Hard-Clip (p-Norm): linear bis ~0.7, dann flach bei ±1
    inline float clipP (float x, float p = 6.0f)
    {
        const float ax = std::abs (x);
        return x / std::pow (1.0f + std::pow (ax, p), 1.0f / p);
    }

    // Foldback für Industrial-Grit
    inline float fold (float x)
    {
        x = std::fmod (x + 1.0f, 4.0f);
        if (x < 0) x += 4.0f;
        return x < 2.0f ? x - 1.0f : 3.0f - x;
    }

    inline float msToSamples (float ms, double sr) { return (float) (ms * 0.001 * sr); }

    // Raised-Cosine-Rampe 0..1
    inline float ramp01 (float x) { x = juce::jlimit (0.0f, 1.0f, x); return 0.5f - 0.5f * std::cos (x * juce::MathConstants<float>::pi); }
}

// =========================================================================
// Voice
// =========================================================================
struct IndustrialEngine::Voice
{
    double sr = 88200.0;
    bool   active = false;
    int64_t t = 0, totalLen = 0, releaseLen = 0;
    float  vel = 1.0f, fEnd = 49.0f;
    double phase = 0.0;
    float  e1 = 1.0f, e2 = 1.0f;

    // Fade-out beim Stehlen der Stimme
    bool  fadingOut = false;
    float fadeGain = 1.0f, fadeStep = 0.0f;

    // Click
    // Zwei Rauschquellen: eine pro Render-Durchgang. So hängt die Zuordnung der Zufallswerte
    // zu den Samples NICHT von der Blockgröße ab → jede Kick ist identisch, egal wo im Block sie startet.
    juce::Random rng, rng2;
    float clickHpState = 0.0f, clickPrev = 0.0f;

    // Rumble
    juce::Reverb reverb;
    std::vector<float> rumbleBuf;
    Svf rumbleLp, rumbleLp2, rumbleHp, punchLp, punchHp;

    // Grit
    Svf gritBp, gritHpL, gritHpR, gritLpL, gritLpR;
    float noiseLpL = 0, noiseLpR = 0;
    std::vector<float> haas; int haasPos = 0, haasLen = 1;

    // Punch-DC
    float dcX = 0, dcY = 0;

    // Dirt-Rauschen (leicht tiefpassgefiltert) für Punch und Rumble
    float dirtLpP = 0, dirtLpR = 0;

    std::vector<float> punchBuf, clickBuf, relBuf;

    // Samples pro Schicht (beim Start übernommen) + Lesepositionen
    LayerSamplePtr samples[NumLayers];
    double samplePos[NumLayers] { 0, 0, 0, 0 };
    int    sampleWait[NumLayers] { 0, 0, 0, 0 };          // Verzögerung (Samples, OS-Rate)
    float  sampleEnv[NumLayers]  { 1, 1, 1, 1 };          // Ausklingen

    explicit Voice (int seed) : rng (seed), rng2 (seed + 7919) {}

    // Lineare Interpolation, Stereo; rate = Schritt pro Ausgabesample
    static inline void readSample (const LayerSample& s, double& pos, double rate, float& l, float& r)
    {
        const int n = s.buffer.getNumSamples();
        const int i0 = (int) std::floor (pos);
        if (i0 < 0 || i0 >= n - 1) { l = r = 0.0f; pos += rate; return; }
        const float fr = (float) (pos - i0);
        const float* c0 = s.buffer.getReadPointer (0);
        const float* c1 = s.buffer.getNumChannels() > 1 ? s.buffer.getReadPointer (1) : c0;
        l = c0[i0] + (c0[i0 + 1] - c0[i0]) * fr;
        r = c1[i0] + (c1[i0 + 1] - c1[i0]) * fr;
        pos += rate;
    }

    void prepare (double sampleRate, int maxBlockOS)
    {
        sr = sampleRate;
        reverb.setSampleRate (sr);
        rumbleBuf.assign ((size_t) maxBlockOS, 0.0f);
        punchBuf .assign ((size_t) maxBlockOS, 0.0f);
        clickBuf .assign ((size_t) maxBlockOS, 0.0f);
        relBuf   .assign ((size_t) maxBlockOS, 0.0f);
        haasLen = (int) (0.020 * sr) + 2;
        haas.assign ((size_t) haasLen, 0.0f);
        reset();
    }

    void reset()
    {
        active = false; fadingOut = false; fadeGain = 1.0f;
        reverb.reset();
        for (auto* f : { &rumbleLp, &rumbleLp2, &rumbleHp, &punchLp, &punchHp, &gritBp, &gritHpL, &gritHpR, &gritLpL, &gritLpR }) f->reset();
        std::fill (haas.begin(), haas.end(), 0.0f); haasPos = 0;
        clickHpState = clickPrev = noiseLpL = noiseLpR = dcX = dcY = dirtLpP = dirtLpR = 0.0f;
    }

    void start (const IndustrialParams& p, float velocity, float noteHz, const LayerSamplePtr* layerSamples)
    {
        reset();
        for (int l = 0; l < NumLayers; ++l)
        {
            samples[l] = layerSamples[l];
            const double srcSr = samples[l] != nullptr ? samples[l]->sampleRate : 44100.0;
            const float off = p.sampleOffsetMs[l];
            samplePos[l]  = off < 0.0f ? (double) -off * 0.001 * srcSr : 0.0;   // Anfang überspringen
            sampleWait[l] = off > 0.0f ? (int) (off * 0.001 * sr) : 0;          // verzögert starten
            sampleEnv[l]  = 1.0f;
        }
        active = true;
        t = 0;
        vel = juce::jlimit (0.0f, 1.0f, velocity);
        fEnd = noteHz > 0.0f ? noteHz : p.fEnd;
        phase = 0.0;
        e1 = e2 = 1.0f;
        const double beatSec = 60.0 / juce::jmax (40.0f, p.bpm);
        totalLen = (int64_t) (beatSec * juce::jlimit (0.125f, 4.0f, p.lengthBeats) * sr);
        releaseLen = p.releaseMs > 0.5f ? (int64_t) (7.0 * p.releaseMs * 0.001 * sr) : 0;
        rng .setSeed ((juce::int64) (p.seed * 1000003.0f) + 17);   // gleiche Kick bei jedem Anschlag
        rng2.setSeed ((juce::int64) (p.seed * 1000003.0f) + 7919);

        // Hall: Parameter setzen und Glättung sofort einrasten lassen (keine Rampen beim Start)
        juce::Reverb::Parameters rp;
        rp.roomSize = juce::jlimit (0.0f, 1.0f, p.rumbleSize);
        rp.damping  = juce::jlimit (0.0f, 1.0f, p.rumbleDamp);
        rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 0.0f; rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        reverb.setSampleRate (sr);
        reverb.reset();
    }

    // Im Nachklang (Körper fertig) → darf beim nächsten Anschlag weiterklingen
    bool inRelease() const { return active && ! fadingOut && t >= totalLen; }

    void startFadeOut (float ms)
    {
        if (! active) return;
        fadingOut = true;
        fadeStep = 1.0f / juce::jmax (1.0f, msToSamples (ms, sr));
    }

    void render (const IndustrialParams& p, int solo, float* outL, float* outR, int n)
    {
        if (! active) return;
        const float srf = (float) sr;

        // ---- Koeffizienten pro Block ----
        const float k1 = std::exp (-1.0f / juce::jmax (1.0f, msToSamples (p.t1Ms, sr)));
        const float k2 = std::exp (-1.0f / juce::jmax (1.0f, msToSamples (p.t2Ms, sr)));
        const float clickK = std::exp (-1.0f / juce::jmax (1.0f, msToSamples (p.clickDecayMs, sr)));
        const float clickHpA = std::exp (-kTwoPi * p.clickTone / srf);

        const float punchLen   = msToSamples (p.punchLenMs, sr);
        const float punchFade  = msToSamples (12.0f, sr);
        const float punchGain  = 1.0f + p.punchDrive * 24.0f;
        const float asymA      = p.punchAsym * 0.6f;
        const float asymOff    = std::tanh (asymA);
        punchLp.set (p.punchTone, 0.6f, srf);
        const bool  useHp  = p.punchHpFrom > 25.0f;
        const float hpTau  = juce::jmax (1.0f, msToSamples (p.punchHpMs, sr)) / 3.0f;

        const float rStart = msToSamples (p.rumbleStartMs, sr);
        const float rFeed  = msToSamples (p.rumbleFeedMs, sr);
        const float rFeedFade = msToSamples (8.0f, sr);
        const float rFade  = juce::jmax (1.0f, msToSamples (p.rumbleFadeMs, sr));
        const float rGain1 = 1.0f + p.rumbleDrive * 60.0f;
        const float rGain2 = 1.5f + p.rumbleDrive * 3.0f;
        rumbleLp .set (p.rumbleTone, 0.8f, srf);
        rumbleLp2.set (p.rumbleTone * p.rumbleBright, 0.7f, srf);   // begrenzt die Obertöne der 2. Drive-Stufe
        rumbleHp .set (28.0f, 0.7f, srf);

        const float gStart = msToSamples (p.gritStartMs, sr);
        const float gFade  = msToSamples (20.0f, sr);
        const float gEnd   = msToSamples (p.gritLenMs, sr);
        const float gEndFade = msToSamples (10.0f, sr);
        gritBp .set (900.0f, 0.6f, srf);
        gritHpL.set (180.0f, 0.7f, srf); gritHpR.set (180.0f, 0.7f, srf);
        gritLpL.set (p.gritTone, 0.7f, srf); gritLpR.set (p.gritTone, 0.7f, srf);
        const float gDrive = 1.0f + p.gritDrive * 6.0f;
        const float noiseA = std::exp (-kTwoPi * 3500.0f / srf);
        const float dirtA  = std::exp (-kTwoPi * 6000.0f / srf);
        const int   haasDelay = (int) (0.009 * sr);

        const float cutLen = juce::jmax (1.0f, msToSamples (p.cutMs, sr));
        const float tEndF  = (float) totalLen;
        const float relStart = tEndF - cutLen;
        const bool  useRelease = releaseLen > 0;
        const float relTau  = juce::jmax (1.0f, msToSamples (p.releaseMs, sr));
        const float relGlideTau = relTau * 1.3f;
        const float relDrop = juce::jlimit (0.2f, 1.0f, p.releaseDrop);
        const float velGain = 1.0f;   // Velocity wirkt auf den Ausgangspegel (Engine::render)

        // Sample-Modus je Schicht
        bool  useSample[NumLayers];
        double sRate[NumLayers];
        for (int l = 0; l < NumLayers; ++l)
        {
            useSample[l] = p.layerMode[l] >= 0.5f && samples[l] != nullptr;
            sRate[l] = useSample[l] ? (samples[l]->sampleRate / sr) * std::exp2 (p.samplePitchSemi[l] / 12.0)
                                    : 0.0;
        }
        // Ausklingen (-60 dB nach decayMs) und Sättigung je Sample-Schicht
        float decMul[NumLayers], sDrv[NumLayers], sDrvG[NumLayers], sDrvN[NumLayers], lvl[NumLayers];
        const float synthLvl[NumLayers] { p.clickLevel, p.punchLevel, p.rumbleLevel, p.gritLevel };
        for (int l = 0; l < NumLayers; ++l)
        {
            const float dMs = p.sampleDecayMs[l];
            decMul[l] = dMs >= 999.0f ? 1.0f : std::exp (-6.9f / juce::jmax (1.0f, msToSamples (dMs, sr)));
            sDrv[l]  = juce::jlimit (0.0f, 1.0f, p.sampleDrive[l]);
            sDrvG[l] = 1.0f + 10.0f * sDrv[l];
            sDrvN[l] = 1.0f / std::tanh (sDrvG[l]);
            lvl[l]   = p.layerOn[l] < 0.5f ? 0.0f : (useSample[l] ? 1.0f : synthLvl[l]);
        }
        auto playSample = [&] (int l, float& L, float& R)
        {
            if (sampleWait[l] > 0) { --sampleWait[l]; L = R = 0.0f; return; }
            readSample (*samples[l], samplePos[l], sRate[l], L, R);
            const float e = sampleEnv[l] * p.sampleGain[l];
            sampleEnv[l] *= decMul[l];
            L *= e; R *= e;
            if (sDrv[l] > 0.0f)
            {
                const float d = sDrv[l];
                L = (1.0f - d) * L + d * std::tanh (sDrvG[l] * L) * sDrvN[l];
                R = (1.0f - d) * R + d * std::tanh (sDrvG[l] * R) * sDrvN[l];
            }
        };

        juce::Reverb::Parameters rp;
        rp.roomSize = juce::jlimit (0.0f, 1.0f, p.rumbleSize);
        rp.damping  = juce::jlimit (0.0f, 1.0f, p.rumbleDamp);
        rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 0.0f; rp.freezeMode = 0.0f;
        reverb.setParameters (rp);

        const int64_t t0 = t;

        // ---- Pass 1: Punch-Oszillator, Click, Hall-Eingang ----
        for (int i = 0; i < n; ++i)
        {
            const float ts = (float) (t0 + i);
            float f = fEnd + (p.fMid - fEnd) * e2 + (p.fStart - p.fMid) * e1;
            float relEnv = 0.0f;
            if (useRelease && ts >= relStart)
            {
                const float dt = ts - relStart;
                f *= relDrop + (1.0f - relDrop) * std::exp (-dt / relGlideTau);   // Tonhöhe fällt weiter
                relEnv = p.releaseLevel * std::exp (-dt / relTau);
            }
            e1 *= k1; e2 *= k2;
            phase += (double) (kTwoPi * f / srf);
            if (phase > juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
            const float s = std::sin ((float) phase);
            relBuf[(size_t) i] = s * relEnv;

            // Punch-Hüllkurve: voll bis (Länge - Fade), dann Kosinus aus
            float pEnv = 1.0f;
            if (ts > punchLen - punchFade)
                pEnv = 1.0f - ramp01 ((ts - (punchLen - punchFade)) / punchFade);
            const float pre = s * pEnv;
            // Hall bekommt nur den Anfang des trockenen Punch → der Tail ist echter Hall-Nachklang
            const float feedEnv = ts < rFeed ? 1.0f : 1.0f - ramp01 ((ts - rFeed) / rFeedFade);
            rumbleBuf[(size_t) i] = pre * feedEnv * 0.5f;

            // Dirt: Rauschen vor der Verzerrung → Intermodulation, "dreckiger" Body
            dirtLpP = (rng.nextFloat() * 2.0f - 1.0f) * (1.0f - dirtA) + dirtA * dirtLpP;
            const float dirty = pre + p.punchDirt * 0.35f * dirtLpP * 3.0f * pEnv;

            float y = std::tanh (punchGain * dirty + asymA) - asymOff;   // asymmetrische Sättigung
            y = clipP (y * 1.25f, 8.0f);                               // Richtung Rechteck
            // DC-Blocker (Asymmetrie erzeugt Gleichanteil)
            const float dc = y - dcX + 0.9995f * dcY; dcX = y; dcY = dc;
            float pOut = punchLp.lp (dc);
            if (useHp)
            {
                if (((t0 + i) & 15) == 0)   // Grenzfrequenz alle 16 Samples nachführen
                    punchHp.set (20.0f + (p.punchHpFrom - 20.0f) * std::exp (-ts / hpTau), 0.707f, srf);
                pOut = punchHp.hp (pOut);
            }
            punchBuf[(size_t) i] = pOut * pEnv;

            // Click: Rausch-Burst mit 1-Pol-Hochpass
            const float env = std::pow (clickK, ts);
            const float nz = rng.nextFloat() * 2.0f - 1.0f;
            const float hp = clickHpA * (clickHpState + nz - clickPrev);
            clickHpState = hp; clickPrev = nz;
            clickBuf[(size_t) i] = hp * env * 1.6f;
        }

        // ---- Hall (Rumble-Quelle) ----
        reverb.processMono (rumbleBuf.data(), n);

        // ---- Pass 2: Rumble, Grit, Summe ----
        for (int i = 0; i < n; ++i)
        {
            const float ts = (float) (t0 + i);

            // Rumble: Hall → Drive → Tiefpass → Drive → Hochpass, per Gate eingeblendet
            dirtLpR = (rng2.nextFloat() * 2.0f - 1.0f) * (1.0f - dirtA) + dirtA * dirtLpR;
            float r = std::tanh ((rumbleBuf[(size_t) i] + p.rumbleDirt * 0.04f * dirtLpR * 3.0f) * rGain1);
            r = rumbleLp.lp (r);
            r = std::tanh (r * rGain2) * 0.95f;
            r = rumbleLp2.lp (r);
            r = rumbleHp.hp (r);
            const float rGate = ts < rStart ? 0.0f : ramp01 ((ts - rStart) / rFade);
            float rumbleL = r * rGate, rumbleR = rumbleL;

            // Samples statt Synthese (Stereo)
            float clickL = clickBuf[(size_t) i], clickR = clickL;
            float punchL = punchBuf[(size_t) i], punchR = punchL;
            if (useSample[ClickL])  playSample (ClickL,  clickL,  clickR);
            if (useSample[PunchL])  playSample (PunchL,  punchL,  punchR);
            if (useSample[RumbleL]) playSample (RumbleL, rumbleL, rumbleR);

            // Grit: verzerrte Mitten (aus Punch+Rumble, ggf. Samples) + Rauschen, Stereo
            const float tonal = fold (gritBp.bp (0.5f * (punchL + punchR) + 0.5f * (rumbleL + rumbleR)) * 3.0f);
            haas[(size_t) haasPos] = tonal;
            const float tonalR = haas[(size_t) ((haasPos - haasDelay + haasLen) % haasLen)];
            haasPos = (haasPos + 1) % haasLen;

            noiseLpL = (rng2.nextFloat() * 2.0f - 1.0f) * (1.0f - noiseA) + noiseA * noiseLpL;
            noiseLpR = (rng2.nextFloat() * 2.0f - 1.0f) * (1.0f - noiseA) + noiseA * noiseLpR;
            const float nm = p.gritNoise;
            float gL = (1.0f - nm) * tonal  + nm * noiseLpL * 4.0f;
            float gR = (1.0f - nm) * tonalR + nm * noiseLpR * 4.0f;
            gL = std::tanh (gritLpL.lp (gritHpL.hp (gL)) * gDrive);
            gR = std::tanh (gritLpR.lp (gritHpR.hp (gR)) * gDrive);
            float gGate = ts < gStart ? 0.0f : ramp01 ((ts - gStart) / gFade);
            if (ts > gEnd) gGate *= 1.0f - (1.0f - juce::jlimit (0.0f, 1.0f, p.gritTail)) * ramp01 ((ts - gEnd) / gEndFade);
            // Breite über Mid/Side
            const float gm = 0.5f * (gL + gR), gs = 0.5f * (gL - gR) * p.gritWidth;
            gL = (gm + gs) * gGate; gR = (gm - gs) * gGate;
            if (useSample[GritL])
                playSample (GritL, gL, gR);

            // Ende: harter Schnitt mit kurzer Blende
            float endGain = 1.0f;
            if (ts > relStart) endGain = 1.0f - ramp01 ((ts - relStart) / cutLen);
            // sauberer Nachklang, während der Schnitt-Blende eingeblendet
            const float rel = relBuf[(size_t) i] * (1.0f - endGain);

            float fg = 1.0f;
            if (fadingOut) { fadeGain = juce::jmax (0.0f, fadeGain - fadeStep); fg = fadeGain; }

            const float g = endGain * fg * velGain;
            float sL = 0.0f, sR = 0.0f;
            if (solo < 0 || solo == ClickL)  { sL += clickL  * lvl[ClickL];  sR += clickR  * lvl[ClickL]; }
            if (solo < 0 || solo == PunchL)  { sL += punchL  * lvl[PunchL];  sR += punchR  * lvl[PunchL]; }
            if (solo < 0 || solo == RumbleL) { sL += rumbleL * lvl[RumbleL]; sR += rumbleR * lvl[RumbleL]; }
            if (solo < 0 || solo == GritL)   { sL += gL * lvl[GritL];        sR += gR * lvl[GritL]; }

            // Nachklang gehört zum synthetischen Punch (bei Punch-Sample oder Punch aus: keiner)
            const bool relOn = ! useSample[PunchL] && p.layerOn[PunchL] >= 0.5f;
            const float relOut = (relOn && (solo < 0 || solo == PunchL)) ? rel * fg * velGain : 0.0f;
            outL[i] += sL * g + relOut;
            outR[i] += sR * g + relOut;
        }

        t += n;
        if (t >= totalLen + releaseLen || (fadingOut && fadeGain <= 0.0f))
            active = false;
    }
};

// =========================================================================
// Engine
// =========================================================================
IndustrialEngine::IndustrialEngine()
{
    voices[0] = std::make_unique<Voice> (0x4b46);
    voices[1] = std::make_unique<Voice> (0x5a17);
}

IndustrialEngine::~IndustrialEngine() = default;

void IndustrialEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    srOS = sampleRate * 2.0;
    maxBlock = juce::jmax (32, maxBlockSize);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampler->initProcessing ((size_t) maxBlock);
    osScratch.setSize (2, maxBlock);

    for (auto& v : voices) v->prepare (srOS, maxBlock * 2);
    reset();
}

void IndustrialEngine::reset()
{
    for (auto& v : voices) v->reset();
    if (oversampler) oversampler->reset();
    for (auto& a : msIc) a[0] = a[1] = 0.0f;
    for (auto& a : mtIc) a[0] = a[1] = 0.0f;
    outVel = outVelTarget = 1.0f;
}

void IndustrialEngine::trigger (float velocity, float noteHz)
{
    LayerSamplePtr active[NumLayers];
    for (int l = 0; l < NumLayers; ++l)
        active[l] = std::atomic_load (&pendingSamples[l]);

    // Stimme im Nachklang darf ausklingen, sonst kurz ausblenden (klickfrei)
    if (! voices[(size_t) currentVoice]->inRelease())
        voices[(size_t) currentVoice]->startFadeOut (4.0f);
    currentVoice = 1 - currentVoice;
    voices[(size_t) currentVoice]->start (params, velocity, noteHz, active);
    outVelTarget = 0.1f + 0.9f * juce::jlimit (0.0f, 1.0f, velocity);
}

void IndustrialEngine::render (float* L, float* R, int numSamples)
{
    int done = 0;
    while (done < numSamples)
    {
        const int n = juce::jmin (maxBlock, numSamples - done);
        float* chans[2] = { L + done, R + done };
        juce::FloatVectorOperations::clear (chans[0], n);
        juce::FloatVectorOperations::clear (chans[1], n);

        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        auto up = oversampler->processSamplesUp (block);
        auto* uL = up.getChannelPointer (0);
        auto* uR = up.getChannelPointer (1);
        const int nOS = (int) up.getNumSamples();
        juce::FloatVectorOperations::clear (uL, nOS);
        juce::FloatVectorOperations::clear (uR, nOS);

        for (auto& v : voices)
            v->render (params, solo, uL, uR, nOS);

        // Master: 2-Band-Clipper (im Oversampling)
        if (solo < 0)
        {
            const float srf = (float) srOS;
            Svf split, tone;
            split.set (200.0f, 0.5f, srf);
            tone.set (params.masterLowTone, 0.6f, srf);
            const float mg  = 1.0f + params.masterDrive * 4.0f;
            const float hg  = 1.0f + params.masterDrive * 1.5f;
            float* ch[2] = { uL, uR };
            for (int c = 0; c < 2; ++c)
            {
                split.ic1 = msIc[c][0]; split.ic2 = msIc[c][1];
                tone.ic1  = mtIc[c][0]; tone.ic2  = mtIc[c][1];
                float* d = ch[c];
                for (int i = 0; i < nOS; ++i)
                {
                    const float x  = d[i];
                    const float lo = split.lp (x);
                    const float hi = x - lo;
                    const float l  = tone.lp (clipP (lo * mg, 5.0f));
                    const float h  = std::tanh (hi * hg);
                    d[i] = clipP ((l + h) * params.outGain, 10.0f);   // weiche Grenze bei 0 dBFS
                }
                msIc[c][0] = split.ic1; msIc[c][1] = split.ic2;
                mtIc[c][0] = tone.ic1;  mtIc[c][1] = tone.ic2;
            }
        }
        else
        {
            juce::FloatVectorOperations::multiply (uL, params.outGain, nOS);
            juce::FloatVectorOperations::multiply (uR, params.outGain, nOS);
        }

        oversampler->processSamplesDown (block);

        // Sicherheitsgrenze nach dem Downsampling (Filter-Überschwinger), dann Velocity-Pegel
        const float velK = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
        for (int i = 0; i < n; ++i)
        {
            outVel += (outVelTarget - outVel) * velK;
            if (solo < 0)
            {
                chans[0][i] = clipP (chans[0][i], 12.0f);
                chans[1][i] = clipP (chans[1][i], 12.0f);
            }
            chans[0][i] *= outVel;
            chans[1][i] *= outVel;
        }
        done += n;
    }
}

} // namespace kf
