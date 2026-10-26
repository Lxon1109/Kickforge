#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "KickVisualizer.h"
#include "KickforgeLookAndFeel.h"

class KickforgeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::FileDragAndDropTarget,
                                      private juce::Timer
{
public:
    explicit KickforgeAudioProcessorEditor (KickforgeAudioProcessor&);
    ~KickforgeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    // WAV/AIFF auf eine Schicht ziehen → Sample-Slot dieser Schicht
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int x, int y) override { dragPos = { x, y }; repaint(); }
    void fileDragMove  (const juce::StringArray&, int x, int y) override { dragPos = { x, y }; repaint(); }
    void fileDragExit  (const juce::StringArray&) override              { dragPos = { -1, -1 }; repaint(); }
    void filesDropped  (const juce::StringArray& files, int x, int y) override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    using SAtt  = APVTS::SliderAttachment;
    using BAtt  = APVTS::ButtonAttachment;

    enum class KnobSize { Big, Med, Small };

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        KnobSize     size = KnobSize::Med;
        std::unique_ptr<SAtt> att;
        void setVisible (bool v) { slider.setVisible (v); label.setVisible (v); }
        void setDimmed (bool d)  { slider.setAlpha (d ? 0.38f : 1.0f); label.setAlpha (d ? 0.38f : 1.0f); }
    };
    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<BAtt> att;
    };

    struct Panel
    {
        juce::String index, title;
        juce::Colour accent;
        int layer = -1;                                   // 0..3 = Schicht mit Sample-Slot
        std::vector<std::unique_ptr<Knob>> big, med, small;
        juce::Rectangle<int> bounds;
        int dividerY = 0;

        // Sample-Slot
        std::unique_ptr<Toggle> sampleToggle;
        std::unique_ptr<juce::TextButton> loadBtn, clearBtn;
        std::unique_ptr<juce::Label> sampleName;
        std::unique_ptr<Knob> sGain, sPitch;
    };

    // ---- LookAndFeel ZUERST → wird zuletzt zerstört ----
    KickforgeLookAndFeel lnf;
    KickforgeAudioProcessor& proc;

    Panel clickP, punchP, rumbleP, gritP, kickP, masterP;
    std::unique_ptr<Toggle> syncToggle, keytrackToggle;
    juce::Label tempoInfo;

    // Header
    std::unique_ptr<juce::Drawable> emblem;
    juce::Rectangle<int> presetPlate;
    juce::ComboBox   presetCombo;
    juce::TextButton savePresetBtn { "SAVE" };
    juce::TextButton prevPresetBtn { juce::String::fromUTF8 ("\xe2\x97\x80") };   // ◀
    juce::TextButton nextPresetBtn { juce::String::fromUTF8 ("\xe2\x96\xb6") };   // ▶
    juce::TextButton exportBtn     { "EXPORT WAV" };
    juce::TextButton triggerBtn    { "KICK" };

    KickVisualizer visualizer;
    juce::Rectangle<int> meterBounds;
    float meterLevel = 0.0f, meterHold = 0.0f;
    int   meterHoldTicks = 0;

    juce::Image bgTexture;
    juce::Point<int> dragPos { -1, -1 };
    juce::uint32 lastChange = 0;
    bool  vizDirty = true;
    int   tickCount = 0;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Visualizer-Rendering im Hintergrund. renderPool MUSS das letzte Member sein.
    std::atomic<bool> renderInFlight { false };
    juce::ThreadPool  renderPool { juce::ThreadPoolOptions{}.withNumberOfThreads (1)
                                                            .withThreadName ("Kickforge Visualizer") };

    // --- Aufbau ---
    std::unique_ptr<Knob>   makeKnob   (const juce::String& id, const juce::String& text, juce::Colour accent,
                                        KnobSize size, bool bipolar = false);
    std::unique_ptr<Toggle> makeToggle (const juce::String& id, const juce::String& text, juce::Colour accent);
    void buildPanels();
    void buildSampleSlot (Panel&);
    void buildHeader();
    void buildBackgroundTexture();
    Panel* layerPanel (int layer);
    Panel* panelAt (juce::Point<int>);

    // --- Aktionen ---
    void refreshPresetCombo();
    void stepPreset (int delta);
    void onSavePresetClicked();
    void onExportClicked();
    void onLoadSampleClicked (int layer);
    void updateSampleSlots();
    void updateTempoInfo();
    void renderVisualizer();

    // --- Zeichnen / Layout ---
    void drawPanel (juce::Graphics&, const Panel&) const;
    void drawScrew (juce::Graphics&, juce::Point<float> c, float angle) const;
    void drawHeader (juce::Graphics&) const;
    void drawMeter (juce::Graphics&) const;
    void placeKnob (Knob&, juce::Rectangle<int> cell);
    void layoutRow (std::vector<std::unique_ptr<Knob>>&, juce::Rectangle<int> row, int slots);
    void layoutLayerPanel (Panel&);
    void layoutSampleSlot (Panel&, juce::Rectangle<int> area, bool stacked);

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickforgeAudioProcessorEditor)
};
