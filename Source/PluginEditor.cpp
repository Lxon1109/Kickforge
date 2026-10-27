#include "PluginEditor.h"
#include "BinaryData.h"

namespace T = kf::theme;
using W = T::Weight;
using namespace kf::params;

namespace layout
{
    constexpr int width   = 1320;
    constexpr int height  = 846;
    constexpr int pad     = 14;
    constexpr int gap     = 8;
    constexpr int libY    = 66,  libH    = 30;
    constexpr int vizY    = 102, vizH    = 94;
    constexpr int layersY = 204, layersH = 428;
    constexpr int bottomY = 640;
    constexpr int clickW  = 204;
    constexpr int masterW = 420;
}

// =========================================================================
// Bausteine
// =========================================================================

std::unique_ptr<KickforgeAudioProcessorEditor::Knob>
KickforgeAudioProcessorEditor::makeKnob (const juce::String& id, const juce::String& text,
                                         juce::Colour accent, KnobSize size, bool bipolar)
{
    using namespace juce;
    auto k = std::make_unique<Knob>();
    k->size = size;

    auto& s = k->slider;
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    s.setTextBoxStyle (Slider::TextBoxBelow, false, 96, size == KnobSize::Small ? 15 : 17);
    s.setColour (Slider::rotarySliderFillColourId, accent);
    s.getProperties().set ("size", size == KnobSize::Big ? "big" : size == KnobSize::Med ? "med" : "small");
    if (bipolar) s.getProperties().set ("bipolar", true);
    s.setMouseDragSensitivity (size == KnobSize::Big ? 280 : 220);
    addAndMakeVisible (s);

    k->label.setText (text, dontSendNotification);
    k->label.setJustificationType (Justification::centred);
    k->label.setFont (lnf.fonts().get (W::Bold, size == KnobSize::Big ? 11.5f : 10.5f, 0.14f));
    k->label.setColour (Label::textColourId, size == KnobSize::Big ? T::text.withAlpha (0.7f) : T::dim);
    k->label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k->label);

    k->att = std::make_unique<SAtt> (proc.apvts, id, s);
    if (auto* p = proc.apvts.getParameter (id))
    {
        s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        s.setTooltip (p->getName (64));
    }
    return k;
}

std::unique_ptr<KickforgeAudioProcessorEditor::Toggle>
KickforgeAudioProcessorEditor::makeToggle (const juce::String& id, const juce::String& text, juce::Colour accent)
{
    auto t = std::make_unique<Toggle>();
    t->button.setButtonText (text);
    t->button.setColour (juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible (t->button);
    t->att = std::make_unique<BAtt> (proc.apvts, id, t->button);
    return t;
}

// =========================================================================
// Konstruktor
// =========================================================================

KickforgeAudioProcessorEditor::KickforgeAudioProcessorEditor (KickforgeAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    buildBackgroundTexture();
    buildHeader();
    buildLibraryBar();
    buildPanels();
    addAndMakeVisible (visualizer);

    setLookAndFeel (&lnf);
    setResizable (false, false);
    setSize (layout::width, layout::height);   // erst am Ende: triggert resized()

    updateLayerPanels();
    updateLibraryBar();
    updateTempoInfo();
    renderVisualizer();
    startTimerHz (30);
}

KickforgeAudioProcessorEditor::~KickforgeAudioProcessorEditor()
{
    stopTimer();
    renderPool.removeAllJobs (false, 3000);
    setLookAndFeel (nullptr);
}

void KickforgeAudioProcessorEditor::buildHeader()
{
    using namespace juce;
    if (auto xml = XmlDocument::parse (String::fromUTF8 (BinaryData::emblem_svg, BinaryData::emblem_svgSize)))
        emblem = Drawable::createFromSVG (*xml);

    presetCombo.setTextWhenNothingSelected (proc.getPresetManager().getCurrentName());
    presetCombo.setColour (ComboBox::arrowColourId, T::brand);
    addAndMakeVisible (presetCombo);
    refreshPresetCombo();
    presetCombo.onChange = [this]
    {
        const int idx = presetCombo.getSelectedId() - 1;
        if (idx >= 0) proc.getPresetManager().load (idx);
    };

    for (auto* b : { &prevPresetBtn, &nextPresetBtn, &savePresetBtn, &exportBtn, &triggerBtn })
        addAndMakeVisible (*b);
    triggerBtn.getProperties().set ("primary", true);

    prevPresetBtn.onClick = [this] { stepPreset (-1); };
    nextPresetBtn.onClick = [this] { stepPreset (+1); };
    savePresetBtn.onClick = [this] { onSavePresetClicked(); };
    exportBtn    .onClick = [this] { onExportClicked(); };
    triggerBtn   .onClick = [this] { proc.triggerAudition(); };
}

void KickforgeAudioProcessorEditor::setParam (const juce::String& id, float value)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        prm->endChangeGesture();
    }
}

void KickforgeAudioProcessorEditor::buildLibraryBar()
{
    using namespace juce;
    libStatus.setFont (lnf.fonts().get (W::SemiBold, 12.0f, 0.04f));
    libStatus.setColour (Label::textColourId, T::text.withAlpha (0.8f));
    libStatus.setJustificationType (Justification::centredLeft);
    libStatus.setMinimumHorizontalScale (0.75f);
    addAndMakeVisible (libStatus);

    kitPrevBtn.setButtonText (String::fromUTF8 ("\xe2\x97\x80"));
    kitNextBtn.setButtonText (String::fromUTF8 ("\xe2\x96\xb6"));
    kitNameBtn.getProperties().set ("align", "left");
    kitPrevBtn.setTooltip ("Vorheriges Kit (alle vier Schichten passend)");
    kitNextBtn.setTooltip (String::fromUTF8 ("N\xc3\xa4" "chstes Kit"));
    kitNameBtn.setTooltip (String::fromUTF8 ("Kit w\xc3\xa4hlen"));
    randomKickBtn.setTooltip (String::fromUTF8 ("Zuf\xc3\xa4llige Kombination aus der Bibliothek, KEY LOCK h\xc3\xa4lt alles in einer Tonart"));
    randomKickBtn.getProperties().set ("primary", true);
    libMenuBtn.setTooltip ("Ordner der Sample-Bibliothek verwalten");

    kitPrevBtn.onClick    = [this] { proc.stepKit (-1); updateLayerPanels(); updateLibraryBar(); };
    kitNextBtn.onClick    = [this] { proc.stepKit (+1); updateLayerPanels(); updateLibraryBar(); };
    kitNameBtn.onClick    = [this] { showKitMenu(); };
    randomKickBtn.onClick = [this] { proc.randomKick(); updateLayerPanels(); updateLibraryBar(); };
    libMenuBtn.onClick    = [this] { showLibraryMenu(); };
    for (auto* b : { &libMenuBtn, &kitPrevBtn, &kitNextBtn, &kitNameBtn, &randomKickBtn }) addAndMakeVisible (*b);

    keylockToggle  = makeToggle (keylockId,  "KEY LOCK",  T::brand);
    autoplayToggle = makeToggle (autoplayId, "AUTO-PLAY", T::brand);
    keylockToggle->button.setTooltip ("Punch- und Rumble-Samples automatisch auf den KEY stimmen");
    autoplayToggle->button.setTooltip (String::fromUTF8 ("Beim Durchbl\xc3\xa4ttern sofort abspielen"));
}

