#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

using namespace kf::params;

// =========================================================================
// Parameter-Beobachter: jede Änderung zählt den Change-Counter hoch
// (der Editor rendert daraufhin den Visualizer neu)
// =========================================================================
struct KickforgeAudioProcessor::ParamWatcher : juce::AudioProcessorParameter::Listener
{
    explicit ParamWatcher (KickforgeAudioProcessor& p) : owner (p)
    {
        for (auto* prm : owner.getParameters()) prm->addListener (this);
    }
    ~ParamWatcher() override
    {
        for (auto* prm : owner.getParameters()) prm->removeListener (this);
    }
    void parameterValueChanged (int, float) override        { owner.changeCounter.fetch_add (1); }
    void parameterGestureChanged (int, bool) override       {}
    KickforgeAudioProcessor& owner;
};

// =========================================================================
// Parameter-Layout
// =========================================================================
static juce::AudioParameterFloatAttributes attrs (Unit u)
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([u] (float v, int) { return format (v, u); })
        .withValueFromStringFunction ([u] (const juce::String& s) { return parse (s, u); });
}

juce::AudioProcessorValueTreeState::ParameterLayout KickforgeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    for (auto& d : engineDefs())
    {
        juce::NormalisableRange<float> r (d.min, d.max, d.unit == Unit::Int ? 1.0f : 0.0f);
        if (d.skewCentre > 0.0f) r.setSkewForCentre (d.skewCentre);
        const float def = kf::PresetManager::factoryValue (0, d.id, 0.5f * (d.min + d.max));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { d.id, 1 }, d.name, r,
                                                                  juce::jlimit (d.min, d.max, def), attrs (d.unit)));
    }

    const char* layerNames[4] { "Click", "Punch", "Rumble", "Grit" };
    for (int l = 0; l < 4; ++l)
    {
        const juce::String ln (layerNames[l]);
        p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { srcId (l), 1 }, ln + " Source",
                                                                   juce::StringArray { "Synth", "Sample" }, 0));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { sGainId (l), 1 }, ln + " Sample Gain",
                                                                  juce::NormalisableRange<float> (0.0f, 2.0f, 0.0f, 0.5f), 1.0f,
                                                                  attrs (Unit::Gain)));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { sPitchId (l), 1 }, ln + " Sample Pitch",
                                                                  juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
                                                                  attrs (Unit::Semi)));
    }

    for (int l = 0; l < 4; ++l)
    {
        const juce::String ln (layerNames[l]);
        p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { onId (l), 1 }, ln + " On", true));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { sOffId (l), 1 }, ln + " Sample Offset",
                                                                  juce::NormalisableRange<float> (-50.0f, 100.0f, 0.0f), 0.0f,
                                                                  attrs (Unit::MsBip)));
        juce::NormalisableRange<float> dec (20.0f, 1000.0f, 0.0f);
        dec.setSkewForCentre (250.0f);
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { sDecId (l), 1 }, ln + " Sample Decay",
                                                                  dec, 1000.0f, attrs (Unit::DecayMs)));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { sDrvId (l), 1 }, ln + " Sample Drive",
                                                                  juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.0f,
                                                                  attrs (Unit::Pct)));
    }

    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { syncId, 1 },     "Tempo Sync", true));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { keytrackId, 1 }, "Key Track",  false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { keylockId, 1 },  "Key Lock",   false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { autoplayId, 1 }, "Auto Play",  true));

    return { p.begin(), p.end() };
}

