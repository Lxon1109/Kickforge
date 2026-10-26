/*
    Kickforge DSP-Tests — rendert Kicks offline und prüft Grundeigenschaften.
    Build: cmake -DKICKFORGE_BUILD_TESTS=ON ...  → Target "KickforgeTests"
*/
#include "../Source/KickVoice.h"
#include "../Source/SvfFilter.h"
#include "../Source/Saturation.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

static int failures = 0;

#define CHECK(cond, msg) \
    do { if (cond) std::printf ("  [ OK ] %s\n", msg); \
         else { std::printf ("  [FAIL] %s\n", msg); ++failures; } } while (0)

static constexpr double kSr = 44100.0;

static std::vector<float> renderKick (kf::KickVoice& v, int numSamples, int block = 512)
{
    std::vector<float> out ((size_t) numSamples, 0.0f);
    v.trigger (0.95f);
    for (int off = 0; off < numSamples; off += block)
        v.render (out.data() + off, std::min (block, numSamples - off));
    return out;
}

static float peak (const std::vector<float>& d, int from = 0, int to = -1)
{
    if (to < 0) to = (int) d.size();
    float p = 0.0f;
    for (int i = from; i < to; ++i) p = std::max (p, std::fabs (d[(size_t) i]));
    return p;
}

static bool allFinite (const std::vector<float>& d)
{
    for (float x : d) if (! std::isfinite (x)) return false;
    return true;
}

static int firstNonSilent (const std::vector<float>& d, float thresh = 1e-4f)
{
    for (size_t i = 0; i < d.size(); ++i)
        if (std::fabs (d[i]) > thresh) return (int) i;
    return -1;
}

