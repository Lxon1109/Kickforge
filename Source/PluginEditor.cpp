#include "PluginEditor.h"
#include "BinaryData.h"

namespace T = kf::theme;
using W = T::Weight;

namespace layout
{
    constexpr int width   = 1240;
    constexpr int height  = 824;
    constexpr int pad     = 14;
    constexpr int gap     = 8;
    constexpr int vizY    = 66,  vizH    = 132;
    constexpr int layersY = 210, layersH = 396;
    constexpr int bottomY = 618;
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
    s.setTextBoxStyle (Slider::TextBoxBelow, false, 90, size == KnobSize::Small ? 15 : 17);
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

    // Doppelklick = Default-Wert
    if (auto* p = proc.apvts.getParameter (id))
        s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    return k;
}

std::unique_ptr<KickforgeAudioProcessorEditor::ToggleCtrl>
KickforgeAudioProcessorEditor::makeToggle (const juce::String& id, const juce::String& text, juce::Colour accent)
{
    auto t = std::make_unique<ToggleCtrl>();
    t->button.setButtonText (text);
    t->button.setColour (juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible (t->button);
    t->att = std::make_unique<BAtt> (proc.apvts, id, t->button);
    return t;
}

std::unique_ptr<KickforgeAudioProcessorEditor::ChoiceCtrl>
KickforgeAudioProcessorEditor::makeChoice (const juce::String& id, const juce::String& text, juce::Colour accent)
{
    using namespace juce;
    auto c = std::make_unique<ChoiceCtrl>();
    if (auto* p = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id)))
    {
        int idx = 1;
        for (auto& s : p->choices)
            c->combo.addItem (s.toUpperCase(), idx++);
    }
    c->combo.setColour (ComboBox::arrowColourId, accent);
    addAndMakeVisible (c->combo);

    c->label.setText (text, dontSendNotification);
    c->label.setJustificationType (Justification::centredLeft);
    c->label.setFont (lnf.fonts().get (W::Bold, 10.5f, 0.14f));
    c->label.setColour (Label::textColourId, T::dim);
    addAndMakeVisible (c->label);

    c->att = std::make_unique<CAtt> (proc.apvts, id, c->combo);
    return c;
}

// =========================================================================
// Konstruktor
// =========================================================================

KickforgeAudioProcessorEditor::KickforgeAudioProcessorEditor (KickforgeAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    // setSize() erst ganz am Ende: es triggert resized(), das alle Komponenten braucht.
    buildBackgroundTexture();
    buildHeader();
    buildLayerPanels();
    buildBottomRow();

    addAndMakeVisible (waveform);

    // LookAndFeel erst setzen, wenn alle Kinder existieren → alle bekommen es mit
    setLookAndFeel (&lnf);
    setResizable (false, false);
    setSize (layout::width, layout::height);

    renderWaveform();
    startTimerHz (30);
}

KickforgeAudioProcessorEditor::~KickforgeAudioProcessorEditor()
{
    stopTimer();
    renderPool.removeAllJobs (false, 2000);   // laufenden Render fertig werden lassen
    setLookAndFeel (nullptr);
}

void KickforgeAudioProcessorEditor::buildHeader()
{
    using namespace juce;

    if (auto xml = XmlDocument::parse (String::fromUTF8 (BinaryData::emblem_svg, BinaryData::emblem_svgSize)))
        emblem = Drawable::createFromSVG (*xml);

    presetCombo.setTextWhenNothingSelected ("(init)");
    presetCombo.setColour (ComboBox::arrowColourId, T::brand);
    addAndMakeVisible (presetCombo);
    refreshPresetCombo();
    presetCombo.setSelectedId (proc.getPresetManager().getCurrentIndex() + 1, dontSendNotification);
    presetCombo.onChange = [this]
    {
        const int idx = presetCombo.getSelectedId() - 1;
        if (idx >= 0 && proc.getPresetManager().loadByIndex (idx))
            proc.getPresetManager().setCurrentIndex (idx);
    };

    for (auto* b : { &prevPresetBtn, &nextPresetBtn, &savePresetBtn, &exportBtn, &triggerBtn,
                     &loadSampleBtn, &clearSampleBtn })
        addAndMakeVisible (*b);

    triggerBtn.getProperties().set ("primary", true);

    prevPresetBtn .onClick = [this] { loadSelectedPreset (-1); };
    nextPresetBtn .onClick = [this] { loadSelectedPreset (+1); };
    savePresetBtn .onClick = [this] { onSavePresetClicked(); };
    exportBtn     .onClick = [this] { onExportClicked(); };
    triggerBtn    .onClick = [this] { proc.triggerAudition(); };
    loadSampleBtn .onClick = [this] { onLoadSampleClicked(); };
    clearSampleBtn.onClick = [this] { onClearSampleClicked(); };

    sampleNameLabel.setFont (lnf.fonts().get (W::SemiBold, 11.5f, 0.02f));
    sampleNameLabel.setColour (Label::textColourId, T::dim);
    sampleNameLabel.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (sampleNameLabel);
    updateSampleLabel();
}

