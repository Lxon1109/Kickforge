/*
    KickforgeRender — rendert die IndustrialEngine ohne DAW.

    KickforgeRender <out.raw> [name=wert ...] [beats=N] [solo=click|punch|rumble|grit]
      • Ausgabe: 32-Bit-Float, Stereo interleaved, 44.1 kHz
      • beats=1  → eine Kick (1 Beat lang)
      • beats=16 → Loop mit einer Kick pro Beat (4 Takte 4/4)
      • name=wert setzt jeden Parameter aus IndustrialParams, z. B. punchDrive=0.9
    --list gibt alle Parameter mit Default aus.
*/
#include "../Source/IndustrialEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using kf::IndustrialParams;

static std::map<std::string, float IndustrialParams::*> paramTable()
{
    return {
        { "bpm", &IndustrialParams::bpm }, { "lengthBeats", &IndustrialParams::lengthBeats },
        { "cutMs", &IndustrialParams::cutMs }, { "fEnd", &IndustrialParams::fEnd },
        { "releaseMs", &IndustrialParams::releaseMs }, { "releaseLevel", &IndustrialParams::releaseLevel },
        { "releaseDrop", &IndustrialParams::releaseDrop }, { "seed", &IndustrialParams::seed },
        { "clickLevel", &IndustrialParams::clickLevel }, { "clickDecayMs", &IndustrialParams::clickDecayMs },
        { "clickTone", &IndustrialParams::clickTone },
        { "fStart", &IndustrialParams::fStart }, { "fMid", &IndustrialParams::fMid },
        { "t1Ms", &IndustrialParams::t1Ms }, { "t2Ms", &IndustrialParams::t2Ms },
        { "punchLenMs", &IndustrialParams::punchLenMs }, { "punchDrive", &IndustrialParams::punchDrive },
        { "punchAsym", &IndustrialParams::punchAsym }, { "punchTone", &IndustrialParams::punchTone },
        { "punchHpFrom", &IndustrialParams::punchHpFrom }, { "punchHpMs", &IndustrialParams::punchHpMs }, { "gritTail", &IndustrialParams::gritTail }, { "punchLevel", &IndustrialParams::punchLevel },
        { "punchDirt", &IndustrialParams::punchDirt }, { "rumbleDirt", &IndustrialParams::rumbleDirt },
        { "rumbleLevel", &IndustrialParams::rumbleLevel }, { "rumbleStartMs", &IndustrialParams::rumbleStartMs },
        { "rumbleFeedMs", &IndustrialParams::rumbleFeedMs },
        { "rumbleFadeMs", &IndustrialParams::rumbleFadeMs }, { "rumbleDrive", &IndustrialParams::rumbleDrive },
        { "rumbleTone", &IndustrialParams::rumbleTone }, { "rumbleSize", &IndustrialParams::rumbleSize },
        { "rumbleDamp", &IndustrialParams::rumbleDamp }, { "rumbleBright", &IndustrialParams::rumbleBright },
        { "gritLevel", &IndustrialParams::gritLevel }, { "gritStartMs", &IndustrialParams::gritStartMs }, { "gritLenMs", &IndustrialParams::gritLenMs },
        { "gritNoise", &IndustrialParams::gritNoise }, { "gritTone", &IndustrialParams::gritTone },
        { "gritDrive", &IndustrialParams::gritDrive }, { "gritWidth", &IndustrialParams::gritWidth },
        { "masterDrive", &IndustrialParams::masterDrive }, { "masterLowTone", &IndustrialParams::masterLowTone }, { "outGain", &IndustrialParams::outGain },
    };
}