void KickforgeAudioProcessorEditor::buildLayerControls (Panel& p)
{
    using namespace juce;
    const int l = p.layer;
    const auto arrowL = String::fromUTF8 ("\xe2\x97\x80"), arrowR = String::fromUTF8 ("\xe2\x96\xb6");

    p.onToggle = makeToggle (onId (l), "ON", p.accent);
    p.onToggle->button.setTooltip ("Schicht an/aus");

    // Quelle: SYNTH | SAMPLE
    for (auto* b : { &p.synthBtn, &p.sampleBtn })
    {
        b->setClickingTogglesState (false);
        b->setColour (TextButton::buttonOnColourId, p.accent);
        addAndMakeVisible (*b);
    }
    p.synthBtn .onClick = [this, l] { setParam (srcId (l), 0.0f); updateLayerPanels(); };
    p.sampleBtn.onClick = [this, l] { setParam (srcId (l), 1.0f); updateLayerPanels(); };
    p.synthBtn .setTooltip ("Synthese (Kickforge-Engine)");
    p.sampleBtn.setTooltip ("Sample aus der Bibliothek oder eigene Datei");

    p.clearBtn.setButtonText (String::fromUTF8 ("\xe2\x9c\x95"));   // ✕
    p.loadBtn .setTooltip ("Eigene WAV/AIFF/FLAC laden (oder Datei auf die Schicht ziehen)");
    p.clearBtn.setTooltip ("Sample entfernen");
    p.loadBtn .onClick = [this, l] { onLoadSampleClicked (l); };
    p.clearBtn.onClick = [this, l] { proc.clearLayerSample (l); updateLayerPanels(); };
    addAndMakeVisible (p.loadBtn);
    addAndMakeVisible (p.clearBtn);

    // Bibliothek: ◀ Name ▶ Würfel
    p.prevBtn.setButtonText (arrowL);
    p.nextBtn.setButtonText (arrowR);
    p.randBtn.setButtonText ("RND");
    p.prevBtn.setTooltip ("Vorheriges Sample dieser Schicht");
    p.nextBtn.setTooltip (juce::String::fromUTF8 ("N\xc3\xa4" "chstes Sample dieser Schicht"));
    p.randBtn.setTooltip (String::fromUTF8 ("Zuf\xc3\xa4lliges Sample dieser Schicht"));
    p.nameBtn.setTooltip (String::fromUTF8 ("Sample aus der Bibliothek w\xc3\xa4hlen"));
    p.nameBtn.getProperties().set ("align", "left");
    p.prevBtn.onClick = [this, l] { proc.stepLayerSample (l, -1); updateLayerPanels(); };
    p.nextBtn.onClick = [this, l] { proc.stepLayerSample (l, +1); updateLayerPanels(); };
    p.randBtn.onClick = [this, l] { proc.randomLayerSample (l);   updateLayerPanels(); };
    p.nameBtn.onClick = [this, l] { showSampleMenu (l); };
    for (auto* b : { &p.prevBtn, &p.nextBtn, &p.randBtn, &p.nameBtn }) addAndMakeVisible (*b);

    p.info.setFont (lnf.fonts().get (W::SemiBold, 11.0f, 0.06f));
    p.info.setColour (Label::textColourId, T::dim);
    p.info.setJustificationType (Justification::centred);
    p.info.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (p.info);

    // Sample-Regler
    p.sBig.push_back (makeKnob (sGainId (l),  "GAIN",   p.accent, KnobSize::Big));
    p.sBig.push_back (makeKnob (sPitchId (l), "PITCH",  p.accent, KnobSize::Big, true));
    p.sMed.push_back (makeKnob (sDrvId (l),   "DRIVE",  p.accent, KnobSize::Med));
    p.sMed.push_back (makeKnob (sOffId (l),   "OFFSET", p.accent, KnobSize::Med));
    p.sMed.push_back (makeKnob (sDecId (l),   "DECAY",  p.accent, KnobSize::Med));
}