void KickforgeAudioProcessorEditor::buildLayerPanels()
{
    using KS = KnobSize;

    // ---------- 01 CLICK ----------
    auto& c = clickPanel;
    c.index = "01"; c.title = "CLICK"; c.accent = T::click; c.bigCount = 2;
    c.onToggle  = makeToggle ("click_on",   "ON",  T::click);
    c.modeCombo = makeChoice ("click_mode", "OSC", T::click);
    c.knobs.push_back (makeKnob ("click_tune", "TUNE",  T::click, KS::Big));
    c.knobs.push_back (makeKnob ("click_dec",  "DECAY", T::click, KS::Big));
    c.knobs.push_back (makeKnob ("click_drv",  "DRIVE", T::click, KS::Med));
    c.knobs.push_back (makeKnob ("click_lvl",  "LEVEL", T::click, KS::Med));
    c.shaperCombo     = makeChoice ("click_shp",   "SHAPE",  T::click);
    c.filterModeCombo = makeChoice ("click_fmode", "FILTER", T::click);
    c.filterCutKnob   = makeKnob   ("click_fcut",  "CUTOFF", T::click, KS::Small);
    c.filterResKnob   = makeKnob   ("click_fres",  "RESO",   T::click, KS::Small);
    c.delayKnob       = makeKnob   ("click_dly",   "DELAY",  T::click, KS::Small);

    // ---------- 02 BODY ----------
    auto& b = bodyPanel;
    b.index = "02"; b.title = "BODY"; b.accent = T::body; b.bigCount = 3;
    b.modeCombo = makeChoice ("body_mode", "OSC", T::body);
    b.knobs.push_back (makeKnob ("body_tune", "TUNE",    T::body, KS::Big));
    b.knobs.push_back (makeKnob ("body_pamt", "PITCH",   T::body, KS::Big));
    b.knobs.push_back (makeKnob ("body_pdec", "P.DECAY", T::body, KS::Big));
    b.knobs.push_back (makeKnob ("body_adec", "DECAY",   T::body, KS::Med));
    b.knobs.push_back (makeKnob ("body_drv",  "DRIVE",   T::body, KS::Med));
    b.knobs.push_back (makeKnob ("body_lvl",  "LEVEL",   T::body, KS::Med));
    b.shaperCombo     = makeChoice ("body_shp",   "SHAPE",  T::body);
    b.filterModeCombo = makeChoice ("body_fmode", "FILTER", T::body);
    b.filterCutKnob   = makeKnob   ("body_fcut",  "CUTOFF", T::body, KS::Small);
    b.filterResKnob   = makeKnob   ("body_fres",  "RESO",   T::body, KS::Small);
    b.delayKnob       = makeKnob   ("body_dly",   "DELAY",  T::body, KS::Small);

    // ---------- 03 TAIL ----------
    auto& t = tailPanel;
    t.index = "03"; t.title = "TAIL"; t.accent = T::tail; t.bigCount = 3;
    t.onToggle = makeToggle ("tail_on", "ON", T::tail);
    t.fixedOscText = juce::String::fromUTF8 ("SINE  \xc2\xb7  SUB");
    t.knobs.push_back (makeKnob ("tail_tune", "TUNE",    T::tail, KS::Big));
    t.knobs.push_back (makeKnob ("tail_adec", "DECAY",   T::tail, KS::Big));
    t.knobs.push_back (makeKnob ("tail_pamt", "PITCH",   T::tail, KS::Big));
    t.knobs.push_back (makeKnob ("tail_pdec", "P.DECAY", T::tail, KS::Med));
    t.knobs.push_back (makeKnob ("tail_drv",  "DRIVE",   T::tail, KS::Med));
    t.knobs.push_back (makeKnob ("tail_lvl",  "LEVEL",   T::tail, KS::Med));
    t.shaperCombo     = makeChoice ("tail_shp",   "SHAPE",  T::tail);
    t.filterModeCombo = makeChoice ("tail_fmode", "FILTER", T::tail);
    t.filterCutKnob   = makeKnob   ("tail_fcut",  "CUTOFF", T::tail, KS::Small);
    t.filterResKnob   = makeKnob   ("tail_fres",  "RESO",   T::tail, KS::Small);
    t.delayKnob       = makeKnob   ("tail_dly",   "DELAY",  T::tail, KS::Small);
}

