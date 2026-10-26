#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using Param = juce::AudioParameterFloat;
using Choice = juce::AudioParameterChoice;
using Bool  = juce::AudioParameterBool;

// ------------------------------------------------------------------
// Parameter-IDs — zentral, damit Processor/Editor konsistent sind.
// Konvention: <layer>_<param>, Layer ∈ { click, body, tail }, master.
// ------------------------------------------------------------------
namespace pid
{
    // Click
    constexpr auto clickOn    = "click_on";
    constexpr auto clickMode  = "click_mode";
    constexpr auto clickTune  = "click_tune";
    constexpr auto clickDec   = "click_dec";
    constexpr auto clickDrv   = "click_drv";
    constexpr auto clickShp   = "click_shp";
    constexpr auto clickLvl   = "click_lvl";
    constexpr auto clickFMode = "click_fmode";
    constexpr auto clickFCut  = "click_fcut";
    constexpr auto clickFRes  = "click_fres";

    // Body
    constexpr auto bodyMode   = "body_mode";
    constexpr auto bodyTune   = "body_tune";
    constexpr auto bodyPAmt   = "body_pamt";
    constexpr auto bodyPDec   = "body_pdec";
    constexpr auto bodyADec   = "body_adec";
    constexpr auto bodyDrv    = "body_drv";
    constexpr auto bodyShp    = "body_shp";
    constexpr auto bodyLvl    = "body_lvl";
    constexpr auto bodyFMode  = "body_fmode";
    constexpr auto bodyFCut   = "body_fcut";
    constexpr auto bodyFRes   = "body_fres";

    // Tail
    constexpr auto tailOn     = "tail_on";
    constexpr auto tailTune   = "tail_tune";
    constexpr auto tailPAmt   = "tail_pamt";
    constexpr auto tailPDec   = "tail_pdec";
    constexpr auto tailADec   = "tail_adec";
    constexpr auto tailDrv    = "tail_drv";
    constexpr auto tailShp    = "tail_shp";
    constexpr auto tailLvl    = "tail_lvl";
    constexpr auto tailFMode  = "tail_fmode";
    constexpr auto tailFCut   = "tail_fcut";
    constexpr auto tailFRes   = "tail_fres";

    // Master
    constexpr auto masterDrv  = "master_drv";
    constexpr auto masterShp  = "master_shp";
    constexpr auto masterGain = "master_gain";
    constexpr auto masterLim  = "master_lim";
    constexpr auto masterOvs  = "master_ovs";

    // Master-EQ
    constexpr auto eqOn       = "eq_on";
    constexpr auto eqLowFreq  = "eq_lo_f";
    constexpr auto eqLowGain  = "eq_lo_g";
    constexpr auto eqMidFreq  = "eq_md_f";
    constexpr auto eqMidGain  = "eq_md_g";
    constexpr auto eqMidQ     = "eq_md_q";
    constexpr auto eqHighFreq = "eq_hi_f";
    constexpr auto eqHighGain = "eq_hi_g";

    // Master Reverb
    constexpr auto rvOn    = "rv_on";
    constexpr auto rvRoom  = "rv_room";
    constexpr auto rvDamp  = "rv_damp";
    constexpr auto rvWet   = "rv_wet";

    // Master Transient Shaper
    constexpr auto trOn    = "tr_on";
    constexpr auto trAtk   = "tr_atk";
    constexpr auto trSus   = "tr_sus";

    // Layer-Delays
    constexpr auto clickDly = "click_dly";
    constexpr auto bodyDly  = "body_dly";
    constexpr auto tailDly  = "tail_dly";
}

static const juce::StringArray kFilterNames { "Off", "LP", "HP", "BP" };

static const juce::StringArray kShaperNames { "Tanh", "HardClip", "Fold", "BitCrush" };
static const juce::StringArray kOscNames    { "Sine", "Noise", "Blip", "Triangle", "Sample" };

// ------------------------------------------------------------------
KickforgeAudioProcessor::KickforgeAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout()),
      presets (apvts)
{
    attachAll();
}

KickforgeAudioProcessor::~KickforgeAudioProcessor()
{
    detachAll();
}