void KickforgeAudioProcessorEditor::buildPanels()
{
    using KS = KnobSize;
    auto add = [this] (std::vector<std::unique_ptr<Knob>>& v, const char* id, const char* txt, juce::Colour c, KS s)
    {
        v.push_back (makeKnob (id, txt, c, s));
    };

    // ---------- 01 CLICK ----------
    clickP.index = "01"; clickP.title = "CLICK"; clickP.accent = T::click; clickP.layer = 0;
    add (clickP.big, "clickLevel",   "LEVEL", T::click, KS::Big);
    add (clickP.med, "clickDecayMs", "DECAY", T::click, KS::Med);
    add (clickP.med, "clickTone",    "TONE",  T::click, KS::Med);

    // ---------- 02 PUNCH ----------
    auto& pu = punchP; const auto cP = T::punch;
    pu.index = "02"; pu.title = "PUNCH"; pu.accent = cP; pu.layer = 1;
    add (pu.big,   "punchLevel",  "LEVEL",   cP, KS::Big);
    add (pu.big,   "punchDrive",  "DRIVE",   cP, KS::Big);
    add (pu.big,   "punchLenMs",  "LENGTH",  cP, KS::Big);
    add (pu.med,   "fStart",      "START",   cP, KS::Med);
    add (pu.med,   "fMid",        "MID",     cP, KS::Med);
    add (pu.med,   "t1Ms",        "SNAP",    cP, KS::Med);
    add (pu.med,   "t2Ms",        "SWEEP",   cP, KS::Med);
    add (pu.small, "punchDirt",   "DIRT",    cP, KS::Small);
    add (pu.small, "punchAsym",   "ASYM",    cP, KS::Small);
    add (pu.small, "punchTone",   "TONE",    cP, KS::Small);
    add (pu.small, "punchHpFrom", "HP",      cP, KS::Small);
    add (pu.small, "punchHpMs",   "HP TIME", cP, KS::Small);

    // ---------- 03 RUMBLE ----------
    auto& ru = rumbleP; const auto cR = T::rumble;
    ru.index = "03"; ru.title = "RUMBLE"; ru.accent = cR; ru.layer = 2;
    add (ru.big,   "rumbleLevel",   "LEVEL",  cR, KS::Big);
    add (ru.big,   "rumbleDrive",   "DRIVE",  cR, KS::Big);
    add (ru.big,   "rumbleSize",    "SIZE",   cR, KS::Big);
    add (ru.med,   "rumbleStartMs", "START",  cR, KS::Med);
    add (ru.med,   "rumbleFeedMs",  "FEED",   cR, KS::Med);
    add (ru.med,   "rumbleTone",    "TONE",   cR, KS::Med);
    add (ru.med,   "rumbleBright",  "BRIGHT", cR, KS::Med);
    add (ru.small, "rumbleDirt",    "DIRT",   cR, KS::Small);
    add (ru.small, "rumbleDamp",    "DAMP",   cR, KS::Small);
    add (ru.small, "rumbleFadeMs",  "FADE",   cR, KS::Small);

    // ---------- 04 GRIT ----------
    auto& gr = gritP; const auto cG = T::grit;
    gr.index = "04"; gr.title = "TOP / GRIT"; gr.accent = cG; gr.layer = 3;
    add (gr.big,   "gritLevel",   "LEVEL",  cG, KS::Big);
    add (gr.big,   "gritDrive",   "DRIVE",  cG, KS::Big);
    add (gr.big,   "gritNoise",   "NOISE",  cG, KS::Big);
    add (gr.med,   "gritStartMs", "START",  cG, KS::Med);
    add (gr.med,   "gritLenMs",   "LENGTH", cG, KS::Med);
    add (gr.med,   "gritTail",    "TAIL",   cG, KS::Med);
    add (gr.med,   "gritTone",    "TONE",   cG, KS::Med);
    add (gr.small, "gritWidth",   "WIDTH",  cG, KS::Small);

    for (auto* p : { &clickP, &punchP, &rumbleP, &gritP })
        buildLayerControls (*p);

    // ---------- 05 KICK (global) ----------
    auto& k = kickP; const auto cK = T::global;
    k.index = "05"; k.title = "KICK"; k.accent = cK;
    add (k.big, "fEnd",         "KEY",       cK, KS::Big);
    add (k.big, "lengthBeats",  "LENGTH",    cK, KS::Big);
    add (k.med, "cutMs",        "CUT",       cK, KS::Med);
    add (k.med, "releaseMs",    "RELEASE",   cK, KS::Med);
    add (k.med, "releaseLevel", "REL LEVEL", cK, KS::Med);
    add (k.med, "releaseDrop",  "REL DROP",  cK, KS::Med);
    add (k.med, "seed",         "VARIATION", cK, KS::Med);
    add (k.med, "bpm",          "TEMPO",     cK, KS::Med);
    syncToggle     = makeToggle (syncId,     "HOST SYNC", cK);
    keytrackToggle = makeToggle (keytrackId, "KEY TRACK", cK);
    syncToggle->button.setTooltip (juce::String::fromUTF8 ("Kick-L\xc3\xa4nge folgt dem Tempo der DAW"));
    keytrackToggle->button.setTooltip ("Die gespielte MIDI-Note bestimmt den Grundton");
    tempoInfo.setFont (lnf.fonts().get (W::SemiBold, 11.5f, 0.06f));
    tempoInfo.setColour (juce::Label::textColourId, T::dim);
    tempoInfo.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (tempoInfo);

    // ---------- 06 MASTER ----------
    auto& m = masterP; const auto cM = T::master;
    m.index = "06"; m.title = "MASTER"; m.accent = cM;
    add (m.big, "masterDrive",   "DRIVE",     cM, KS::Big);
    add (m.big, "masterLowTone", "BASS TONE", cM, KS::Big);
    add (m.big, "outGain",       "OUTPUT",    cM, KS::Big);
}

void KickforgeAudioProcessorEditor::buildBackgroundTexture()
{
    bgTexture = juce::Image (juce::Image::ARGB, 256, 256, true);
    juce::Random rng (0x4b464f52);
    juce::Image::BitmapData bd (bgTexture, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < 256; ++y)
    {
        const float row = rng.nextFloat();
        for (int x = 0; x < 256; ++x)
            bd.setPixelColour (x, y, juce::Colours::white.withAlpha ((0.55f * row + 0.45f * rng.nextFloat()) * 0.035f));
    }
}

KickforgeAudioProcessorEditor::Panel* KickforgeAudioProcessorEditor::layerPanel (int layer)
{
    Panel* ps[4] { &clickP, &punchP, &rumbleP, &gritP };
    return juce::isPositiveAndBelow (layer, 4) ? ps[layer] : nullptr;
}

KickforgeAudioProcessorEditor::Panel* KickforgeAudioProcessorEditor::panelAt (juce::Point<int> pt)
{
    for (auto* p : { &clickP, &punchP, &rumbleP, &gritP })
        if (p->bounds.contains (pt)) return p;
    return nullptr;
}

// =========================================================================
// Timer
// =========================================================================

void KickforgeAudioProcessorEditor::timerCallback()
{
    ++tickCount;
    const auto c = proc.getChangeCounter();
    if (c != lastChange)
    {
        lastChange = c;
        vizDirty = true;
        updateLayerPanels();
        updateLibraryBar();
        updateTempoInfo();
        presetCombo.setTextWhenNothingSelected (proc.getPresetManager().getCurrentName());
    }
    const auto lv = proc.getLibrary().getVersion();
    if (lv != lastLibVersion || (tickCount % 15 == 0 && proc.getLibrary().isScanning()))
    {
        lastLibVersion = lv;
        updateLibraryBar();
        updateLayerPanels();
    }
    if (tickCount % 15 == 0) updateTempoInfo();   // Host-Tempo kann sich ändern

    if (vizDirty && tickCount % 3 == 0)
    {
        vizDirty = false;
        renderVisualizer();
    }

    const float pk = proc.takeOutputPeak();
    meterLevel = juce::jmax (pk, meterLevel * 0.86f);
    if (pk >= meterHold) { meterHold = pk; meterHoldTicks = 36; }
    else if (--meterHoldTicks <= 0) meterHold *= 0.92f;
    repaint (meterBounds.expanded (4));
}