void KickforgeAudioProcessorEditor::buildBottomRow()
{
    using KS = KnobSize;

    eqOnToggle = makeToggle ("eq_on", "ON", T::fx);
    eqKnobs.push_back (makeKnob ("eq_lo_f", "FREQ", T::fx, KS::Med));
    eqKnobs.push_back (makeKnob ("eq_lo_g", "GAIN", T::fx, KS::Med, true));
    eqKnobs.push_back (makeKnob ("eq_md_f", "FREQ", T::fx, KS::Med));
    eqKnobs.push_back (makeKnob ("eq_md_g", "GAIN", T::fx, KS::Med, true));
    eqKnobs.push_back (makeKnob ("eq_md_q", "Q",    T::fx, KS::Med));
    eqKnobs.push_back (makeKnob ("eq_hi_f", "FREQ", T::fx, KS::Med));
    eqKnobs.push_back (makeKnob ("eq_hi_g", "GAIN", T::fx, KS::Med, true));

    reverbOnToggle = makeToggle ("rv_on", "REVERB", T::fx);
    reverbKnobs.push_back (makeKnob ("rv_room", "ROOM", T::fx, KS::Med));
    reverbKnobs.push_back (makeKnob ("rv_damp", "DAMP", T::fx, KS::Med));
    reverbKnobs.push_back (makeKnob ("rv_wet",  "MIX",  T::fx, KS::Med));

    transientOnToggle = makeToggle ("tr_on", "TRANSIENT", T::master);
    transientKnobs.push_back (makeKnob ("tr_atk", "ATTACK",  T::master, KS::Med, true));
    transientKnobs.push_back (makeKnob ("tr_sus", "SUSTAIN", T::master, KS::Med, true));

    masterKnobs.push_back (makeKnob ("master_drv",  "DRIVE",  T::master, KS::Big));
    masterKnobs.push_back (makeKnob ("master_gain", "OUTPUT", T::master, KS::Big));
    masterShaperCombo = makeChoice ("master_shp", "SHAPE", T::master);
    limiterToggle     = makeToggle ("master_lim", "LIMIT",  T::master);
    oversampleToggle  = makeToggle ("master_ovs", "OVS 2X", T::master);
}

void KickforgeAudioProcessorEditor::buildBackgroundTexture()
{
    // Gebürsteter Stahl: horizontale Streifen + feines Rauschen, sehr dezent
    bgTexture = juce::Image (juce::Image::ARGB, 256, 256, true);
    juce::Random rng (0x4b464f52);   // fester Seed → immer gleiche Textur
    juce::Image::BitmapData bd (bgTexture, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < 256; ++y)
    {
        const float row = rng.nextFloat();
        for (int x = 0; x < 256; ++x)
        {
            const float v = 0.55f * row + 0.45f * rng.nextFloat();
            bd.setPixelColour (x, y, juce::Colours::white.withAlpha (v * 0.035f));
        }
    }
}

// =========================================================================
// Timer: Waveform neu rendern + Output-Meter
// =========================================================================

void KickforgeAudioProcessorEditor::timerCallback()
{
    ++tickCount;
    if (proc.pollParamsChanged())
        waveformDirty = true;

    if (waveformDirty && tickCount % 3 == 0)   // max. 10× pro Sekunde
    {
        waveformDirty = false;
        renderWaveform();
    }

    const float pk = proc.takeOutputPeak();
    meterLevel = juce::jmax (pk, meterLevel * 0.86f);
    if (pk >= meterHold) { meterHold = pk; meterHoldTicks = 36; }
    else if (--meterHoldTicks <= 0) meterHold *= 0.92f;

    repaint (meterBounds.expanded (4));
}