// ------------------------------------------------------------------
// Anzeige-Formatierung der Parameter (Knob-Werte, DAW-Automation)
// ------------------------------------------------------------------
enum class U { Pct, Hz, Ms, St, Db, Gain, Bip, Num };

static juce::String fmtVal (float v, U u)
{
    using juce::String;
    switch (u)
    {
        case U::Pct:  return String (juce::roundToInt (v * 100.0f)) + " %";
        case U::Hz:   return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                                          : String (v, v < 100.0f ? 1 : 0) + " Hz";
        case U::Ms:   return v >= 1000.0f ? String (v / 1000.0f, 2) + " s"
                                          : String (v, v < 100.0f ? 1 : 0) + " ms";
        case U::St:   return String (v, 1) + " st";
        case U::Db:   return (v > 0.05f ? "+" : "") + String (v, 1) + " dB";
        case U::Gain: return v <= 0.0001f ? String ("-inf dB")
                                          : String (juce::Decibels::gainToDecibels (v), 1) + " dB";
        case U::Bip:  return (v > 0.005f ? "+" : "") + String (juce::roundToInt (v * 100.0f)) + " %";
        case U::Num:  return String (v, 2);
    }
    return String (v, 2);
}

static float parseVal (const juce::String& txt, U u)
{
    auto t = txt.trim().toLowerCase();
    float v = t.retainCharacters ("0123456789.-+").getFloatValue();
    if (u == U::Hz && t.contains ("k"))                       v *= 1000.0f;
    if (u == U::Ms && t.endsWith ("s") && ! t.contains ("ms")) v *= 1000.0f;
    if (u == U::Pct || u == U::Bip)                           v /= 100.0f;
    if (u == U::Gain)                                         v = juce::Decibels::decibelsToGain (v);
    return v;
}

static std::unique_ptr<juce::AudioParameterFloat> fp (const char* id, const char* name,
                                                      juce::NormalisableRange<float> range,
                                                      float def, U u)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, range, def,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([u] (float v, int) { return fmtVal (v, u); })
            .withValueFromStringFunction ([u] (const juce::String& s) { return parseVal (s, u); }));
}