// =========================================================================
KickforgeAudioProcessor::KickforgeAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout()),
      presets (apvts)
{
    for (auto& d : engineDefs())
        engineRaw.push_back (apvts.getRawParameterValue (d.id));
    for (int l = 0; l < 4; ++l)
    {
        srcRaw[l]    = apvts.getRawParameterValue (srcId (l));
        sGainRaw[l]  = apvts.getRawParameterValue (sGainId (l));
        sPitchRaw[l] = apvts.getRawParameterValue (sPitchId (l));
    }
    syncRaw     = apvts.getRawParameterValue (syncId);
    keytrackRaw = apvts.getRawParameterValue (keytrackId);
    keylockRaw  = apvts.getRawParameterValue (keylockId);
    autoplayRaw = apvts.getRawParameterValue (autoplayId);
    fEndRaw     = apvts.getRawParameterValue ("fEnd");
    for (int l = 0; l < 4; ++l)
    {
        onRaw[l]   = apvts.getRawParameterValue (onId (l));
        sOffRaw[l] = apvts.getRawParameterValue (sOffId (l));
        sDecRaw[l] = apvts.getRawParameterValue (sDecId (l));
        sDrvRaw[l] = apvts.getRawParameterValue (sDrvId (l));
    }

    presets.getStateXml = [this] { return createStateXml(); };
    presets.setStateXml = [this] (const juce::XmlElement& x) { applyStateXml (x); };

    watcher = std::make_unique<ParamWatcher> (*this);
}

KickforgeAudioProcessor::~KickforgeAudioProcessor() = default;

// =========================================================================
// Parameter → Engine
// =========================================================================
double KickforgeAudioProcessor::getEffectiveBpm() const
{
    const double host = hostBpm.load();
    if (syncRaw->load() >= 0.5f && host > 20.0)
        return host;
    for (size_t i = 0; i < engineDefs().size(); ++i)
        if (engineDefs()[i].field == &kf::IndustrialParams::bpm)
            return engineRaw[i]->load();
    return 150.0;
}

kf::IndustrialParams KickforgeAudioProcessor::buildParams() const
{
    kf::IndustrialParams p;
    const auto& defs = engineDefs();
    for (size_t i = 0; i < defs.size(); ++i)
        p.*(defs[i].field) = engineRaw[i]->load();
    p.bpm = (float) getEffectiveBpm();
    for (int l = 0; l < 4; ++l)
    {
        p.layerMode[l]       = srcRaw[l]->load();
        p.sampleGain[l]      = sGainRaw[l]->load();
        p.samplePitchSemi[l] = sPitchRaw[l]->load() + getKeyLockShift (l);
        p.layerOn[l]         = onRaw[l]->load();
        p.sampleOffsetMs[l]  = sOffRaw[l]->load();
        p.sampleDecayMs[l]   = sDecRaw[l]->load();
        p.sampleDrive[l]     = sDrvRaw[l]->load();
    }
    return p;
}

int KickforgeAudioProcessor::kickLengthSamples (double sampleRate) const
{
    const auto p = buildParams();
    const double sec = 60.0 / juce::jmax (40.0f, p.bpm) * p.lengthBeats
                     + (p.releaseMs > 0.5f ? 7.0 * p.releaseMs * 0.001 : 0.0) + 0.02;
    return (int) std::ceil (sec * sampleRate);
}

// =========================================================================
// Audio
// =========================================================================
void KickforgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    monoScratch.assign ((size_t) juce::jmax (samplesPerBlock, 32), 0.0f);
    for (int l = 0; l < 4; ++l)
    {
        const juce::SpinLock::ScopedLockType sl (samplesLock);
        engine.setLayerSample (l, layerSamples[l]);
    }
}

bool KickforgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void KickforgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n <= 0) return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                hostBpm.store (*bpm);

    engine.setParams (buildParams());

    float* L = buffer.getWritePointer (0);
    float* R = nullptr;
    if (buffer.getNumChannels() > 1)
        R = buffer.getWritePointer (1);
    else
    {
        if ((int) monoScratch.size() < n) monoScratch.resize ((size_t) n);   // nur wenn der Host größere Blöcke schickt
        R = monoScratch.data();
    }

    int pos = 0;
    auto renderTo = [&] (int end)
    {
        end = juce::jlimit (0, n, end);
        if (end > pos) { engine.render (L + pos, R + pos, end - pos); pos = end; }
    };

    if (auditionPending.exchange (false))
        engine.trigger (1.0f);

    const bool keytrack = keytrackRaw->load() >= 0.5f;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isNoteOn()) continue;
        renderTo (meta.samplePosition);
        float hz = 0.0f;
        if (keytrack)
        {
            hz = (float) juce::MidiMessage::getMidiNoteInHertz (msg.getNoteNumber());
            while (hz >= 80.0f) hz *= 0.5f;      // in den Kick-Bereich 40..80 Hz falten
            while (hz < 40.0f)  hz *= 2.0f;
        }
        engine.trigger (msg.getFloatVelocity(), hz);
    }
    renderTo (n);

    if (buffer.getNumChannels() == 1)
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    const float pk = buffer.getMagnitude (0, n);
    if (pk > outputPeak.load()) outputPeak.store (pk);
}