void KickforgeAudioProcessorEditor::renderWaveform()
{
    // Läuft schon ein Render? Dann beim nächsten Tick nochmal (mit den dann aktuellen Werten).
    if (renderInFlight.exchange (true))
    {
        waveformDirty = true;
        return;
    }

    juce::Component::SafePointer<KickforgeAudioProcessorEditor> safe (this);
    renderPool.addJob ([this, safe]
    {
        constexpr double sr = 44100.0;
        const int n = (int) (sr * 1.5);
        auto parts = std::make_shared<juce::AudioBuffer<float>> (3, n);
        auto sum   = std::make_shared<juce::AudioBuffer<float>> (1, n);
        parts->clear();
        sum->clear();
        proc.renderOfflineParts (*parts, sr);
        proc.renderOfflineKick  (*sum, sr);

        juce::MessageManager::callAsync ([safe, parts, sum]
        {
            if (auto* ed = safe.getComponent())
            {
                ed->waveform.setData (*parts, *sum, sr);
                ed->renderInFlight = false;
            }
        });
    });
}

// =========================================================================
// Presets / Export / Sample
// =========================================================================

void KickforgeAudioProcessorEditor::refreshPresetCombo()
{
    presetCombo.clear (juce::dontSendNotification);
    int id = 1;
    for (auto& n : proc.getPresetManager().getAllPresetNames())
        presetCombo.addItem (n, id++);
}

void KickforgeAudioProcessorEditor::loadSelectedPreset (int delta)
{
    const int count = presetCombo.getNumItems();
    if (count == 0) return;
    int idx = juce::jmax (0, presetCombo.getSelectedId() - 1);
    idx = (idx + delta + count) % count;
    presetCombo.setSelectedId (idx + 1, juce::sendNotification);
}

void KickforgeAudioProcessorEditor::onSavePresetClicked()
{
    auto dir = kf::PresetManager::getUserDir();
    if (! dir.exists()) dir.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> (
        juce::String::fromUTF8 ("Save Preset As\xe2\x80\xa6"), dir.getChildFile ("MyKick.kfp"), "*.kfp");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File()) return;
            if (! file.hasFileExtension ("kfp"))
                file = file.withFileExtension ("kfp");

            if (auto xml = proc.apvts.copyState().createXml())
            {
                xml->writeTo (file);
                refreshPresetCombo();
            }
        });
}

void KickforgeAudioProcessorEditor::onExportClicked()
{
    auto defaultDir = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
    fileChooser = std::make_unique<juce::FileChooser> (
        "Export Kick as WAV", defaultDir.getChildFile ("kickforge.wav"), "*.wav");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File()) return;
            if (! file.hasFileExtension ("wav"))
                file = file.withFileExtension ("wav");

            constexpr double sr = 44100.0;
            constexpr int numSamples = 44100 * 2;

            juce::AudioBuffer<float> buf (2, numSamples);
            buf.clear();
            proc.renderOfflineKick (buf, sr);

            juce::WavAudioFormat wav;
            file.deleteFile();
            std::unique_ptr<juce::FileOutputStream> fos (file.createOutputStream());
            if (fos == nullptr) return;
            std::unique_ptr<juce::AudioFormatWriter> writer (
                wav.createWriterFor (fos.get(), sr, 2u, 24, {}, 0));
            if (writer != nullptr)
            {
                fos.release();   // Writer besitzt jetzt den Stream
                writer->writeFromAudioSampleBuffer (buf, 0, numSamples);
            }
        });
}

void KickforgeAudioProcessorEditor::onLoadSampleClicked()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load Click Sample",
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File() && proc.loadClickSample (file))
                updateSampleLabel();
        });
}

void KickforgeAudioProcessorEditor::onClearSampleClicked()
{
    proc.clearClickSample();
    updateSampleLabel();
}

void KickforgeAudioProcessorEditor::updateSampleLabel()
{
    const auto path = proc.getClickSamplePath();
    sampleNameLabel.setText (path.isEmpty() ? juce::String ("no sample")
                                            : juce::File (path).getFileNameWithoutExtension(),
                             juce::dontSendNotification);
    clearSampleBtn.setEnabled (path.isNotEmpty());
}

// =========================================================================
// Zeichnen
// =========================================================================

void KickforgeAudioProcessorEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> b,
                                               const juce::String& index, const juce::String& title,
                                               juce::Colour accent) const
{
    using namespace juce;
    auto r = b.toFloat();
    constexpr float cut = 12.0f;
    const auto shape = T::chamferRect (r.reduced (0.5f), cut);

    g.setGradientFill (ColourGradient (T::panel, 0.0f, r.getY(), T::panelLo, 0.0f, r.getBottom(), false));
    g.fillPath (shape);

    // Lichtkante oben + Rahmen
    g.setColour (Colours::white.withAlpha (0.045f));
    g.drawLine (r.getX() + cut + 1.0f, r.getY() + 1.5f, r.getRight() - 1.0f, r.getY() + 1.5f, 1.0f);
    g.setColour (T::edge);
    g.strokePath (shape, PathStrokeType (1.0f));

    // Akzentbalken an der Abschrägung
    Path bar;
    bar.startNewSubPath (r.getX() + 1.0f, r.getY() + cut + 0.5f);
    bar.lineTo (r.getX() + cut + 0.5f, r.getY() + 1.0f);
    bar.lineTo (r.getX() + cut + 52.0f, r.getY() + 1.0f);
    g.setColour (accent);
    g.strokePath (bar, PathStrokeType (2.5f, PathStrokeType::mitered, PathStrokeType::square));

    // Titel: "01  CLICK"
    auto titleArea = b.withHeight (36).withTrimmedLeft (18);
    if (index.isNotEmpty())
    {
        g.setColour (T::faint);
        g.setFont (lnf.fonts().get (W::SemiBold, 12.0f, 0.1f));
        g.drawText (index, titleArea.removeFromLeft (24), Justification::centredLeft, false);
    }
    g.setColour (accent);
    g.setFont (lnf.fonts().get (W::Bold, 15.5f, 0.18f));
    g.drawText (title, titleArea, Justification::centredLeft, false);
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

    // Emblem
    const Rectangle<float> emb (16.0f, 3.0f, 58.0f, 58.0f);
    if (emblem != nullptr)
        emblem->drawWithin (g, emb, RectanglePlacement::centred, 1.0f);

    // Wordmark: KICK (Stahlweiß) + FORGE (Warnorange)
    const auto wordFont = lnf.fonts().get (W::Bold, 33.0f, 0.06f);
    const float x0    = 78.0f;
    const float kickW = GlyphArrangement::getStringWidth (wordFont, "KICK");
    const float forgW = GlyphArrangement::getStringWidth (wordFont, "FORGE");
    g.setFont (wordFont);
    g.setColour (T::text);
    g.drawText ("KICK",  Rectangle<float> (x0, 6.0f, kickW + 4.0f, 34.0f), Justification::centredLeft, false);
    g.setColour (T::brand);
    g.drawText ("FORGE", Rectangle<float> (x0 + kickW, 6.0f, forgW + 4.0f, 34.0f), Justification::centredLeft, false);

    // Tagline exakt so breit wie die Wordmark (Blocksatz-Lockup)
    const String tag = String::fromUTF8 ("HARDTECHNO \xc2\xb7 INDUSTRIAL \xc2\xb7 SCHRANZ");
    const float wordW = kickW + forgW - 33.0f * 0.06f;   // letzte Laufweite abziehen
    auto tagFont = lnf.fonts().get (W::SemiBold, 9.5f, 0.24f);
    const float tagW = GlyphArrangement::getStringWidth (tagFont, tag);
    if (tagW > 1.0f)
        tagFont = lnf.fonts().get (W::SemiBold, jlimit (7.5f, 11.0f, 9.5f * wordW / tagW), 0.24f);
    g.setFont (tagFont);
    g.setColour (T::dim);
    g.drawText (tag, Rectangle<float> (x0 + 1.0f, 40.0f, wordW + 20.0f, 13.0f), Justification::centredLeft, false);

    // Version
    g.setColour (T::faint);
    g.setFont (lnf.fonts().get (W::SemiBold, 10.0f, 0.1f));
    g.drawText ("v0.6", Rectangle<float> (x0 + wordW + 8.0f, 12.0f, 40.0f, 12.0f), Justification::centredLeft, false);

    // Preset-Display (eingelassen)
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

    // dB → 0..1 (-48 .. +3 dB)
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
        const float pos = (float) (i + 1) / (float) nSeg;          // 0 unten … 1 oben
        const float y   = inner.getBottom() - (float) (i + 1) * (segH + segGap) + segGap;
        const float db  = pos * 51.0f - 48.0f;
        const auto  col = db > -1.0f ? Colour (0xffff3b1f) : db > -12.0f ? T::brand : T::tail;
        g.setColour (pos <= lit ? col : Colour (0xff1c1a19));
        g.fillRect (inner.getX(), y, inner.getWidth(), segH);
    }

    // Peak-Hold-Linie
    if (meterHold > 0.0005f)
    {
        const float y = inner.getBottom() - norm (meterHold) * inner.getHeight();
        g.setColour (T::text);
        g.fillRect (inner.getX(), y - 1.0f, inner.getWidth(), 2.0f);
    }

    // Skala
    g.setFont (lnf.fonts().get (W::SemiBold, 9.5f, 0.0f));
    for (float db : { 0.0f, -12.0f, -24.0f, -48.0f })
    {
        const float y = inner.getBottom() - (db + 48.0f) / 51.0f * inner.getHeight();
        g.setColour (T::faint);
        g.drawText (db == 0.0f ? String ("0") : String ((int) db),
                    Rectangle<float> (bar.getX() - 26.0f, y - 6.0f, 22.0f, 12.0f),
                    Justification::centredRight, false);
    }
}

void KickforgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;

    // Hintergrund: Verlauf + gebürstete Textur + Vignette oben
    g.setGradientFill (ColourGradient (T::bgTop, 0.0f, 0.0f, T::bg, 0.0f, (float) getHeight(), false));
    g.fillAll();
    g.setTiledImageFill (bgTexture, 0, 0, 1.0f);
    g.fillAll();
    g.setGradientFill (ColourGradient (T::brand.withAlpha (0.06f), (float) getWidth() * 0.5f, -80.0f,
                                       Colours::transparentBlack, (float) getWidth() * 0.5f, 220.0f, true));
    g.fillRect (getLocalBounds().removeFromTop (240));

    // Rack-Schrauben in den Ecken
    const float W_ = (float) getWidth(), H_ = (float) getHeight();
    drawScrew (g, { 7.0f, 7.0f }, 0.6f);
    drawScrew (g, { W_ - 7.0f, 7.0f }, 2.1f);
    drawScrew (g, { 7.0f, H_ - 7.0f }, 1.2f);
    drawScrew (g, { W_ - 7.0f, H_ - 7.0f }, 0.2f);

    drawHeader (g);

    // Layer-Panels
    for (auto* p : { &clickPanel, &bodyPanel, &tailPanel })
    {
        drawPanel (g, p->bounds, p->index, p->title, p->accent);

        // feste Oszillator-Anzeige (Tail)
        if (p->modeCombo == nullptr && p->fixedOscText.isNotEmpty())
        {
            auto row = p->oscRow;
            g.setColour (T::dim);
            g.setFont (lnf.fonts().get (W::Bold, 10.5f, 0.14f));
            g.drawText ("OSC", row.removeFromLeft (40), Justification::centredLeft, false);
            g.setColour (T::text.withAlpha (0.55f));
            g.setFont (lnf.fonts().get (W::SemiBold, 13.0f, 0.06f));
            g.drawText (p->fixedOscText, row, Justification::centredLeft, false);
        }

        // Trennlinie zwischen Knob-Reihen und Shape/Filter
        g.setColour (T::edge);
        g.drawHorizontalLine (p->dividerY, (float) p->bounds.getX() + 12.0f, (float) p->bounds.getRight() - 12.0f);
    }

    // EQ / FX / Master
    drawPanel (g, eqBounds,     "04", "MASTER EQ", T::fx);
    drawPanel (g, fxBounds,     "05", "FX",        T::fx);
    drawPanel (g, masterBounds, "06", "MASTER",    T::master);

    // EQ-Gruppenlabels mit Klammerlinie
    const char* groups[3] = { "LOW SHELF", "MID PEAK", "HIGH SHELF" };
    for (int i = 0; i < 3; ++i)
    {
        auto r = eqGroupRects[i];
        g.setColour (T::faint);
        g.drawHorizontalLine (r.getCentreY(), (float) r.getX() + 4.0f, (float) r.getRight() - 4.0f);
        g.setFont (lnf.fonts().get (W::Bold, 10.0f, 0.16f));
        const int tw = (int) GlyphArrangement::getStringWidth (g.getCurrentFont(), groups[i]) + 12;
        auto label = r.withSizeKeepingCentre (tw, r.getHeight());
        g.setColour (T::panel);
        g.fillRect (label);
        g.setColour (T::dim);
        g.drawText (groups[i], label, Justification::centred, false);
    }

    // Warnstreifen im Master-Titel
    {
        auto stripe = masterBounds.withHeight (36).withTrimmedLeft (130).withTrimmedRight (18)
                                  .withSizeKeepingCentre (56, 8).withX (masterBounds.getRight() - 18 - 56);
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

// =========================================================================
// Layout
// =========================================================================

void KickforgeAudioProcessorEditor::placeKnob (Knob& k, juce::Rectangle<int> cell)
{
    // Feste Knob-Durchmesser je Größe → gleiche Hierarchie in allen Panels
    const int knobD  = k.size == KnobSize::Big ? 78 : k.size == KnobSize::Med ? 54 : 40;
    const int labelH = k.size == KnobSize::Big ? 15 : 14;
    const int textH  = k.slider.getTextBoxHeight();

    const int sliderH = juce::jmin (cell.getHeight() - labelH, knobD + textH);
    auto block = cell.withSizeKeepingCentre (cell.getWidth(), labelH + sliderH);   // vertikal zentriert

    k.label.setBounds (block.removeFromTop (labelH));
    const int sliderW = juce::jmin (block.getWidth(), knobD + 34);
    k.slider.setBounds (block.withSizeKeepingCentre (sliderW, sliderH));
}

void KickforgeAudioProcessorEditor::layoutPanel (LayerPanel& p)
{
    auto inner = p.bounds.withTrimmedTop (38).reduced (12, 8);

    if (p.onToggle)
        p.onToggle->button.setBounds (p.bounds.getRight() - 14 - 54, p.bounds.getY() + 8, 54, 20);

    // OSC-Zeile
    auto row = inner.removeFromTop (26);
    p.oscRow = row;
    if (p.modeCombo)
    {
        const bool isClick = (&p == &clickPanel);
        p.modeCombo->label.setBounds (row.removeFromLeft (isClick ? 34 : 40));
        p.modeCombo->combo.setBounds (row.removeFromLeft (isClick ? 100 : 122));
    }
    if (&p == &clickPanel)
    {
        row.removeFromLeft (8);
        loadSampleBtn.setBounds (row.removeFromLeft (56));
        row.removeFromLeft (4);
        clearSampleBtn.setBounds (row.removeFromLeft (26));
        row.removeFromLeft (6);
        sampleNameLabel.setBounds (row.withSizeKeepingCentre (row.getWidth(), 16));   // 1 Zeile → kürzen statt umbrechen
    }
    inner.removeFromTop (6);

    // Große Knobs
    const int nBig = juce::jmin (p.bigCount, (int) p.knobs.size());
    auto bigRow = inner.removeFromTop (108);
    for (int i = 0; i < nBig; ++i)
    {
        const int w = bigRow.getWidth() / (nBig - i);
        placeKnob (*p.knobs[(size_t) i], bigRow.removeFromLeft (w));
    }
    inner.removeFromTop (6);   // Luft zwischen großen Werten und mittleren Labels

    // Mittlere Knobs
    const int nMed = (int) p.knobs.size() - nBig;
    auto medRow = inner.removeFromTop (82);
    for (int i = 0; i < nMed; ++i)
    {
        const int w = medRow.getWidth() / (nMed - i);
        placeKnob (*p.knobs[(size_t) (nBig + i)], medRow.removeFromLeft (w));
    }

    inner.removeFromTop (4);
    p.dividerY = inner.getY();
    inner.removeFromTop (8);

    // SHAPE + FILTER (Combos nebeneinander)
    auto comboRow = inner.removeFromTop (24);
    const int half = (comboRow.getWidth() - 10) / 2;
    auto left  = comboRow.removeFromLeft (half);
    comboRow.removeFromLeft (10);
    auto right = comboRow;
    p.shaperCombo->label.setBounds (left.removeFromLeft (48));
    p.shaperCombo->combo.setBounds (left);
    p.filterModeCombo->label.setBounds (right.removeFromLeft (52));
    p.filterModeCombo->combo.setBounds (right);
    inner.removeFromTop (6);

    // Kleine Knobs: Cutoff, Reso, Delay
    Knob* smalls[3] = { p.filterCutKnob.get(), p.filterResKnob.get(), p.delayKnob.get() };
    auto smallRow = inner;
    for (int i = 0; i < 3; ++i)
    {
        const int w = smallRow.getWidth() / (3 - i);
        placeKnob (*smalls[i], smallRow.removeFromLeft (w));
    }
}

void KickforgeAudioProcessorEditor::resized()
{
    using namespace layout;
    if (masterKnobs.size() < 2 || eqKnobs.size() < 7 || reverbKnobs.size() < 3 || transientKnobs.size() < 2)
        return;

    const int W_ = getWidth();
    const int availW = W_ - pad * 2;

    // ---- Header ----
    presetPlate = { 380, 12, 486, 38 };
    {
        // [PRESET] [◀] [ Combo ] [▶]  [SAVE]
        auto r = presetPlate.reduced (6, 6).withTrimmedLeft (64);
        savePresetBtn.setBounds (r.removeFromRight (66));
        r.removeFromRight (8);
        prevPresetBtn.setBounds (r.removeFromLeft (28));
        r.removeFromLeft (4);
        nextPresetBtn.setBounds (r.removeFromRight (28));
        r.removeFromRight (4);
        presetCombo.setBounds (r);
    }
    triggerBtn.setBounds (W_ - pad - 132, 12, 132, 38);
    exportBtn .setBounds (W_ - pad - 132 - gap - 124, 12, 124, 38);

    // ---- Visualizer ----
    waveform.setBounds (pad, vizY, availW, vizH);

    // ---- Layer-Panels ----
    const int lw = availW - 2 * gap;
    const int clickW = (int) (lw * 0.29f);
    const int bodyW  = (int) (lw * 0.40f);
    const int tailW  = lw - clickW - bodyW;
    int x = pad;
    clickPanel.bounds = { x, layersY, clickW, layersH }; x += clickW + gap;
    bodyPanel .bounds = { x, layersY, bodyW,  layersH }; x += bodyW  + gap;
    tailPanel .bounds = { x, layersY, tailW,  layersH };
    layoutPanel (clickPanel);
    layoutPanel (bodyPanel);
    layoutPanel (tailPanel);

    // ---- Untere Reihe ----
    const int bottomH = getHeight() - bottomY - pad;
    const int eqW  = (int) (lw * 0.42f);
    const int fxW  = (int) (lw * 0.32f);
    const int masW = lw - eqW - fxW;
    eqBounds     = { pad,                         bottomY, eqW,  bottomH };
    fxBounds     = { pad + eqW + gap,             bottomY, fxW,  bottomH };
    masterBounds = { pad + eqW + fxW + 2 * gap,   bottomY, masW, bottomH };

    // EQ: 3 Gruppen (2 / 3 / 2 Knobs)
    {
        eqOnToggle->button.setBounds (eqBounds.getRight() - 14 - 54, eqBounds.getY() + 8, 54, 20);
        auto in = eqBounds.withTrimmedTop (38).reduced (12, 8);
        auto groupRow = in.removeFromTop (14);
        in.removeFromTop (4);
        const int cellW = in.getWidth() / 7;
        const int counts[3] = { 2, 3, 2 };
        int k = 0;
        for (int gI = 0; gI < 3; ++gI)
        {
            const int gw = (gI == 2) ? in.getWidth() : cellW * counts[gI];
            eqGroupRects[gI] = groupRow.removeFromLeft (gw);
            auto gArea = in.removeFromLeft (gw);
            for (int j = 0; j < counts[gI]; ++j, ++k)
            {
                const int w = gArea.getWidth() / (counts[gI] - j);
                placeKnob (*eqKnobs[(size_t) k], gArea.removeFromLeft (w));
            }
        }
    }

    // FX: Reverb (3) | Transient (2)
    {
        auto in = fxBounds.withTrimmedTop (38).reduced (12, 8);
        const int cellW = (in.getWidth() - 14) / 5;
        auto rv = in.removeFromLeft (cellW * 3);
        in.removeFromLeft (14);
        auto tr = in;
        reverbOnToggle   ->button.setBounds (rv.removeFromTop (20).withWidth (110));
        transientOnToggle->button.setBounds (tr.removeFromTop (20).withWidth (120));
        rv.removeFromTop (4);
        tr.removeFromTop (4);
        for (int i = 0; i < 3; ++i) { const int w = rv.getWidth() / (3 - i); placeKnob (*reverbKnobs[(size_t) i],    rv.removeFromLeft (w)); }
        for (int i = 0; i < 2; ++i) { const int w = tr.getWidth() / (2 - i); placeKnob (*transientKnobs[(size_t) i], tr.removeFromLeft (w)); }
    }

    // Master: 2 große Knobs + Meter, darunter Shape + Toggles
    {
        auto in = masterBounds.withTrimmedTop (38).reduced (12, 8);
        auto bottom = in.removeFromBottom (24);
        in.removeFromBottom (6);
        meterBounds = in.removeFromRight (14).withTrimmedTop (2);
        in.removeFromRight (32);   // Platz für die dB-Skala
        for (int i = 0; i < 2; ++i) { const int w = in.getWidth() / (2 - i); placeKnob (*masterKnobs[(size_t) i], in.removeFromLeft (w)); }

        masterShaperCombo->label.setBounds (bottom.removeFromLeft (44));
        masterShaperCombo->combo.setBounds (bottom.removeFromLeft (94));
        bottom.removeFromLeft (8);
        oversampleToggle->button.setBounds (bottom.removeFromRight (80));
        limiterToggle   ->button.setBounds (bottom);
    }
}