void KickforgeAudioProcessorEditor::renderVisualizer()
{
    if (renderInFlight.exchange (true)) { vizDirty = true; return; }

    juce::Component::SafePointer<KickforgeAudioProcessorEditor> safe (this);
    renderPool.addJob ([this, safe]
    {
        constexpr double sr = 44100.0;
        const auto params = proc.buildParams();
        const int len = juce::jlimit (2048, (int) (sr * 3.0), proc.kickLengthSamples (sr));
        const int beatEnd = (int) (60.0 / juce::jmax (40.0f, params.bpm) * params.lengthBeats * sr);

        auto parts = std::make_shared<juce::AudioBuffer<float>> (KickVisualizer::numLayers, len);
        auto sum   = std::make_shared<juce::AudioBuffer<float>> (1, len);
        juce::AudioBuffer<float> st (2, len);

        auto toMono = [&st, len] (float* dst)
        {
            const float* l = st.getReadPointer (0);
            const float* r = st.getReadPointer (1);
            for (int i = 0; i < len; ++i) dst[i] = 0.5f * (l[i] + r[i]);
        };
        proc.renderOffline (st, sr, -1);
        toMono (sum->getWritePointer (0));
        for (int l = 0; l < KickVisualizer::numLayers; ++l)
        {
            proc.renderOffline (st, sr, l);
            toMono (parts->getWritePointer (l));
        }

        const double bpm = params.bpm;
        const float  beats = params.lengthBeats;
        juce::MessageManager::callAsync ([safe, parts, sum, beatEnd, bpm, beats]
        {
            if (auto* ed = safe.getComponent())
            {
                ed->visualizer.setData (*parts, *sum, sr, beatEnd, bpm, beats);
                ed->renderInFlight = false;
            }
        });
    });
}

// =========================================================================
// Presets / Export / Samples
// =========================================================================

void KickforgeAudioProcessorEditor::refreshPresetCombo()
{
    presetCombo.clear (juce::dontSendNotification);
    juce::String lastCat;
    int id = 1;
    for (auto& e : proc.getPresetManager().getEntries())
    {
        if (e.category != lastCat)
        {
            presetCombo.addSectionHeading (e.category);
            lastCat = e.category;
        }
        presetCombo.addItem (e.name, id++);
    }
}

void KickforgeAudioProcessorEditor::stepPreset (int delta)
{
    const int count = proc.getPresetManager().getEntries().size();
    if (count == 0) return;
    int idx = proc.getPresetManager().getCurrentIndex();
    idx = (idx + delta + count) % count;
    presetCombo.setSelectedId (idx + 1, juce::sendNotification);
}

void KickforgeAudioProcessorEditor::onSavePresetClicked()
{
    auto dir = kf::PresetManager::getUserDir();
    if (! dir.exists()) dir.createDirectory();
    fileChooser = std::make_unique<juce::FileChooser> (
        juce::String::fromUTF8 ("Preset speichern\xe2\x80\xa6"), dir.getChildFile ("Meine Kick.kfp"), "*.kfp");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File()) return;
            if (! file.hasFileExtension ("kfp")) file = file.withFileExtension ("kfp");
            if (proc.getPresetManager().saveUserPreset (file))
            {
                refreshPresetCombo();
                presetCombo.setSelectedId (proc.getPresetManager().getCurrentIndex() + 1, juce::dontSendNotification);
            }
        });
}

void KickforgeAudioProcessorEditor::onExportClicked()
{
    const auto params = proc.buildParams();
    const auto name = proc.getPresetManager().getCurrentName().replaceCharacters (" #", "__")
                    + "_" + noteName (params.fEnd).replace ("#", "s")
                    + "_" + juce::String (juce::roundToInt (params.bpm)) + "BPM";
    auto defaultDir = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
    fileChooser = std::make_unique<juce::FileChooser> ("Kick als WAV exportieren",
                                                       defaultDir.getChildFile ("Kickforge_" + name + ".wav"), "*.wav");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File()) return;
            if (! file.hasFileExtension ("wav")) file = file.withFileExtension ("wav");

            constexpr double sr = 44100.0;
            const int n = proc.kickLengthSamples (sr);
            juce::AudioBuffer<float> buf (2, n);
            proc.renderOffline (buf, sr, -1);

            file.deleteFile();
            std::unique_ptr<juce::FileOutputStream> fos (file.createOutputStream());
            if (fos == nullptr) return;
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (fos.get(), sr, 2u, 24, {}, 0));
            if (writer != nullptr)
            {
                fos.release();
                writer->writeFromAudioSampleBuffer (buf, 0, n);
            }
        });
}

void KickforgeAudioProcessorEditor::onLoadSampleClicked (int layer)
{
    const auto prev = proc.getLayerSamplePath (layer);
    auto start = prev.isNotEmpty() ? juce::File (prev).getParentDirectory()
                                   : juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
    fileChooser = std::make_unique<juce::FileChooser> ("Sample laden", start, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, layer] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File() && proc.loadLayerSample (layer, file))
            {
                updateLayerPanels();
                if (proc.apvts.getRawParameterValue (autoplayId)->load() >= 0.5f) proc.triggerAudition();
            }
        });
}

void KickforgeAudioProcessorEditor::showSampleMenu (int layer)
{
    using namespace juce;
    auto& lib = proc.getLibrary();
    const auto list = lib.entriesForLayer (layer);
    const auto entries = lib.getEntries();
    const int cur = lib.indexOfFile (File (proc.getLayerSamplePath (layer)));

    PopupMenu menu;
    if (list.empty())
    {
        menu.addItem (1, lib.getStatusText(), false);
        menu.addItem (2, String::fromUTF8 ("Bibliotheksordner w\xc3\xa4hlen\xe2\x80\xa6"));
    }
    else
    {
        // Gruppen (Unterordner) als Untermenüs, große Gruppen in 25er-Blöcken
        String group; PopupMenu sub; int inGroup = 0; std::vector<std::pair<String, PopupMenu>> groups;
        auto flush = [&] { if (inGroup > 0) groups.push_back ({ group, sub }); sub = PopupMenu(); inGroup = 0; };
        for (int idx : list)
        {
            const auto& e = entries[(size_t) idx];
            if (e.group != group) { flush(); group = e.group; }
            String label = e.display;
            if (e.rootHz > 0.0f) label << "   " << noteName (e.rootHz);
            sub.addItem (1000 + idx, label, true, idx == cur);
            ++inGroup;
        }
        flush();
        for (auto& [name, m] : groups)
            menu.addSubMenu (name + "  (" + String (m.getNumItems()) + ")", m);
    }
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&layerPanel (layer)->nameBtn)
                                            .withMaximumNumColumns (4),
        [this, layer] (int r)
        {
            if (r == 2) { showLibraryMenu(); return; }
            if (r >= 1000) { proc.loadLibraryEntry (layer, r - 1000); updateLayerPanels(); }
        });
}