APVTS::ParameterLayout KickforgeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto hzRange   = juce::NormalisableRange<float>(20.0f, 8000.0f, 0.1f, 0.3f);
    auto subRange  = juce::NormalisableRange<float>(20.0f, 200.0f, 0.1f, 0.4f);
    auto decShort  = juce::NormalisableRange<float>(1.0f, 500.0f, 0.1f, 0.4f);
    auto decLong   = juce::NormalisableRange<float>(10.0f, 2500.0f, 1.0f, 0.4f);
    auto zeroOne   = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);
    auto semiRange = juce::NormalisableRange<float>(0.0f, 60.0f, 0.1f);
    auto gainRange = juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f, 0.5f);
    auto fCutRange = juce::NormalisableRange<float>(20.0f, 18000.0f, 0.1f, 0.3f);
    auto fResRange = juce::NormalisableRange<float>(0.1f, 20.0f, 0.01f, 0.3f);

    // ---- Click ----
    p.push_back (std::make_unique<Bool>  (juce::ParameterID{pid::clickOn,1},   "Click On", true));
    p.push_back (std::make_unique<Choice>(juce::ParameterID{pid::clickMode,1}, "Click Mode", kOscNames, 1));
    p.push_back (fp (pid::clickTune, "Click Tune", hzRange, 2500.0f, U::Hz));
    p.push_back (fp (pid::clickDec, "Click Decay", juce::NormalisableRange<float>(1.0f,500.0f,0.1f,0.3f), 8.0f, U::Ms));
    p.push_back (fp (pid::clickDrv, "Click Drive", zeroOne, 0.4f, U::Pct));
    p.push_back (std::make_unique<Choice>(juce::ParameterID{pid::clickShp,1},  "Click Shaper", kShaperNames, 1));
    p.push_back (fp (pid::clickLvl, "Click Level", zeroOne, 0.6f, U::Pct));
    p.push_back (std::make_unique<Choice>(juce::ParameterID{pid::clickFMode,1},"Click Filter", kFilterNames, 2)); // Default HP
    p.push_back (fp (pid::clickFCut, "Click F.Cut", fCutRange, 300.0f, U::Hz));
    p.push_back (fp (pid::clickFRes, "Click F.Res", fResRange, 0.707f, U::Num));

    // ---- Body ----
    p.push_back (std::make_unique<Choice>(juce::ParameterID{pid::bodyMode,1}, "Body Mode", kOscNames, 0));
    p.push_back (fp (pid::bodyTune, "Body Tune", subRange, 55.0f, U::Hz));
    p.push_back (fp (pid::bodyPAmt, "Body P.Amt", semiRange, 36.0f, U::St));
    p.push_back (fp (pid::bodyPDec, "Body P.Dec", decShort, 40.0f, U::Ms));
    p.push_back (fp (pid::bodyADec, "Body A.Dec", decLong, 320.0f, U::Ms));
    p.push_back (fp (pid::bodyDrv, "Body Drive", zeroOne, 0.45f, U::Pct));
    p.push_back (std::make_unique<Choice>(juce::ParameterID{pid::bodyShp,1},  "Body Shaper", kShaperNames, 1));
    p.push_back (fp (pid::bodyLvl, "Body Level", zeroOne, 1.0f, U::Pct));
    p.push_back (std::make_unique<Choice>(juce::ParameterID{pid::bodyFMode,1},"Body Filter", kFilterNames, 0));
    p.push_back (fp (pid::bodyFCut, "Body F.Cut", fCutRange, 2000.0f, U::Hz));
    p.push_back (fp (pid::bodyFRes, "Body F.Res", fResRange, 0.707f, U::Num));

    // ---- Tail ----
    p.push_back (std::make_unique<Bool>  (juce::ParameterID{pid::tailOn,1},   "Tail On", true));
    p.push_back (fp (pid::tailTune, "Tail Tune", subRange, 42.0f, U::Hz));
    p.push_back (fp (pid::tailPAmt, "Tail P.Amt", juce::NormalisableRange<float>(0.0f,24.0f,0.1f), 4.0f, U::St));
    p.push_back (fp (pid::tailPDec, "Tail P.Dec", decShort, 120.0f, U::Ms));
    p.push_back (fp (pid::tailADec, "Tail A.Dec", decLong, 900.0f, U::Ms));
    p.push_back (fp (pid::tailDrv, "Tail Drive", zeroOne, 0.1f, U::Pct));
    p.push_back (std::make_unique<Choice>(juce::ParameterID{pid::tailShp,1},  "Tail Shaper", kShaperNames, 0));
    p.push_back (fp (pid::tailLvl, "Tail Level", zeroOne, 0.7f, U::Pct));
    p.push_back (std::make_unique<Choice>(juce::ParameterID{pid::tailFMode,1},"Tail Filter", kFilterNames, 1)); // Default LP
    p.push_back (fp (pid::tailFCut, "Tail F.Cut", fCutRange, 180.0f, U::Hz));
    p.push_back (fp (pid::tailFRes, "Tail F.Res", fResRange, 0.707f, U::Num));

    // ---- Master ----
    p.push_back (fp (pid::masterDrv, "Master Drive", zeroOne, 0.25f, U::Pct));
    p.push_back (std::make_unique<Choice>(juce::ParameterID{pid::masterShp,1},  "Master Shaper", kShaperNames, 1));
    p.push_back (fp (pid::masterGain, "Master Gain", gainRange, 0.9f, U::Gain));
    p.push_back (std::make_unique<Bool>  (juce::ParameterID{pid::masterLim,1},  "Limiter", true));
    p.push_back (std::make_unique<Bool>  (juce::ParameterID{pid::masterOvs,1},  "Oversample 2x", true));

    // ---- Master EQ ----
    auto eqFreqLow  = juce::NormalisableRange<float>(20.0f, 500.0f,  0.1f, 0.5f);
    auto eqFreqMid  = juce::NormalisableRange<float>(100.0f, 8000.0f,0.1f, 0.3f);
    auto eqFreqHigh = juce::NormalisableRange<float>(1000.0f, 18000.0f, 1.0f, 0.3f);
    auto eqGainRng  = juce::NormalisableRange<float>(-18.0f, 18.0f, 0.01f);
    auto eqQRng     = juce::NormalisableRange<float>(0.3f, 8.0f, 0.01f, 0.4f);

    p.push_back (std::make_unique<Bool>  (juce::ParameterID{pid::eqOn,1},      "EQ On", true));
    p.push_back (fp (pid::eqLowFreq, "EQ Low F", eqFreqLow, 80.0f, U::Hz));
    p.push_back (fp (pid::eqLowGain, "EQ Low G", eqGainRng, 0.0f, U::Db));
    p.push_back (fp (pid::eqMidFreq, "EQ Mid F", eqFreqMid, 800.0f, U::Hz));
    p.push_back (fp (pid::eqMidGain, "EQ Mid G", eqGainRng, 0.0f, U::Db));
    p.push_back (fp (pid::eqMidQ, "EQ Mid Q", eqQRng, 0.707f, U::Num));
    p.push_back (fp (pid::eqHighFreq, "EQ High F", eqFreqHigh, 6000.0f, U::Hz));
    p.push_back (fp (pid::eqHighGain, "EQ High G", eqGainRng, 0.0f, U::Db));

    // ---- Master Reverb ----
    p.push_back (std::make_unique<Bool>  (juce::ParameterID{pid::rvOn,1},   "Reverb On", false));
    p.push_back (fp (pid::rvRoom, "Rev Room", zeroOne, 0.5f, U::Pct));
    p.push_back (fp (pid::rvDamp, "Rev Damp", zeroOne, 0.5f, U::Pct));
    p.push_back (fp (pid::rvWet, "Rev Wet", zeroOne, 0.2f, U::Pct));

    // ---- Master Transient Shaper ----
    auto bipolar = juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f);
    p.push_back (std::make_unique<Bool>  (juce::ParameterID{pid::trOn,1},  "Trans On",  false));
    p.push_back (fp (pid::trAtk, "Trans Atk", bipolar, 0.0f, U::Bip));
    p.push_back (fp (pid::trSus, "Trans Sus", bipolar, 0.0f, U::Bip));

    // ---- Layer-Delays (0..100 ms) ----
    auto dlyRng = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);
    p.push_back (fp (pid::clickDly, "Click Delay", dlyRng, 0.0f, U::Ms));
    p.push_back (fp (pid::bodyDly, "Body Delay", dlyRng, 0.0f, U::Ms));
    p.push_back (fp (pid::tailDly, "Tail Delay", dlyRng, 0.0f, U::Ms));

    return { p.begin(), p.end() };
}