int main()
{
    std::printf ("Kickforge DSP tests\n");

    // --- 1. Default-Kick: hörbar, endlich, Limiter hält < 1.0 ---
    {
        kf::KickVoice v; v.prepare (kSr, 512);
        auto d = renderKick (v, (int) kSr);
        const float pk = peak (d);
        std::printf ("  default peak = %.3f\n", pk);
        CHECK (allFinite (d),            "Default-Kick: keine NaN/Inf");
        CHECK (pk > 0.2f,                "Default-Kick: hörbar (Peak > 0.2)");
        CHECK (pk < 1.0f,                "Default-Kick: Soft-Clip hält Peak < 1.0");
        CHECK (peak (d, (int)(0.9*kSr)) < peak (d, 0, (int)(0.1*kSr)), "Default-Kick: klingt ab");
    }

    // --- 2. Alle Shaper-Modi auf Master + Layer: endlich & begrenzt ---
    for (int s = 0; s < 4; ++s)
    {
        kf::KickVoice v; v.prepare (kSr, 512);
        v.setMasterShaper (s); v.setMasterDrive (1.0f);
        v.body().setShaper (s); v.body().setDrive (1.0f);
        auto d = renderKick (v, (int) (0.5 * kSr));
        char msg[96]; std::snprintf (msg, sizeof msg, "Shaper %d @ max Drive: endlich & Peak < 1.0 (%.3f)", s, peak (d));
        CHECK (allFinite (d) && peak (d) < 1.0f, msg);
    }

    // --- 3. Layer-Delay: Body um 20 ms verzögert, Click/Tail aus ---
    {
        kf::KickVoice v; v.prepare (kSr, 512);
        v.click().setMute (true); v.tail().setMute (true);
        v.setMasterDrive (0.0f);
        v.eq().setOn (false);
        v.body().setDelayMs (20.0f);
        auto d = renderKick (v, (int) (0.3 * kSr));
        const int onset = firstNonSilent (d);
        const int expected = (int) (0.020 * kSr);
        std::printf ("  body onset = %d samples (erwartet ~%d)\n", onset, expected);
        CHECK (onset >= expected - 2 && onset <= expected + 64, "Layer-Delay 20 ms verschiebt den Onset korrekt");
        CHECK (peak (d) > 0.2f, "Layer-Delay: Layer startet nach dem Delay wirklich");
    }

    // --- 4. Layer-Delay bei gemutetem Layer: kein Hänger, kein Output ---
    {
        kf::KickVoice v; v.prepare (kSr, 512);
        v.click().setMute (true); v.tail().setMute (true); v.body().setMute (true);
        v.body().setDelayMs (30.0f);
        auto d = renderKick (v, (int) (0.2 * kSr));
        CHECK (peak (d) < 1e-6f, "Alles gemutet + Delay: Stille");
    }

    // --- 5. Sample-Modus im Click-Layer ---
    {
        kf::KickVoice v; v.prepare (kSr, 512);
        v.body().setMute (true); v.tail().setMute (true);
        v.setMasterDrive (0.0f); v.eq().setOn (false);
        auto& c = v.click();
        c.setMode (4); c.setDrive (0.0f); c.setFilterMode (0);
        c.setAmpDecay (300.0f); c.setPitchAmount (0.0f); c.setTune (100.0f); c.setLevel (1.0f);

        // ohne Sample: still
        auto silent = renderKick (v, 4096);
        CHECK (peak (silent) < 1e-6f, "Sample-Modus ohne Sample: Stille");

        // 1 kHz-Sinus als Test-Sample (200 ms)
        auto buf = std::make_shared<juce::AudioBuffer<float>> (1, (int) (0.2 * kSr));
        for (int i = 0; i < buf->getNumSamples(); ++i)
            buf->setSample (0, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 1000.0f * (float) i / (float) kSr));
        c.setSampleBuffer (buf, kSr);

        auto d = renderKick (v, (int) (0.3 * kSr));
        std::printf ("  sample peak = %.3f\n", peak (d));
        CHECK (allFinite (d),                         "Sample-Modus: endlich");
        CHECK (peak (d, 0, 2000) > 0.1f,              "Sample-Modus: Sample wird abgespielt");
        CHECK (peak (d, (int) (0.25 * kSr)) < 1e-3f,  "Sample-Modus: endet nach Sample-Ende");
    }

    // --- 6. SVF: LP dämpft Höhen, HP dämpft Tiefen ---
    {
        auto rms = [] (kf::FilterMode m, float freq)
        {
            kf::SvfFilter f; f.prepare (kSr); f.setMode (m); f.setCutoff (500.0f); f.setResonance (0.707f);
            double acc = 0.0; const int n = 44100;
            for (int i = 0; i < n; ++i)
            {
                const float y = f.processSample (std::sin (2.0f * juce::MathConstants<float>::pi * freq * (float) i / (float) kSr));
                if (i > n / 2) acc += (double) y * y;
            }
            return std::sqrt (acc / (n / 2));
        };
        CHECK (rms (kf::FilterMode::LowPass, 5000.0f)  < 0.1 * rms (kf::FilterMode::LowPass, 50.0f),  "SVF LP: 5 kHz >20 dB unter 50 Hz");
        CHECK (rms (kf::FilterMode::HighPass, 50.0f)   < 0.1 * rms (kf::FilterMode::HighPass, 5000.0f),"SVF HP: 50 Hz >20 dB unter 5 kHz");
    }

    // --- 7. Reverb + Transient Shaper an: endlich, begrenzt ---
    {
        kf::KickVoice v; v.prepare (kSr, 512);
        v.reverb().setOn (true); v.reverb().setWet (1.0f); v.reverb().setRoomSize (1.0f);
        v.transient().setOn (true); v.transient().setAttack (1.0f); v.transient().setSustain (1.0f);
        auto d = renderKick (v, (int) kSr);
        CHECK (allFinite (d) && peak (d) < 1.0f, "Reverb + Transient @ max: endlich & Peak < 1.0");
    }

    // --- 8. Ungerade Blockgrößen (Hosts schicken auch 37er-Blöcke) ---
    {
        kf::KickVoice v; v.prepare (kSr, 512);
        auto d = renderKick (v, 20000, 37);
        CHECK (allFinite (d) && peak (d) > 0.2f, "Blockgröße 37: funktioniert");
    }

    // --- 9. Performance: Visualizer rendert Summe + Parts (je 1.5 s) im GUI-Thread ---
    {
        kf::KickVoice v; v.prepare (kSr, 1024);
        v.reverb().setOn (true); v.transient().setOn (true);   // Worst Case: alles an
        const int n = (int) (1.5 * kSr);
        std::vector<float> a ((size_t) n), c ((size_t) n), b ((size_t) n), t ((size_t) n);

        const double t0 = juce::Time::getMillisecondCounterHiRes();
        constexpr int runs = 20;
        for (int r = 0; r < runs; ++r)
        {
            v.prepare (kSr, 1024); v.reset(); v.trigger (0.95f);
            for (int off = 0; off < n; off += 1024) v.render (a.data() + off, std::min (1024, n - off));
            v.prepare (kSr, 1024); v.reset(); v.trigger (0.95f);
            for (int off = 0; off < n; off += 1024)
            {
                const int k = std::min (1024, n - off);
                v.renderParts (c.data() + off, b.data() + off, t.data() + off, k);
            }
        }
        const double ms = (juce::Time::getMillisecondCounterHiRes() - t0) / runs;
        // Läuft im Hintergrund-Thread; Budget = flüssige ~10 Updates/s beim Drehen.
        // Auf Cloud-Build-Servern (CI) nur informativ: deren CPUs sind oft
        // deutlich langsamer und geteilt — das soll den Build nicht kippen.
        char msg[110]; std::snprintf (msg, sizeof msg, "Visualizer-Render %.1f ms pro Update (Hintergrund, Budget 50 ms)", ms);
        if (std::getenv ("CI") != nullptr)
            std::printf ("  [INFO] %s — auf CI nicht bewertet\n", msg);
        else
            CHECK (ms < 50.0, msg);
    }

    std::printf ("\n%s — %d Fehler\n", failures == 0 ? "ALLE TESTS GRÜN" : "TESTS FEHLGESCHLAGEN", failures);
    return failures == 0 ? 0 : 1;
}