void KickforgeAudioProcessorEditor::showKitMenu()
{
    using namespace juce;
    const auto kits = proc.getLibrary().getKits();
    PopupMenu menu;
    if (kits.empty()) menu.addItem (1, "Keine Kits in der Bibliothek", false);
    // Blöcke zu 20 Kits als Untermenüs
    PopupMenu sub; String first; int count = 0;
    auto flush = [&] (const String& last)
    {
        if (count > 0) menu.addSubMenu (first + String::fromUTF8 (" \xe2\x80\x93 ") + last, sub);
        sub = PopupMenu(); count = 0;
    };
    String lastName, lastFamily;
    for (int i = 0; i < (int) kits.size(); ++i)
    {
        const auto family = kits[(size_t) i].name.upToLastOccurrenceOf (" ", false, false);
        if (count == 20 || (count > 0 && family != lastFamily)) flush (lastName);
        if (count == 0) first = kits[(size_t) i].name;
        String label = kits[(size_t) i].name;
        if (kits[(size_t) i].rootHz > 0.0f) label << "   " << noteName (kits[(size_t) i].rootHz);
        sub.addItem (100 + i, label, true, i == proc.getCurrentKit());
        lastName = kits[(size_t) i].name; lastFamily = family; ++count;
    }
    flush (lastName);
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&kitNameBtn),
        [this] (int r)
        {
            if (r >= 100) { proc.loadKit (r - 100); updateLayerPanels(); updateLibraryBar(); }
        });
}

void KickforgeAudioProcessorEditor::showLibraryMenu()
{
    using namespace juce;
    const auto roots = proc.getLibrary().getRoots();
    PopupMenu menu;
    menu.addItem (1, String::fromUTF8 ("Ordner hinzuf\xc3\xbc" "gen\xe2\x80\xa6"));
    menu.addItem (2, "Neu einlesen", ! roots.isEmpty());
    if (! roots.isEmpty())
    {
        menu.addSeparator();
        menu.addSectionHeader ("ORDNER");
        for (int i = 0; i < roots.size(); ++i)
        {
            PopupMenu sub;
            sub.addItem (100 + i, "Aus der Bibliothek entfernen");
            menu.addSubMenu (File (roots[i]).getFileName(), sub);
        }
    }
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&libMenuBtn),
        [this, roots] (int r)
        {
            auto& lib = proc.getLibrary();
            if (r == 1)
            {
                fileChooser = std::make_unique<FileChooser> (String::fromUTF8 ("Sample-Ordner w\xc3\xa4hlen (z. B. Teknovault Kick Builder)"),
                                                             File::getSpecialLocation (File::userHomeDirectory).getChildFile ("Downloads"));
                fileChooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                    [this] (const FileChooser& fc)
                    {
                        const auto dir = fc.getResult();
                        if (dir.isDirectory()) { proc.getLibrary().addRoot (dir); updateLibraryBar(); }
                    });
            }
            else if (r == 2) lib.rescan();
            else if (r >= 100 && r - 100 < roots.size())
            {
                auto rr = roots;
                rr.remove (r - 100);
                lib.setRoots (rr);
            }
            updateLibraryBar();
        });
}

void KickforgeAudioProcessorEditor::updateLayerPanels()
{
    auto& lib = proc.getLibrary();
    const bool libEmpty = lib.getEntries().empty();
    for (int l = 0; l < 4; ++l)
    {
        auto& p = *layerPanel (l);
        const auto path = proc.getLayerSamplePath (l);
        const bool hasSample  = path.isNotEmpty();
        const bool sampleMode = proc.apvts.getRawParameterValue (srcId (l))->load() >= 0.5f;
        const bool on = proc.apvts.getRawParameterValue (onId (l))->load() >= 0.5f;

        p.sampleView = sampleMode;
        p.synthBtn .setToggleState (! sampleMode, juce::dontSendNotification);
        p.sampleBtn.setToggleState (sampleMode,   juce::dontSendNotification);
        for (auto* v : { &p.big, &p.med, &p.small }) for (auto& k : *v) { k->setVisible (! sampleMode); k->setDimmed (! on); }
        for (auto* v : { &p.sBig, &p.sMed })         for (auto& k : *v) { k->setVisible (sampleMode);   k->setDimmed (! on); }

        juce::String name;
        if (hasSample) name = proc.getLayerSampleName (l);
        else if (lib.isScanning()) name = juce::String::fromUTF8 ("Bibliothek wird eingelesen\xe2\x80\xa6");
        else if (libEmpty) name = juce::String::fromUTF8 ("Keine Bibliothek \xe2\x80\x93 oben w\xc3\xa4hlen");
        else name = juce::String::fromUTF8 ("Sample w\xc3\xa4hlen \xe2\x96\xbe");
        p.nameBtn.setButtonText (name);
        p.clearBtn.setEnabled (hasSample);
        for (auto* b : { &p.prevBtn, &p.nextBtn, &p.randBtn }) b->setEnabled (! libEmpty);

        // Info: Grundton + KEY LOCK
        juce::String info;
        if (sampleMode)
        {
            if (! hasSample) info = "kein Sample - Synth aktiv";
            else
            {
                const float root = proc.getLayerRootHz (l);
                if (root > 0.0f) info << "GRUNDTON " << noteName (root);
                const float sh = proc.getKeyLockShift (l);
                if (std::abs (sh) > 0.05f) info << (info.isEmpty() ? juce::String() : juce::String::fromUTF8 ("  \xc2\xb7  ")) << "KEY LOCK " << (sh > 0 ? "+" : "") << juce::String (sh, 1) << " st";
            }
        }
        p.info.setText (info, juce::dontSendNotification);
        p.info.setVisible (sampleMode);
    }
    repaint();
}

void KickforgeAudioProcessorEditor::updateLibraryBar()
{
    auto& lib = proc.getLibrary();
    const auto roots = lib.getRoots();
    juce::String s = lib.getStatusText();
    if (! roots.isEmpty() && ! lib.isScanning())
        s = juce::File (roots[0]).getFileName() + (roots.size() > 1 ? " +" + juce::String (roots.size() - 1) : juce::String())
          + juce::String::fromUTF8 ("  \xc2\xb7  ") + s;
    libStatus.setText (s, juce::dontSendNotification);

    const auto kits = lib.getKits();
    const int k = proc.getCurrentKit();
    kitNameBtn.setButtonText (juce::isPositiveAndBelow (k, (int) kits.size()) ? kits[(size_t) k].name
                              : (kits.empty() ? juce::String ("KEINE KITS") : juce::String::fromUTF8 ("KIT W\xc3\x84HLEN \xe2\x96\xbe")));
    const bool hasKits = ! kits.empty();
    for (auto* b : { &kitPrevBtn, &kitNextBtn, &kitNameBtn }) b->setEnabled (hasKits);
    randomKickBtn.setEnabled (! lib.getEntries().empty());
}

