#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "WaveformDisplay.h"
#include "KickforgeLookAndFeel.h"

class KickforgeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit KickforgeAudioProcessorEditor (KickforgeAudioProcessor&);
    ~KickforgeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    using SAtt  = APVTS::SliderAttachment;
    using BAtt  = APVTS::ButtonAttachment;
    using CAtt  = APVTS::ComboBoxAttachment;

    enum class KnobSize { Big, Med, Small };

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        KnobSize     size = KnobSize::Med;
        std::unique_ptr<SAtt> att;
    };
    struct ToggleCtrl
    {
        juce::ToggleButton button;
        std::unique_ptr<BAtt> att;
    };
    struct ChoiceCtrl
    {
        juce::ComboBox combo;
        juce::Label    label;
        std::unique_ptr<CAtt> att;
    };

    struct LayerPanel
    {
        juce::String  index, title;
        juce::Colour  accent;
        std::unique_ptr<ToggleCtrl>        onToggle;   // null = Layer immer an
        std::unique_ptr<ChoiceCtrl>        modeCombo;  // null = fester Oszillator
        std::vector<std::unique_ptr<Knob>> knobs;      // erst die großen, dann die mittleren
        int                                bigCount = 3;
        std::unique_ptr<ChoiceCtrl>        shaperCombo, filterModeCombo;
        std::unique_ptr<Knob>              filterCutKnob, filterResKnob, delayKnob;
        juce::String                       fixedOscText;   // für Layer ohne Mode-Combo
        juce::Rectangle<int>               bounds, oscRow;
        int                                dividerY = 0;
    };

    // ---- LookAndFeel ZUERST deklarieren → wird zuletzt zerstört ----
    KickforgeLookAndFeel lnf;
    KickforgeAudioProcessor& proc;

    LayerPanel clickPanel, bodyPanel, tailPanel;

    // EQ / FX / Master
    std::unique_ptr<ToggleCtrl>        eqOnToggle, reverbOnToggle, transientOnToggle,
                                       limiterToggle, oversampleToggle;
    std::vector<std::unique_ptr<Knob>> eqKnobs, reverbKnobs, transientKnobs, masterKnobs;
    std::unique_ptr<ChoiceCtrl>        masterShaperCombo;
    juce::Rectangle<int>               eqBounds, fxBounds, masterBounds, meterBounds;
    juce::Rectangle<int>               eqGroupRects[3];

    // Header
    std::unique_ptr<juce::Drawable> emblem;
    juce::Rectangle<int> presetPlate;
    juce::ComboBox   presetCombo;
    juce::TextButton savePresetBtn { "SAVE" };
    juce::TextButton prevPresetBtn { juce::String::fromUTF8 ("\xe2\x97\x80") };   // ◀
    juce::TextButton nextPresetBtn { juce::String::fromUTF8 ("\xe2\x96\xb6") };   // ▶
    juce::TextButton exportBtn     { "EXPORT WAV" };
    juce::TextButton triggerBtn    { "KICK" };

    // Click-Sample
    juce::TextButton loadSampleBtn  { "LOAD" };
    juce::TextButton clearSampleBtn { juce::String::fromUTF8 ("\xe2\x9c\x95") };  // ✕
    juce::Label      sampleNameLabel;

    WaveformDisplay waveform;

    // Output-Meter
    float meterLevel = 0.0f, meterHold = 0.0f;
    int   meterHoldTicks = 0;

    // Hintergrund-Textur (gebürsteter Stahl, prozedural)
    juce::Image bgTexture;

    bool waveformDirty = true;
    int  tickCount = 0;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Visualizer-Rendering im Hintergrund (GUI-Thread wartet nie).
    // renderPool MUSS das letzte Member sein: wird zuerst zerstört und
    // wartet dabei auf laufende Jobs, bevor irgendetwas anderes abgebaut wird.
    std::atomic<bool> renderInFlight { false };
    juce::ThreadPool  renderPool { juce::ThreadPoolOptions{}.withNumberOfThreads (1)
                                                            .withThreadName ("Kickforge Visualizer") };

    // --- Aufbau ---
    std::unique_ptr<Knob>       makeKnob   (const juce::String& id, const juce::String& text,
                                            juce::Colour accent, KnobSize size, bool bipolar = false);
    std::unique_ptr<ToggleCtrl> makeToggle (const juce::String& id, const juce::String& text, juce::Colour accent);
    std::unique_ptr<ChoiceCtrl> makeChoice (const juce::String& id, const juce::String& text, juce::Colour accent);
    void buildLayerPanels();
    void buildBottomRow();
    void buildHeader();
    void buildBackgroundTexture();

    // --- Aktionen ---
    void refreshPresetCombo();
    void loadSelectedPreset (int delta);
    void onSavePresetClicked();
    void onExportClicked();
    void onLoadSampleClicked();
    void onClearSampleClicked();
    void updateSampleLabel();
    void renderWaveform();

    // --- Zeichnen / Layout ---
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& index,
                    const juce::String& title, juce::Colour accent) const;
    void drawScrew (juce::Graphics&, juce::Point<float> c, float angle) const;
    void drawHeader (juce::Graphics&) const;
    void drawMeter (juce::Graphics&) const;
    void layoutPanel (LayerPanel&);
    void placeKnob (Knob&, juce::Rectangle<int> cell);

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickforgeAudioProcessorEditor)
};
