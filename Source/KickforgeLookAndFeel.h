#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

/*
    KickforgeLookAndFeel — eigenes Industrial-Design für alle Controls.

    Slider-Properties (über slider.getProperties()):
      "bipolar" = true   → Wertbogen startet in der Mitte (EQ-Gain, Transient)
      "size"    = "big" | "med" | "small"  → Detailgrad (Ticks nur bei big)
    TextButton-Property:
      "primary" = true   → orange gefüllter Haupt-Button (KICK)
*/
class KickforgeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KickforgeLookAndFeel();

    // ---- Slider ----
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

    // ---- Toggle (LED) ----
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool highlighted, bool down) override;

    // ---- Buttons ----
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& bgColour,
                               bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    // ---- ComboBox + Popup ----
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    // ---- Labels / Fallback ----
    juce::Font getLabelFont (juce::Label&) override;
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    const kf::theme::FontSet& fonts() const { return *fontSet; }

private:
    juce::SharedResourcePointer<kf::theme::FontSet> fontSet;
};