void KickforgeAudioProcessorEditor::updateTempoInfo()
{
    const bool sync = proc.apvts.getRawParameterValue (syncId)->load() >= 0.5f;
    const double bpm = proc.getEffectiveBpm();
    const bool fromHost = proc.isUsingHostTempo();
    tempoInfo.setText (fromHost ? "DAW: " + juce::String (bpm, 1) + " BPM"
                                : (sync ? juce::String ("kein DAW-Tempo, manuell") : juce::String ("manuelles Tempo")),
                       juce::dontSendNotification);
    kickP.med.back()->setDimmed (fromHost);   // TEMPO-Knob (letzter) gilt nur ohne DAW-Tempo
}

// =========================================================================
// Drag & Drop
// =========================================================================

bool KickforgeAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3")) return true;
    return false;
}

void KickforgeAudioProcessorEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    dragPos = { -1, -1 };
    if (auto* p = panelAt ({ x, y }))
        for (auto& f : files)
            if (proc.loadLayerSample (p->layer, juce::File (f))) break;
    updateLayerPanels();
    repaint();
}

// =========================================================================
// Zeichnen
// =========================================================================

void KickforgeAudioProcessorEditor::drawPanel (juce::Graphics& g, const Panel& p) const
{
    using namespace juce;
    auto r = p.bounds.toFloat();
    constexpr float cut = 12.0f;
    const auto shape = T::chamferRect (r.reduced (0.5f), cut);

    g.setGradientFill (ColourGradient (T::panel, 0.0f, r.getY(), T::panelLo, 0.0f, r.getBottom(), false));
    g.fillPath (shape);
    g.setColour (Colours::white.withAlpha (0.045f));
    g.drawLine (r.getX() + cut + 1.0f, r.getY() + 1.5f, r.getRight() - 1.0f, r.getY() + 1.5f, 1.0f);
    g.setColour (T::edge);
    g.strokePath (shape, PathStrokeType (1.0f));

    Path bar;
    bar.startNewSubPath (r.getX() + 1.0f, r.getY() + cut + 0.5f);
    bar.lineTo (r.getX() + cut + 0.5f, r.getY() + 1.0f);
    bar.lineTo (r.getX() + cut + 52.0f, r.getY() + 1.0f);
    g.setColour (p.accent);
    g.strokePath (bar, PathStrokeType (2.5f, PathStrokeType::mitered, PathStrokeType::square));

    auto titleArea = p.bounds.withHeight (36).withTrimmedLeft (18);
    g.setColour (T::faint);
    g.setFont (lnf.fonts().get (W::SemiBold, 12.0f, 0.1f));
    g.drawText (p.index, titleArea.removeFromLeft (24), Justification::centredLeft, false);
    g.setColour (p.accent);
    g.setFont (lnf.fonts().get (W::Bold, 15.5f, 0.18f));
    g.drawText (p.title, titleArea, Justification::centredLeft, false);

    if (p.dividerY > 0)
    {
        g.setColour (T::edge);
        g.drawHorizontalLine (p.dividerY, r.getX() + 12.0f, r.getRight() - 12.0f);
    }
}