// =========================================================================
// Offline-Render (Visualizer + Export)
// =========================================================================
void KickforgeAudioProcessor::renderOffline (juce::AudioBuffer<float>& out, double sampleRate, int soloLayer)
{
    const juce::ScopedLock sl (offlineLock);
    constexpr int block = 512;
    if (std::abs (offlineSr - sampleRate) > 0.5)
    {
        offlineEngine.prepare (sampleRate, block);
        offlineSr = sampleRate;
    }
    {
        const juce::SpinLock::ScopedLockType ssl (samplesLock);
        for (int l = 0; l < 4; ++l) offlineEngine.setLayerSample (l, layerSamples[l]);
    }
    offlineEngine.reset();
    offlineEngine.setParams (buildParams());
    offlineEngine.setLayerSolo (soloLayer);
    offlineEngine.trigger (1.0f);

    out.clear();
    const int n = out.getNumSamples();
    float* L = out.getWritePointer (0);
    std::vector<float> tmp;
    float* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : (tmp.resize ((size_t) n), tmp.data());
    for (int off = 0; off < n; off += block)
        offlineEngine.render (L + off, R + off, juce::jmin (block, n - off));
}

// =========================================================================
// Sample-Slots
// =========================================================================
void KickforgeAudioProcessor::setParamValue (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

bool KickforgeAudioProcessor::loadLayerSampleInternal (int layer, const juce::File& file, bool switchToSample)
{
    if (! juce::isPositiveAndBelow (layer, 4)) return false;
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;

    // bis zu 4 Sekunden, Stereo (Mono wird dupliziert)
    const int len = (int) juce::jmin ((juce::int64) (reader->sampleRate * 4.0), reader->lengthInSamples);
    auto s = std::make_shared<kf::LayerSample>();
    s->sampleRate = reader->sampleRate;
    s->buffer.setSize (2, len);
    reader->read (&s->buffer, 0, len, 0, true, true);
    if (reader->numChannels == 1)
        s->buffer.copyFrom (1, 0, s->buffer, 0, 0, len);

    {
        const juce::SpinLock::ScopedLockType sl (samplesLock);
        layerSamples[layer] = s;
        layerSamplePaths[layer] = file.getFullPathName();
    }
    // Grundton: aus der Bibliothek (Kit-Tonart) oder direkt erkennen
    float root = 0.0f;
    const int libIdx = library->indexOfFile (file);
    if (libIdx >= 0) root = library->getEntry (libIdx).rootHz;
    else if (layer == 1 || layer == 2) root = kf::SampleLibrary::detectRootHz (s->buffer, s->sampleRate);
    layerRoot[layer].store (root);
    pendingRelink[layer].clear();
    engine.setLayerSample (layer, s);
    if (switchToSample) setParamValue (srcId (layer), 1.0f);
    changeCounter.fetch_add (1);
    return true;
}

bool KickforgeAudioProcessor::loadLayerSample (int layer, const juce::File& file)
{
    return loadLayerSampleInternal (layer, file, true);
}

void KickforgeAudioProcessor::clearLayerSample (int layer)
{
    if (! juce::isPositiveAndBelow (layer, 4)) return;
    {
        const juce::SpinLock::ScopedLockType sl (samplesLock);
        layerSamples[layer].reset();
        layerSamplePaths[layer].clear();
    }
    engine.setLayerSample (layer, nullptr);
    layerRoot[layer].store (0.0f);
    setParamValue (srcId (layer), 0.0f);
    changeCounter.fetch_add (1);
}

juce::String KickforgeAudioProcessor::getLayerSampleName (int layer) const
{
    const auto path = getLayerSamplePath (layer);
    if (path.isEmpty()) return {};
    const juce::File f (path);
    const int idx = library->indexOfFile (f);
    return idx >= 0 ? library->getEntry (idx).display : f.getFileNameWithoutExtension();
}

float KickforgeAudioProcessor::getLayerRootHz (int layer) const
{
    return juce::isPositiveAndBelow (layer, 4) ? layerRoot[layer].load() : 0.0f;
}

float KickforgeAudioProcessor::getKeyLockShift (int layer) const
{
    // nur tonale Schichten (Punch, Rumble), auf den nächsten Oktav-Abstand (±6 Halbtöne)
    if (keylockRaw == nullptr || keylockRaw->load() < 0.5f || (layer != 1 && layer != 2)) return 0.0f;
    const float root = layerRoot[layer].load(), key = fEndRaw->load();
    if (root <= 0.0f || key <= 0.0f) return 0.0f;
    float st = 12.0f * std::log2 (key / root);
    st -= 12.0f * std::round (st / 12.0f);
    return st;
}

// =========================================================================
// Bibliothek
// =========================================================================
bool KickforgeAudioProcessor::loadLibraryEntry (int layer, int entryIndex)
{
    const auto e = library->getEntry (entryIndex);
    if (e.file == juce::File() || ! loadLayerSampleInternal (layer, e.file, true)) return false;
    setParamValue (onId (layer), 1.0f);
    currentKit = -1;
    autoplay();
    return true;
}

void KickforgeAudioProcessor::stepLayerSample (int layer, int delta)
{
    const auto list = library->entriesForLayer (layer);
    if (list.empty()) return;
    const int cur = library->indexOfFile (juce::File (getLayerSamplePath (layer)));
    const auto it = std::find (list.begin(), list.end(), cur);
    int pos = it == list.end() ? (delta > 0 ? -1 : 0) : (int) (it - list.begin());
    pos = ((pos + delta) % (int) list.size() + (int) list.size()) % (int) list.size();
    loadLibraryEntry (layer, list[(size_t) pos]);
}

void KickforgeAudioProcessor::randomLayerSample (int layer)
{
    const auto list = library->entriesForLayer (layer);
    if (! list.empty())
        loadLibraryEntry (layer, list[(size_t) rng.nextInt ((int) list.size())]);
}

bool KickforgeAudioProcessor::loadKit (int kitIndex)
{
    const auto kits = library->getKits();
    if (! juce::isPositiveAndBelow (kitIndex, (int) kits.size())) return false;
    const auto& k = kits[(size_t) kitIndex];
    for (int l = 0; l < 4; ++l)
    {
        // frisches Kit: Sample-Regler auf neutral
        setParamValue (sGainId (l), 0.8f);
        setParamValue (sPitchId (l), 0.0f);
        setParamValue (sOffId (l), 0.0f);
        setParamValue (sDecId (l), 1000.0f);
        setParamValue (sDrvId (l), 0.0f);
        const auto e = library->getEntry (k.entry[l]);
        const bool ok = k.entry[l] >= 0 && loadLayerSampleInternal (l, e.file, true);
        setParamValue (onId (l), ok ? 1.0f : 0.0f);
    }
    // Master als sanfter Kleber statt Synth-Verzerrung
    setParamValue ("masterDrive", 0.1f);
    setParamValue ("masterLowTone", 2500.0f);
    setParamValue ("outGain", 0.95f);
    currentKit = kitIndex;
    autoplay();
    return true;
}

void KickforgeAudioProcessor::stepKit (int delta)
{
    const int n = (int) library->getKits().size();
    if (n == 0) return;
    const int cur = currentKit < 0 ? (delta > 0 ? -1 : 0) : currentKit;
    loadKit (((cur + delta) % n + n) % n);
}

void KickforgeAudioProcessor::randomKick()
{
    bool any = false;
    for (int l = 0; l < 4; ++l)
    {
        std::vector<int> list;
        for (int i : library->entriesForLayer (l))
            if (! library->getEntry (i).fullKick) list.push_back (i);
        if (list.empty()) continue;
        const auto e = library->getEntry (list[(size_t) rng.nextInt ((int) list.size())]);
        if (loadLayerSampleInternal (l, e.file, true)) { setParamValue (onId (l), 1.0f); any = true; }
    }
    if (! any) return;
    setParamValue (keylockId, 1.0f);   // zufällige Kombination → alles in eine Tonart
    currentKit = -1;
    autoplay();
}

void KickforgeAudioProcessor::timerCallback()
{
    // Samples aus einem gespeicherten Projekt, deren Pfad nicht mehr stimmt:
    // nach dem Bibliotheks-Scan über den Dateinamen wiederfinden
    if (library->isScanning()) return;
    bool pending = false;
    for (int l = 0; l < 4; ++l)
    {
        if (pendingRelink[l].isEmpty()) continue;
        const int idx = library->findByFileName (pendingRelink[l]);
        if (idx >= 0) loadLayerSampleInternal (l, library->getEntry (idx).file, false);
        pendingRelink[l].clear();
        pending = true;
    }
    if (! pending) stopTimer();
}

juce::String KickforgeAudioProcessor::getLayerSamplePath (int layer) const
{
    const juce::SpinLock::ScopedLockType sl (samplesLock);
    return juce::isPositiveAndBelow (layer, 4) ? layerSamplePaths[layer] : juce::String();
}

// =========================================================================
// Zustand
// =========================================================================
std::unique_ptr<juce::XmlElement> KickforgeAudioProcessor::createStateXml()
{
    auto root = std::make_unique<juce::XmlElement> ("KickforgeState");
    root->setAttribute ("version", JucePlugin_VersionString);
    if (auto params = apvts.copyState().createXml())
        root->addChildElement (params.release());
    auto* samples = root->createNewChildElement ("Samples");
    for (int l = 0; l < 4; ++l)
    {
        const auto path = getLayerSamplePath (l);
        if (path.isNotEmpty())
        {
            auto* e = samples->createNewChildElement ("Layer");
            e->setAttribute ("name", layerKey (l));
            e->setAttribute ("path", path);
        }
    }
    return root;
}

void KickforgeAudioProcessor::applyStateXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName ("KickforgeState")) return;
    if (auto* params = xml.getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*params));

    for (int l = 0; l < 4; ++l)
    {
        juce::String path;
        if (auto* samples = xml.getChildByName ("Samples"))
            for (auto* e : samples->getChildIterator())
                if (e->getStringAttribute ("name") == layerKey (l))
                    path = e->getStringAttribute ("path");

        const juce::File f (path);
        if (path.isNotEmpty() && f.existsAsFile())
            loadLayerSampleInternal (l, f, false);   // Modus kommt aus dem gespeicherten Parameter
        else
        {
            if (path.isNotEmpty())
            {
                pendingRelink[l] = f.getFileName();   // nach dem Scan in der Bibliothek suchen
                startTimer (500);
            }
            const juce::SpinLock::ScopedLockType sl (samplesLock);
            layerSamples[l].reset();
            layerSamplePaths[l].clear();
            engine.setLayerSample (l, nullptr);
            layerRoot[l].store (0.0f);
        }
    }
    changeCounter.fetch_add (1);
}

void KickforgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateXml())
        copyXmlToBinary (*xml, destData);
}

void KickforgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        applyStateXml (*xml);
}

juce::AudioProcessorEditor* KickforgeAudioProcessor::createEditor()
{
    return new KickforgeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KickforgeAudioProcessor();
}
