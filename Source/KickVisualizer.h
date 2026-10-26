#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Theme.h"

/*
    KickVisualizer — zeigt eine Kick wie ein Kick-Builder:
      • CLICK / PUNCH / RUMBLE / GRIT als farbige Hüllkurven-Flächen (vor dem Master)
      • Summe (nach dem Master) als helle Kontur
      • Markierung am Beat-Ende (harter Schnitt), Zeitlineal in ms
*/
class KickVisualizer : public juce::Component
{
public:
    static constexpr int numLayers = 4;

    KickVisualizer() { setOpaque (false); }

    // parts: 4 Kanäle (Mono-Summe je Schicht), sum: 1 Kanal
    void setData (const juce::AudioBuffer<float>& partsIn, const juce::AudioBuffer<float>& sumIn,
                  double sr, int beatEndSample, double bpmIn, float lengthBeatsIn)
    {
        parts.makeCopyOf (partsIn, true);
        sum.makeCopyOf (sumIn, true);
        sampleRate = sr;
        beatEnd = beatEndSample;
        bpm = bpmIn;
        lengthBeats = lengthBeatsIn;
        analyse();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        namespace T = kf::theme;

        auto outer = getLocalBounds().toFloat();
        const auto frame = T::chamferRect (outer.reduced (0.5f), 10.0f);
        g.setGradientFill (ColourGradient (Colour (0xff0b0a09), 0.0f, outer.getY(),
                                           Colour (0xff121010), 0.0f, outer.getBottom(), false));
        g.fillPath (frame);

        auto plot = outer.reduced (14.0f, 10.0f).withTrimmedTop (16.0f).withTrimmedBottom (16.0f);
        const float midY = plot.getCentreY();
        const float h2   = plot.getHeight() * 0.5f;

        // Raster + Lineal
        g.setFont (fonts->get (T::Weight::SemiBold, 10.5f, 0.06f));
        if (sampleRate > 0.0 && displaySamples > 0)
        {
            const double lenMs = 1000.0 * displaySamples / sampleRate;
            const double step  = lenMs > 900 ? 200.0 : lenMs > 450 ? 100.0 : lenMs > 180 ? 50.0 : 25.0;
            for (double ms = 0.0; ms <= lenMs + 0.01; ms += step)
            {
                const float x = plot.getX() + (float) (ms / lenMs) * plot.getWidth();
                g.setColour (Colour (0xff1d1a18));
                g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
                g.setColour (T::faint);
                g.drawText (ms < 0.5 ? String ("0") : String ((int) ms) + " ms",
                            Rectangle<float> (x - 30.0f, plot.getBottom() + 3.0f, 60.0f, 12.0f).toNearestInt(),
                            Justification::centred, false);
            }
        }
        g.setColour (Colour (0xff2a2624));
        g.drawHorizontalLine ((int) midY, plot.getX(), plot.getRight());

        const Colour cols[numLayers] = { T::click, T::punch, T::rumble, T::grit };
        if (parts.getNumChannels() >= numLayers && displaySamples > 0)
            for (int ch : { 2, 1, 3, 0 })          // Rumble hinten, Click vorne
                drawEnvelope (g, parts.getReadPointer (ch), plot, midY, h2, cols[ch], true);

        if (sum.getNumSamples() > 0 && displaySamples > 0)
            drawEnvelope (g, sum.getReadPointer (0), plot, midY, h2, T::text, false);

        // Beat-Ende
        if (beatEnd > 0 && beatEnd < displaySamples)
        {
            const float x = plot.getX() + (float) beatEnd / (float) displaySamples * plot.getWidth();
            g.setColour (T::brand.withAlpha (0.75f));
            for (float y = plot.getY(); y < plot.getBottom(); y += 6.0f)
                g.drawVerticalLine ((int) x, y, juce::jmin (y + 3.0f, plot.getBottom()));
            g.setFont (fonts->get (T::Weight::Bold, 10.0f, 0.1f));
            const bool whole = std::abs (lengthBeats - std::round (lengthBeats)) < 1.0e-3f;
            const String txt = (whole ? String (juce::roundToInt (lengthBeats)) : String (lengthBeats, 2))
                             + (std::abs (lengthBeats - 1.0f) < 1.0e-3f ? " BEAT @ " : " BEATS @ ") + String (bpm, 1) + " BPM";
            g.drawText (txt, Rectangle<float> (x - 170.0f, plot.getY() - 1.0f, 164.0f, 12.0f).toNearestInt(),
                        Justification::centredRight, false);
        }

        // Legende
        auto legend = outer.reduced (16.0f, 7.0f).removeFromTop (14.0f);
        const char* names[numLayers] = { "CLICK", "PUNCH", "RUMBLE", "GRIT" };
        g.setFont (fonts->get (T::Weight::Bold, 10.5f, 0.12f));
        for (int i = 0; i < numLayers; ++i)
        {
            g.setColour (cols[i]);
            g.fillRect (legend.removeFromLeft (8.0f).withSizeKeepingCentre (8.0f, 8.0f));
            legend.removeFromLeft (5.0f);
            g.setColour (T::dim);
            g.drawText (names[i], legend.removeFromLeft (i == 2 ? 54.0f : 48.0f).toNearestInt(),
                        Justification::centredLeft, false);
        }
        g.setColour (T::text);
        g.fillRect (legend.removeFromLeft (12.0f).withSizeKeepingCentre (12.0f, 2.0f));
        legend.removeFromLeft (5.0f);
        g.setColour (T::dim);
        g.drawText ("SUM", legend.removeFromLeft (40.0f).toNearestInt(), Justification::centredLeft, false);

        // Peak
        const float peakDb = Decibels::gainToDecibels (sumPeak, -60.0f);
        g.setColour (peakDb > -0.3f ? T::brand : T::dim);
        g.drawText ("PEAK " + String (peakDb, 1) + " dB",
                    outer.reduced (16.0f, 7.0f).removeFromTop (14.0f).toNearestInt(),
                    Justification::centredRight, false);

        g.setColour (T::edge);
        g.strokePath (frame, PathStrokeType (1.0f));
    }

private:
    void analyse()
    {
        sumPeak = sum.getNumSamples() > 0 ? sum.getMagnitude (0, 0, sum.getNumSamples()) : 0.0f;
        float maxAll = sumPeak;
        for (int ch = 0; ch < parts.getNumChannels(); ++ch)
            maxAll = juce::jmax (maxAll, parts.getMagnitude (ch, 0, parts.getNumSamples()));
        displayGain = maxAll > 1.0e-5f ? 0.92f / maxAll : 1.0f;

        // Anzeige bis die Kick unter -40 dB liegt, mindestens bis kurz nach dem Beat-Ende
        const float thresh = maxAll * 0.01f;
        int last = 0;
        auto scan = [&] (const float* d, int n)
        {
            for (int i = n - 1; i > last; --i)
                if (std::abs (d[i]) > thresh) { last = i; break; }
        };
        if (sum.getNumSamples() > 0) scan (sum.getReadPointer (0), sum.getNumSamples());
        const int maxLen = sum.getNumSamples();
        displaySamples = juce::jlimit (juce::jmin (maxLen, (int) (0.15 * sampleRate)), maxLen,
                                       juce::jmax ((int) (last * 1.06), (int) (beatEnd * 1.08)));
    }