void KickforgeAudioProcessorEditor::drawScrew (juce::Graphics& g, juce::Point<float> c, float angle) const
{
    using namespace juce;
    constexpr float r = 3.6f;
    g.setGradientFill (ColourGradient (Colour (0xff4a4440), c.x, c.y - r, Colour (0xff1a1817), c.x, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (Colour (0xff0d0c0b));
    const auto d = Point<float> (std::cos (angle), std::sin (angle)) * (r * 0.75f);
    g.drawLine ({ c - d, c + d }, 1.2f);
}

void KickforgeAudioProcessorEditor::drawHeader (juce::Graphics& g) const
{
    using namespace juce;
    const Rectangle<float> emb (16.0f, 3.0f, 58.0f, 58.0f);
    if (emblem != nullptr)
        emblem->drawWithin (g, emb, RectanglePlacement::centred, 1.0f);

    const auto wordFont = lnf.fonts().get (W::Bold, 33.0f, 0.06f);
    const float x0    = 78.0f;
    const float kickW = GlyphArrangement::getStringWidth (wordFont, "KICK");
    const float forgW = GlyphArrangement::getStringWidth (wordFont, "FORGE");
    g.setFont (wordFont);
    g.setColour (T::text);
    g.drawText ("KICK",  Rectangle<float> (x0, 6.0f, kickW + 4.0f, 34.0f), Justification::centredLeft, false);
    g.setColour (T::brand);
    g.drawText ("FORGE", Rectangle<float> (x0 + kickW, 6.0f, forgW + 4.0f, 34.0f), Justification::centredLeft, false);

    const String tag = String::fromUTF8 ("HARDTECHNO \xc2\xb7 INDUSTRIAL \xc2\xb7 SCHRANZ");
    const float wordW = kickW + forgW - 33.0f * 0.06f;
    auto tagFont = lnf.fonts().get (W::SemiBold, 9.5f, 0.24f);
    const float tagW = GlyphArrangement::getStringWidth (tagFont, tag);
    if (tagW > 1.0f)
        tagFont = lnf.fonts().get (W::SemiBold, jlimit (7.5f, 11.0f, 9.5f * wordW / tagW), 0.24f);
    g.setFont (tagFont);
    g.setColour (T::dim);
    g.drawText (tag, Rectangle<float> (x0 + 1.0f, 40.0f, wordW + 20.0f, 13.0f), Justification::centredLeft, false);

    g.setColour (T::faint);
    g.setFont (lnf.fonts().get (W::SemiBold, 10.0f, 0.1f));
    g.drawText ("v" JucePlugin_VersionString, Rectangle<float> (x0 + wordW + 8.0f, 12.0f, 50.0f, 12.0f),
                Justification::centredLeft, false);

    const auto plate = T::chamferRect (presetPlate.toFloat().reduced (0.5f), 8.0f);
    g.setColour (T::recess);
    g.fillPath (plate);
    g.setColour (T::edge);
    g.strokePath (plate, PathStrokeType (1.0f));
    g.setColour (T::dim);
    g.setFont (lnf.fonts().get (W::Bold, 10.5f, 0.18f));
    g.drawText ("PRESET", presetPlate.withTrimmedLeft (14).withWidth (60), Justification::centredLeft, false);
}

void KickforgeAudioProcessorEditor::drawMeter (juce::Graphics& g) const
{
    using namespace juce;
    if (meterBounds.isEmpty()) return;
    auto bar = meterBounds.toFloat();
    g.setColour (T::recess);
    g.fillRect (bar);
    g.setColour (T::edge);
    g.drawRect (bar, 1.0f);

    auto norm = [] (float gain)
    {
        const float db = Decibels::gainToDecibels (gain, -60.0f);
        return jlimit (0.0f, 1.0f, (db + 48.0f) / 51.0f);
    };
    auto inner = bar.reduced (2.0f);
    constexpr float segH = 3.0f, segGap = 1.0f;
    const int nSeg = (int) (inner.getHeight() / (segH + segGap));
    const float lit = norm (meterLevel);
    for (int i = 0; i < nSeg; ++i)
    {
        const float pos = (float) (i + 1) / (float) nSeg;
        const float y   = inner.getBottom() - (float) (i + 1) * (segH + segGap) + segGap;
        const float db  = pos * 51.0f - 48.0f;
        const auto  col = db > -1.0f ? Colour (0xffff3b1f) : db > -12.0f ? T::brand : T::rumble;
        g.setColour (pos <= lit ? col : Colour (0xff1c1a19));
        g.fillRect (inner.getX(), y, inner.getWidth(), segH);
    }
    if (meterHold > 0.0005f)
    {
        const float y = inner.getBottom() - norm (meterHold) * inner.getHeight();
        g.setColour (T::text);
        g.fillRect (inner.getX(), y - 1.0f, inner.getWidth(), 2.0f);
    }
    g.setFont (lnf.fonts().get (W::SemiBold, 9.5f, 0.0f));
    for (float db : { 0.0f, -12.0f, -24.0f, -48.0f })
    {
        const float y = inner.getBottom() - (db + 48.0f) / 51.0f * inner.getHeight();
        g.setColour (T::faint);
        g.drawText (db > -0.5f ? String ("0") : String ((int) db),
                    Rectangle<float> (bar.getX() - 26.0f, y - 6.0f, 22.0f, 12.0f), Justification::centredRight, false);
    }
}

void KickforgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.setGradientFill (ColourGradient (T::bgTop, 0.0f, 0.0f, T::bg, 0.0f, (float) getHeight(), false));
    g.fillAll();
    g.setTiledImageFill (bgTexture, 0, 0, 1.0f);
    g.fillAll();
    g.setGradientFill (ColourGradient (T::brand.withAlpha (0.06f), (float) getWidth() * 0.5f, -80.0f,
                                       Colours::transparentBlack, (float) getWidth() * 0.5f, 220.0f, true));
    g.fillRect (getLocalBounds().removeFromTop (240));

    const float W_ = (float) getWidth(), H_ = (float) getHeight();
    drawScrew (g, { 7.0f, 7.0f }, 0.6f);
    drawScrew (g, { W_ - 7.0f, 7.0f }, 2.1f);
    drawScrew (g, { 7.0f, H_ - 7.0f }, 1.2f);
    drawScrew (g, { W_ - 7.0f, H_ - 7.0f }, 0.2f);

    drawHeader (g);

    // Bibliotheks-Leiste: eingelassene Platte mit Beschriftungen
    {
        const auto plate = T::chamferRect (libBar.toFloat().reduced (0.5f), 7.0f);
        g.setColour (T::recess);
        g.fillPath (plate);
        g.setColour (T::edge);
        g.strokePath (plate, juce::PathStrokeType (1.0f));
        g.setFont (lnf.fonts().get (W::Bold, 10.5f, 0.18f));
        g.setColour (T::brand);
        g.drawText ("LIBRARY", libBar.withTrimmedLeft (14).withWidth (70), juce::Justification::centredLeft, false);
        g.setColour (T::dim);
        g.drawText ("KIT", juce::Rectangle<int> (kitPrevBtn.getX() - 34, libBar.getY(), 30, libBar.getHeight()),
                    juce::Justification::centredRight, false);
    }

    for (auto* p : { &clickP, &punchP, &rumbleP, &gritP, &kickP, &masterP })
        drawPanel (g, *p);

    // Warnstreifen im Master-Titel
    {
        auto stripe = Rectangle<int> (masterP.bounds.getRight() - 18 - 56, masterP.bounds.getY() + 14, 56, 8);
        g.saveState();
        g.reduceClipRegion (stripe);
        g.setColour (T::master.withAlpha (0.85f));
        g.fillRect (stripe);
        g.setColour (T::bg);
        for (int x = stripe.getX() - 16; x < stripe.getRight() + 16; x += 10)
        {
            Path p;
            p.addQuadrilateral ((float) x, (float) stripe.getBottom(), (float) x + 5.0f, (float) stripe.getBottom(),
                                (float) x + 13.0f, (float) stripe.getY(), (float) x + 8.0f, (float) stripe.getY());
            g.fillPath (p);
        }
        g.restoreState();
    }
    drawMeter (g);
}

void KickforgeAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    // Drop-Ziel beim Ziehen einer Datei hervorheben
    if (dragPos.x < 0) return;
    if (auto* p = panelAt (dragPos))
    {
        const auto r = p->bounds.toFloat().reduced (1.5f);
        g.setColour (p->accent.withAlpha (0.10f));
        g.fillPath (T::chamferRect (r, 12.0f));
        g.setColour (p->accent);
        g.strokePath (T::chamferRect (r, 12.0f), juce::PathStrokeType (2.0f));
        g.setFont (lnf.fonts().get (W::Bold, 14.0f, 0.16f));
        g.drawText (juce::String::fromUTF8 ("SAMPLE \xe2\x86\x92 ") + p->title, p->bounds.withTrimmedTop (36).removeFromTop (24),
                    juce::Justification::centred, false);
    }
}

// =========================================================================
// Layout
// =========================================================================

void KickforgeAudioProcessorEditor::placeKnob (Knob& k, juce::Rectangle<int> cell)
{
    const int knobD  = k.size == KnobSize::Big ? 78 : k.size == KnobSize::Med ? 54 : 40;
    const int labelH = k.size == KnobSize::Big ? 15 : 14;
    const int textH  = k.slider.getTextBoxHeight();
    const int sliderH = juce::jmin (cell.getHeight() - labelH, knobD + textH);
    auto block = cell.withSizeKeepingCentre (cell.getWidth(), labelH + sliderH);
    k.label.setBounds (block.removeFromTop (labelH));
    const int sliderW = juce::jmin (block.getWidth(), knobD + 40);
    k.slider.setBounds (block.withSizeKeepingCentre (sliderW, sliderH));
}

void KickforgeAudioProcessorEditor::layoutRow (std::vector<std::unique_ptr<Knob>>& knobs,
                                               juce::Rectangle<int> row, int slots)
{
    // feste Spaltenbreite → gleiche Raster über alle Panels; zentriert, wenn weniger Knobs als Slots
    const int n = (int) knobs.size();
    if (n == 0) return;
    slots = juce::jmax (slots, n);
    const int cellW = row.getWidth() / slots;
    auto r = row.withSizeKeepingCentre (cellW * n, row.getHeight());
    for (auto& k : knobs) placeKnob (*k, r.removeFromLeft (cellW));
}

