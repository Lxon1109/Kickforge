#pragma once
#include <cmath>
#include <juce_core/juce_core.h>

/*
    Waveshaper-Zoo für Kickforge.
    Alle Shaper erwarten Eingang um ±1 .. ±N (nach Pre-Gain) und geben
    Werte in ~[-1, 1] zurück. Pre-Gain wird vorher appliziert.

    Modi (abgestimmt auf Hardtechno/Schranz):
      0 TANH     — klassische weiche Sättigung (für wärmeren Druck)
      1 HARDCLIP — brettharter Clip, der Schranz-Standard
      2 FOLD     — Foldback-Distortion, metallisch/industrial
      3 BITCRUSH — Sample-Reduction + Quantisierung, dreckig
*/
namespace kf
{
    enum class SatMode : int { Tanh = 0, HardClip = 1, Fold = 2, BitCrush = 3 };

    inline float shapeTanh (float x) noexcept
    {
        return std::tanh (x);
    }

    inline float shapeHardClip (float x) noexcept
    {
        // Harter Clip bei ±1 — klassisch Schranz/Hoover
        return juce::jlimit (-1.0f, 1.0f, x);
    }

    inline float shapeFold (float x) noexcept
    {
        // Dreieck-Foldback — bringt harmonisch harten, "ratternden"
        // Industrial-Charakter in den Body
        float y = x;
        while (y >  1.0f) y =  2.0f - y;
        while (y < -1.0f) y = -2.0f - y;
        return y;
    }

    inline float shapeBitcrush (float x, float bits) noexcept
    {
        // bits: 2..16 — kleiner = dreckiger
        const float steps = std::exp2 (bits) * 0.5f;
        return std::round (juce::jlimit (-1.0f, 1.0f, x) * steps) / steps;
    }

    inline float applyShaper (float x, SatMode mode, float bitDepth = 8.0f) noexcept
    {
        switch (mode)
        {
            case SatMode::Tanh:     return shapeTanh (x);
            case SatMode::HardClip: return shapeHardClip (x);
            case SatMode::Fold:     return shapeFold (x);
            case SatMode::BitCrush: return shapeBitcrush (x, bitDepth);
        }
        return x;
    }

    // Soft-Clip-Limiter am Master-Ausgang (verhindert digitales Clipping
    // und gibt der Kick zusätzlich etwas Analog-Charakter)
    inline float softClipOutput (float x) noexcept
    {
        // 2/pi * atan — mild, erst oberhalb ±0.7 spürbar
        return (2.0f / juce::MathConstants<float>::pi) * std::atan (x * 1.4f);
    }
}