int main (int argc, char** argv)
{
    auto table = paramTable();
    if (argc >= 2 && std::strcmp (argv[1], "--list") == 0)
    {
        IndustrialParams d;
        for (auto& [k, ptr] : table) std::printf ("%s=%g\n", k.c_str(), d.*ptr);
        return 0;
    }
    if (argc < 2) { std::fprintf (stderr, "usage: KickforgeRender out.raw [name=value ...]\n"); return 1; }

    IndustrialParams p;
    int beats = 1, solo = -1;
    double tailMs = 0.0;   // zusätzliche Renderzeit nach dem letzten Beat (für Nachklang)
    std::map<int, std::string> samplePaths;
    const std::map<std::string, int> layerIdx { { "click", 0 }, { "punch", 1 }, { "rumble", 2 }, { "grit", 3 } };
    for (int i = 2; i < argc; ++i)
    {
        std::string a = argv[i];
        auto eq = a.find ('=');
        if (eq == std::string::npos) continue;
        std::string k = a.substr (0, eq), v = a.substr (eq + 1);
        if (k == "beats") { beats = std::atoi (v.c_str()); continue; }
        if (k == "tail")  { tailMs = std::atof (v.c_str()); continue; }

        // Schicht-Optionen: sample.punch=pfad.wav, mode.punch=1, gain.punch=0.8, pitch.punch=-2
        auto dot = k.find ('.');
        if (dot != std::string::npos)
        {
            auto li = layerIdx.find (k.substr (dot + 1));
            if (li == layerIdx.end()) { std::fprintf (stderr, "unbekannte Schicht: %s\n", k.c_str()); return 2; }
            const auto kind = k.substr (0, dot);
            if      (kind == "sample") { samplePaths[li->second] = v; p.layerMode[li->second] = 1.0f; }
            else if (kind == "mode")   p.layerMode[li->second]       = (float) std::atof (v.c_str());
            else if (kind == "gain")   p.sampleGain[li->second]      = (float) std::atof (v.c_str());
            else if (kind == "pitch")  p.samplePitchSemi[li->second] = (float) std::atof (v.c_str());
            else { std::fprintf (stderr, "unbekannte Option: %s\n", k.c_str()); return 2; }
            continue;
        }
        if (k == "solo")
        {
            solo = v == "click" ? 0 : v == "punch" ? 1 : v == "rumble" ? 2 : v == "grit" ? 3 : -1;
            continue;
        }
        auto it = table.find (k);
        if (it == table.end()) { std::fprintf (stderr, "unbekannter Parameter: %s\n", k.c_str()); return 2; }
        p.*(it->second) = (float) std::atof (v.c_str());
    }

    constexpr double sr = 44100.0;
    kf::IndustrialEngine eng;
    eng.prepare (sr, 512);
    eng.setParams (p);
    eng.setLayerSolo (solo);

    if (! samplePaths.empty())
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        for (auto& [layer, path] : samplePaths)
        {
            std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (juce::File (juce::String::fromUTF8 (path.c_str()))));
            if (rd == nullptr) { std::fprintf (stderr, "Sample nicht lesbar: %s\n", path.c_str()); return 4; }
            auto s = std::make_shared<kf::LayerSample>();
            s->sampleRate = rd->sampleRate;
            s->buffer.setSize ((int) juce::jmax (1u, rd->numChannels), (int) rd->lengthInSamples);
            rd->read (&s->buffer, 0, (int) rd->lengthInSamples, 0, true, true);
            eng.setLayerSample (layer, s);
        }
    }

    const int beatLen = (int) std::lround (60.0 / p.bpm * sr);
    const int extra = (int) std::lround (tailMs * 0.001 * sr);
    const int total = beatLen * beats + extra;
    std::vector<float> L ((size_t) total), R ((size_t) total);
    for (int b = 0; b < beats; ++b)
    {
        eng.trigger (1.0f);
        const int off = b * beatLen;
        const int len = beatLen + (b == beats - 1 ? extra : 0);
        for (int done = 0; done < len; done += 512)
        {
            const int n = std::min (512, len - done);
            eng.render (L.data() + off + done, R.data() + off + done, n);
        }
    }

    std::vector<float> inter ((size_t) total * 2);
    for (int i = 0; i < total; ++i) { inter[(size_t) 2 * i] = L[(size_t) i]; inter[(size_t) 2 * i + 1] = R[(size_t) i]; }
    FILE* f = std::fopen (argv[1], "wb");
    if (! f) return 3;
    std::fwrite (inter.data(), sizeof (float), inter.size(), f);
    std::fclose (f);
    return 0;
}