void KickforgeAudioProcessorEditor::layoutLayerPanel (Panel& p)
{
    p.onToggle->button.setBounds (p.bounds.getRight() - 14 - 54, p.bounds.getY() + 8, 54, 20);
    auto inner = p.bounds.withTrimmedTop (38).reduced (12, 6);
    const bool narrow = p.bounds.getWidth() < 260;

    // Zeile 1: SYNTH | SAMPLE   …   LOAD ✕
    auto r1 = inner.removeFromTop (24);
    p.synthBtn .setBounds (r1.removeFromLeft (narrow ? 58 : 66));
    r1.removeFromLeft (2);
    p.sampleBtn.setBounds (r1.removeFromLeft (narrow ? 64 : 72));
    p.clearBtn.setBounds (r1.removeFromRight (26));
    r1.removeFromRight (4);
    if (! narrow) p.loadBtn.setBounds (r1.removeFromRight (54));
    else          p.loadBtn.setBounds ({});
    inner.removeFromTop (6);

    // Zeile 2: ◀ Name ▶ RND
    auto r2 = inner.removeFromTop (26);
    p.prevBtn.setBounds (r2.removeFromLeft (26));
    r2.removeFromLeft (3);
    p.randBtn.setBounds (r2.removeFromRight (narrow ? 36 : 42));
    r2.removeFromRight (3);
    p.nextBtn.setBounds (r2.removeFromRight (26));
    r2.removeFromRight (3);
    p.nameBtn.setBounds (r2);
    inner.removeFromTop (8);
    p.dividerY = inner.getY() - 4;

    // Synth-Regler
    {
        auto c = inner;
        layoutRow (p.big, c.removeFromTop (108), 3);
        c.removeFromTop (2);
        layoutRow (p.med, c.removeFromTop (82), narrow ? 2 : 4);
        c.removeFromTop (2);
        if (! narrow) layoutRow (p.small, c.removeFromTop (70), 5);
    }
    // Sample-Regler
    {
        auto c = inner;
        layoutRow (p.sBig, c.removeFromTop (108), narrow ? 2 : 3);
        c.removeFromTop (2);
        layoutRow (p.sMed, c.removeFromTop (82), 3);
        c.removeFromTop (6);
        p.info.setBounds (c.removeFromTop (18));
    }
}

void KickforgeAudioProcessorEditor::resized()
{
    using namespace layout;
    if (masterP.big.size() < 3 || kickP.med.size() < 6 || gritP.sMed.size() < 3 || keylockToggle == nullptr) return;

    const int W_ = getWidth();
    const int availW = W_ - pad * 2;

    // ---- Header ----
    presetPlate = { 400, 12, 520, 38 };
    {
        auto r = presetPlate.reduced (6, 6).withTrimmedLeft (64);
        savePresetBtn.setBounds (r.removeFromRight (66));
        r.removeFromRight (8);
        prevPresetBtn.setBounds (r.removeFromLeft (28));
        r.removeFromLeft (4);
        nextPresetBtn.setBounds (r.removeFromRight (28));
        r.removeFromRight (4);
        presetCombo.setBounds (r);
    }
    triggerBtn.setBounds (W_ - pad - 140, 12, 140, 38);
    exportBtn .setBounds (W_ - pad - 140 - gap - 128, 12, 128, 38);

    visualizer.setBounds (pad, vizY, availW, vizH);

    // ---- Bibliotheks-Leiste ----
    libBar = { pad, libY, availW, libH };
    {
        auto r = libBar.reduced (6, 3);
        r.removeFromLeft (78);                                   // "LIBRARY"-Beschriftung (gezeichnet)
        libMenuBtn.setBounds (r.removeFromLeft (118));
        r.removeFromLeft (10);
        autoplayToggle->button.setBounds (r.removeFromRight (112));
        r.removeFromRight (6);
        keylockToggle->button.setBounds (r.removeFromRight (104));
        r.removeFromRight (12);
        randomKickBtn.setBounds (r.removeFromRight (92));
        r.removeFromRight (12);
        kitNextBtn.setBounds (r.removeFromRight (26));
        r.removeFromRight (3);
        kitNameBtn.setBounds (r.removeFromRight (150));
        r.removeFromRight (3);
        kitPrevBtn.setBounds (r.removeFromRight (26));
        r.removeFromRight (36);                                  // "KIT"-Beschriftung (gezeichnet)
        libStatus.setBounds (r);
    }

    // ---- Schichten ----
    const int restW = availW - clickW - 3 * gap;
    const int colW  = restW / 3;
    int x = pad;
    clickP .bounds = { x, layersY, clickW, layersH };              x += clickW + gap;
    punchP .bounds = { x, layersY, colW, layersH };                x += colW + gap;
    rumbleP.bounds = { x, layersY, colW, layersH };                x += colW + gap;
    gritP  .bounds = { x, layersY, pad + availW - x, layersH };
    for (auto* p : { &clickP, &punchP, &rumbleP, &gritP })
        layoutLayerPanel (*p);

    // ---- Untere Reihe: KICK | MASTER ----
    const int bottomH = getHeight() - bottomY - pad;
    kickP  .bounds = { pad, bottomY, availW - masterW - gap, bottomH };
    masterP.bounds = { pad + availW - masterW, bottomY, masterW, bottomH };

    {
        auto in = kickP.bounds.withTrimmedTop (38).reduced (14, 6);
        auto toggles = in.removeFromRight (150);
        in.removeFromRight (8);
        auto bigArea = in.removeFromLeft (220);
        layoutRow (kickP.big, bigArea.withSizeKeepingCentre (bigArea.getWidth(), 112), 2);
        in.removeFromLeft (8);
        layoutRow (kickP.med, in.withSizeKeepingCentre (in.getWidth(), 86), 6);

        auto t = toggles.withSizeKeepingCentre (toggles.getWidth(), 92);
        syncToggle->button.setBounds (t.removeFromTop (24));
        t.removeFromTop (2);
        tempoInfo.setBounds (t.removeFromTop (20).withTrimmedLeft (2));
        t.removeFromTop (14);
        keytrackToggle->button.setBounds (t.removeFromTop (24));
    }
    {
        auto in = masterP.bounds.withTrimmedTop (38).reduced (14, 6);
        meterBounds = in.removeFromRight (14).reduced (0, 4);
        in.removeFromRight (34);
        layoutRow (masterP.big, in.withSizeKeepingCentre (in.getWidth(), 112), 3);
    }
}
