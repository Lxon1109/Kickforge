#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

/*
    Kickforge Theme — dunkel-industrial: Stahl, Warnorange, kantig.
    Zentrale Farben + eingebettete Schrift (Chakra Petch, SIL OFL).
*/
namespace kf::theme
{
    // ---- Grundflächen ----
    inline const juce::Colour bg        { 0xff0c0b0a };
    inline const juce::Colour bgTop     { 0xff16120f };
    inline const juce::Colour panel     { 0xff181615 };
    inline const juce::Colour panelLo   { 0xff121010 };
    inline const juce::Colour edge      { 0xff2b2725 };
    inline const juce::Colour edgeHi    { 0xff3d3835 };
    inline const juce::Colour recess    { 0xff0e0d0c };   // eingelassene Flächen (Display, Felder)

    // ---- Text ----
    inline const juce::Colour text      { 0xffece7e2 };
    inline const juce::Colour dim       { 0xff8f857e };
    inline const juce::Colour faint     { 0xff5a534e };

    // ---- Marke + Layer-Akzente ----
    inline const juce::Colour brand     { 0xffff6a1a };   // Warnorange
    inline const juce::Colour brandHi   { 0xffffa040 };
    inline const juce::Colour brandLo   { 0xffd2450e };
    inline const juce::Colour click     { 0xffffa040 };   // Transient: heißes Gelb-Orange
    inline const juce::Colour body      { 0xffff4a1f };   // Body: Rot-Orange
    inline const juce::Colour tail      { 0xff6f9bb8 };   // Tail: Stahlblau
    inline const juce::Colour master    { 0xfff2c040 };   // Master: Warngelb
    inline const juce::Colour fx        { 0xff63c79a };   // EQ/FX: Oxid-Grün

    // ---- Knob-Körper ----
    inline const juce::Colour knobTop   { 0xff3b3633 };
    inline const juce::Colour knobBot   { 0xff1b1918 };
    inline const juce::Colour track     { 0xff272321 };

    enum class Weight { Regular, SemiBold, Bold };

    // Geteilte Schrift-Ressource: wird erzeugt, solange mindestens ein
    // Editor offen ist, und danach sauber freigegeben (kein statischer Typeface).
    struct FontSet
    {
        FontSet()
        {
            regular  = juce::Typeface::createSystemTypefaceFor (BinaryData::ChakraPetchRegular_ttf,
                                                               BinaryData::ChakraPetchRegular_ttfSize);
            semibold = juce::Typeface::createSystemTypefaceFor (BinaryData::ChakraPetchSemiBold_ttf,
                                                               BinaryData::ChakraPetchSemiBold_ttfSize);
            bold     = juce::Typeface::createSystemTypefaceFor (BinaryData::ChakraPetchBold_ttf,
                                                               BinaryData::ChakraPetchBold_ttfSize);
        }

        juce::Font get (Weight w, float height, float tracking = 0.0f) const
        {
            const auto& tf = (w == Weight::Bold) ? bold : (w == Weight::SemiBold ? semibold : regular);
            return juce::Font (juce::FontOptions (tf).withHeight (height).withKerningFactor (tracking));
        }

        juce::Typeface::Ptr regular, semibold, bold;
    };

    // Kantiges Rechteck mit abgeschrägten Ecken (oben-links + unten-rechts)
    inline juce::Path chamferRect (juce::Rectangle<float> r, float cut)
    {
        juce::Path p;
        p.startNewSubPath (r.getX() + cut, r.getY());
        p.lineTo (r.getRight(), r.getY());
        p.lineTo (r.getRight(), r.getBottom() - cut);
        p.lineTo (r.getRight() - cut, r.getBottom());
        p.lineTo (r.getX(), r.getBottom());
        p.lineTo (r.getX(), r.getY() + cut);
        p.closeSubPath();
        return p;
    }
}