void KickforgeAudioProcessor::attachAll()
{
    for (auto* p : getParameters())
    {
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
            apvts.addParameterListener (rp->getParameterID(), this);
    }
    // Beim Start einmal alles sync'en (beide voices)
    for (auto* p : getParameters())
    {
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
            syncParamToEngine (rp->getParameterID(),
                               *apvts.getRawParameterValue (rp->getParameterID()),
                               true);
    }
}

void KickforgeAudioProcessor::detachAll()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
            apvts.removeParameterListener (rp->getParameterID(), this);
}

void KickforgeAudioProcessor::parameterChanged (const juce::String& id, float v)
{
    syncParamToEngine (id, v, true);
    paramsChanged.store (true);
}

void KickforgeAudioProcessor::syncParamToEngine (const juce::String& id, float v, bool toBothVoices)
{
    auto apply = [&id, v] (kf::KickVoice& vc)
    {
        auto& C = vc.click();
        auto& B = vc.body();
        auto& T = vc.tail();
        auto& E = vc.eq();

        if      (id == pid::clickOn)    C.setMute (v < 0.5f);
        else if (id == pid::clickMode)  C.setMode ((int) v);
        else if (id == pid::clickTune)  C.setTune (v);
        else if (id == pid::clickDec)   C.setAmpDecay (v);
        else if (id == pid::clickDrv)   C.setDrive (v);
        else if (id == pid::clickShp)   C.setShaper ((int) v);
        else if (id == pid::clickLvl)   C.setLevel (v);
        else if (id == pid::clickFMode) C.setFilterMode ((int) v);
        else if (id == pid::clickFCut)  C.setFilterCutoff (v);
        else if (id == pid::clickFRes)  C.setFilterReso (v);

        else if (id == pid::bodyMode)   B.setMode ((int) v);
        else if (id == pid::bodyTune)   B.setTune (v);
        else if (id == pid::bodyPAmt)   B.setPitchAmount (v);
        else if (id == pid::bodyPDec)   B.setPitchDecay (v);
        else if (id == pid::bodyADec)   B.setAmpDecay (v);
        else if (id == pid::bodyDrv)    B.setDrive (v);
        else if (id == pid::bodyShp)    B.setShaper ((int) v);
        else if (id == pid::bodyLvl)    B.setLevel (v);
        else if (id == pid::bodyFMode)  B.setFilterMode ((int) v);
        else if (id == pid::bodyFCut)   B.setFilterCutoff (v);
        else if (id == pid::bodyFRes)   B.setFilterReso (v);

        else if (id == pid::tailOn)     T.setMute (v < 0.5f);
        else if (id == pid::tailTune)   T.setTune (v);
        else if (id == pid::tailPAmt)   T.setPitchAmount (v);
        else if (id == pid::tailPDec)   T.setPitchDecay (v);
        else if (id == pid::tailADec)   T.setAmpDecay (v);
        else if (id == pid::tailDrv)    T.setDrive (v);
        else if (id == pid::tailShp)    T.setShaper ((int) v);
        else if (id == pid::tailLvl)    T.setLevel (v);
        else if (id == pid::tailFMode)  T.setFilterMode ((int) v);
        else if (id == pid::tailFCut)   T.setFilterCutoff (v);
        else if (id == pid::tailFRes)   T.setFilterReso (v);

        else if (id == pid::masterDrv)  vc.setMasterDrive (v);
        else if (id == pid::masterShp)  vc.setMasterShaper ((int) v);
        else if (id == pid::masterGain) vc.setMasterGain (v);
        else if (id == pid::masterLim)  vc.setLimiterOn (v >= 0.5f);
        else if (id == pid::masterOvs)  vc.setOversampleOn (v >= 0.5f);

        else if (id == pid::eqOn)       E.setOn (v >= 0.5f);
        else if (id == pid::eqLowFreq)  E.setLowFreq (v);
        else if (id == pid::eqLowGain)  E.setLowGain (v);
        else if (id == pid::eqMidFreq)  E.setMidFreq (v);
        else if (id == pid::eqMidGain)  E.setMidGain (v);
        else if (id == pid::eqMidQ)     E.setMidQ (v);
        else if (id == pid::eqHighFreq) E.setHighFreq (v);
        else if (id == pid::eqHighGain) E.setHighGain (v);

        else if (id == pid::rvOn)       vc.reverb().setOn (v >= 0.5f);
        else if (id == pid::rvRoom)     vc.reverb().setRoomSize (v);
        else if (id == pid::rvDamp)     vc.reverb().setDamping (v);
        else if (id == pid::rvWet)      vc.reverb().setWet (v);

        else if (id == pid::trOn)       vc.transient().setOn (v >= 0.5f);
        else if (id == pid::trAtk)      vc.transient().setAttack (v);
        else if (id == pid::trSus)      vc.transient().setSustain (v);

        else if (id == pid::clickDly)   C.setDelayMs (v);
        else if (id == pid::bodyDly)    B.setDelayMs (v);
        else if (id == pid::tailDly)    T.setDelayMs (v);
    };

    apply (voice);
    if (toBothVoices)
        apply (offlineVoice);
}

void KickforgeAudioProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    voice.prepare (sampleRate, blockSize);
    // Offline-Voice wird on-demand mit der jeweiligen SR in renderOfflineKick geprepared.
}

void KickforgeAudioProcessor::renderOfflineKick (juce::AudioBuffer<float>& out, double sampleRate)
{
    // Offline-Voice für diese Sample-Rate frisch prepare+reset, dann
    // triggern und in den Buffer schreiben. Keine Race mit der Live-Voice.
    const juce::ScopedLock sl (offlineLock);
    const int numSamples = out.getNumSamples();
    const int blockSize  = juce::jmin (1024, numSamples);

    offlineVoice.prepare (sampleRate, blockSize);
    offlineVoice.reset();
    offlineVoice.trigger (0.95f);

    auto* data = out.getWritePointer (0);
    int remaining = numSamples;
    int offset = 0;
    while (remaining > 0)
    {
        const int chunk = juce::jmin (blockSize, remaining);
        offlineVoice.render (data + offset, chunk);
        remaining -= chunk;
        offset    += chunk;
    }

    // Stereo kopieren falls gewünscht
    for (int ch = 1; ch < out.getNumChannels(); ++ch)
        out.copyFrom (ch, 0, out.getReadPointer (0), numSamples);
}

void KickforgeAudioProcessor::renderOfflineParts (juce::AudioBuffer<float>& parts, double sampleRate)
{
    jassert (parts.getNumChannels() >= 3);
    const juce::ScopedLock sl (offlineLock);
    const int numSamples = parts.getNumSamples();
    const int blockSize  = juce::jmin (1024, numSamples);

    offlineVoice.prepare (sampleRate, blockSize);
    offlineVoice.reset();
    offlineVoice.trigger (0.95f);

    for (int off = 0; off < numSamples; off += blockSize)
    {
        const int chunk = juce::jmin (blockSize, numSamples - off);
        offlineVoice.renderParts (parts.getWritePointer (0) + off,
                                  parts.getWritePointer (1) + off,
                                  parts.getWritePointer (2) + off, chunk);
    }
}