    void drawEnvelope (juce::Graphics& g, const float* d, juce::Rectangle<float> plot,
                       float midY, float h2, juce::Colour col, bool filled) const
    {
        using namespace juce;
        const int numPx = jmax (2, (int) plot.getWidth());
        Path top, bot;
        for (int px = 0; px < numPx; ++px)
        {
            const int s0 = (int) ((int64) px * displaySamples / numPx);
            const int s1 = jmax (s0 + 1, (int) ((int64) (px + 1) * displaySamples / numPx));
            float mn = 0.0f, mx = 0.0f;
            for (int i = s0; i < s1; ++i) { mx = jmax (mx, d[i]); mn = jmin (mn, d[i]); }
            const float x = plot.getX() + (float) px;
            const float yT = midY - jlimit (-1.05f, 1.05f, mx * displayGain) * h2;
            const float yB = midY - jlimit (-1.05f, 1.05f, mn * displayGain) * h2;
            if (px == 0) { top.startNewSubPath (x, yT); bot.startNewSubPath (x, yB); }
            else         { top.lineTo (x, yT);          bot.lineTo (x, yB); }
        }
        if (filled)
        {
            Path area (top);
            Path::Iterator it (bot);
            std::vector<Point<float>> pts;
            while (it.next()) pts.push_back ({ it.x1, it.y1 });
            for (auto p = pts.rbegin(); p != pts.rend(); ++p) area.lineTo (*p);
            area.closeSubPath();
            g.setColour (col.withAlpha (0.22f));
            g.fillPath (area);
            g.setColour (col.withAlpha (0.8f));
            g.strokePath (top, PathStrokeType (1.0f));
            g.strokePath (bot, PathStrokeType (1.0f));
        }
        else
        {
            g.setColour (col.withAlpha (0.92f));
            g.strokePath (top, PathStrokeType (1.2f));
            g.strokePath (bot, PathStrokeType (1.2f));
        }
    }

    juce::SharedResourcePointer<kf::theme::FontSet> fonts;
    juce::AudioBuffer<float> parts, sum;
    double sampleRate = 44100.0, bpm = 150.0;
    float  lengthBeats = 1.0f;
    int    beatEnd = 0;
    float  sumPeak = 0.0f, displayGain = 1.0f;
    int    displaySamples = 0;
};