bool KickforgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void KickforgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    auto* left = buffer.getWritePointer (0);

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            voice.trigger (msg.getFloatVelocity());
    }

    if (auditionPending.exchange (false))
        voice.trigger (0.9f);

    voice.render (left, numSamples);

    // Peak für die Output-Anzeige (GUI liest per Timer, eigener Abfall dort)
    const float blockPeak = buffer.getMagnitude (0, 0, numSamples);
    if (blockPeak > outputPeak.load())
        outputPeak.store (blockPeak);

    if (buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, left, numSamples);
}

juce::AudioProcessorEditor* KickforgeAudioProcessor::createEditor()
{
    return new KickforgeAudioProcessorEditor (*this);
}

void KickforgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Wrapper-XML, der APVTS-State und sample-Pfad enthält
    juce::XmlElement root ("KickforgeState");
    if (auto apvtsXml = apvts.copyState().createXml())
        root.addChildElement (apvtsXml.release());

    if (clickSamplePath.isNotEmpty())
    {
        auto* s = new juce::XmlElement ("ClickSample");
        s->setAttribute ("path", clickSamplePath);
        root.addChildElement (s);
    }
    copyXmlToBinary (root, destData);
}

void KickforgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;

    // Rückwärts-kompatibel: alte Presets haben direkt den APVTS-State
    if (xml->hasTagName ("KickforgeState"))
    {
        if (auto* apvtsXml = xml->getChildByName ("PARAMS"))
            apvts.replaceState (juce::ValueTree::fromXml (*apvtsXml));
        if (auto* s = xml->getChildByName ("ClickSample"))
        {
            juce::File f (s->getStringAttribute ("path"));
            if (f.existsAsFile()) loadClickSample (f);
        }
    }
    else
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

bool KickforgeAudioProcessor::loadClickSample (const juce::File& file)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) return false;

    // Bis zu 2 Sekunden laden (reicht für Transients/Clicks dicke)
    const int maxSamples = (int) juce::jmin ((juce::int64) (reader->sampleRate * 2.0),
                                             (juce::int64) reader->lengthInSamples);

    auto buf = std::make_shared<juce::AudioBuffer<float>> (1, maxSamples);
    buf->clear();
    reader->read (buf.get(), 0, maxSamples, 0, true, false);

    clickSampleBuffer = buf;
    clickSampleSr     = reader->sampleRate;
    clickSamplePath   = file.getFullPathName();

    // Beiden Voices zuweisen (shared pointer — kein Kopieren)
    voice.click()       .setSampleBuffer (clickSampleBuffer, clickSampleSr);
    offlineVoice.click().setSampleBuffer (clickSampleBuffer, clickSampleSr);
    paramsChanged.store (true);
    return true;
}

void KickforgeAudioProcessor::clearClickSample()
{
    clickSampleBuffer.reset();
    clickSamplePath.clear();
    voice.click()       .setSampleBuffer (nullptr, 44100.0);
    offlineVoice.click().setSampleBuffer (nullptr, 44100.0);
    paramsChanged.store (true);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KickforgeAudioProcessor();
}
