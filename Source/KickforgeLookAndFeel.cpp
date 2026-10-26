#include "KickforgeLookAndFeel.h"

using namespace kf;
using W = theme::Weight;

namespace
{
    // JUCE-Rotary-Winkel: 0 = 12 Uhr, im Uhrzeigersinn
    juce::Point<float> polar (juce::Point<float> c, float r, float angle)
    {
        return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) };
    }
}

KickforgeLookAndFeel::KickforgeLookAndFeel()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId,      theme::bg);
    setColour (Label::textColourId,                      theme::dim);

    setColour (Slider::textBoxTextColourId,              theme::text);
    setColour (Slider::textBoxOutlineColourId,           Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId,        Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId,         theme::brand.withAlpha (0.4f));

    setColour (ComboBox::backgroundColourId,             theme::recess);
    setColour (ComboBox::textColourId,                   theme::text);
    setColour (ComboBox::outlineColourId,                theme::edge);
    setColour (ComboBox::arrowColourId,                  theme::brand);

    setColour (PopupMenu::backgroundColourId,            Colour (0xff151312));
    setColour (PopupMenu::textColourId,                  theme::text);
    setColour (PopupMenu::highlightedBackgroundColourId, theme::brand);
    setColour (PopupMenu::highlightedTextColourId,       Colour (0xff140c07));

    setColour (TextButton::textColourOffId,              theme::text);
    setColour (TextButton::textColourOnId,               theme::text);

    setColour (TextEditor::backgroundColourId,           theme::recess);
    setColour (TextEditor::textColourId,                 theme::text);
    setColour (TextEditor::outlineColourId,              theme::edge);
    setColour (TextEditor::focusedOutlineColourId,       theme::brand);
    setColour (CaretComponent::caretColourId,            theme::brand);

    setColour (AlertWindow::backgroundColourId,          theme::panel);
    setColour (AlertWindow::textColourId,                theme::text);
    setColour (AlertWindow::outlineColourId,             theme::edgeHi);
}

// =========================================================================
// Rotary Knob: Stahlkörper + Ring-Track + leuchtender Wertbogen + Zeiger
// =========================================================================
void KickforgeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                             float pos, float a0, float a1, juce::Slider& s)
{
    using namespace juce;

    const auto accent  = s.findColour (Slider::rotarySliderFillColourId);
    const auto size    = s.getProperties()["size"].toString();
    const bool big     = size == "big";
    const bool small   = size == "small";
    const bool bipolar = (bool) s.getProperties()["bipolar"];
    const bool enabled = s.isEnabled();

    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (1.0f);
    const float d = jmin (bounds.getWidth(), bounds.getHeight());
    if (d < 8.0f) return;

    const auto  c      = bounds.getCentre();
    const float trackW = jmax (2.5f, d * (big ? 0.068f : 0.08f));
    const float tickRm = big ? d * 0.07f : 0.0f;                      // Platz für Skalenstriche
    const float rArc   = d * 0.5f - trackW * 0.5f - tickRm - 0.5f;
    const float angle  = a0 + pos * (a1 - a0);

    // --- Skalenstriche (nur große Knobs) ---
    if (big)
    {
        g.setColour (theme::faint);
        for (int i = 0; i <= 10; ++i)
        {
            const float a  = a0 + (float) i / 10.0f * (a1 - a0);
            const float r1 = rArc + trackW * 0.5f + 2.0f;
            const float r2 = r1 + ((i % 5 == 0) ? tickRm - 1.0f : tickRm * 0.55f);
            g.drawLine ({ polar (c, r1, a), polar (c, r2, a) }, (i % 5 == 0) ? 1.4f : 1.0f);
        }
    }

    // --- Track ---
    Path track;
    track.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, a0, a1, true);
    g.setColour (theme::track);
    g.strokePath (track, PathStrokeType (trackW, PathStrokeType::curved, PathStrokeType::butt));

    // --- Wertbogen mit Glow ---
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    if (std::abs (angle - from) > 0.002f)
    {
        Path v;
        v.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, jmin (from, angle), jmax (from, angle), true);
        const auto col = enabled ? accent : accent.withSaturation (0.1f).withAlpha (0.5f);
        g.setColour (col.withAlpha (0.16f));
        g.strokePath (v, PathStrokeType (trackW * 2.8f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (col);
        g.strokePath (v, PathStrokeType (trackW, PathStrokeType::curved, PathStrokeType::butt));
    }

    // --- Mittelmarke bei bipolar ---
    if (bipolar)
    {
        g.setColour (theme::dim);
        const float am = (a0 + a1) * 0.5f;
        g.drawLine ({ polar (c, rArc - trackW * 0.5f, am), polar (c, rArc + trackW * 0.5f, am) }, 1.2f);
    }

    // --- Knob-Körper (Stahl) ---
    const float gap   = jmax (2.0f, d * 0.065f);
    const float rBody = rArc - trackW * 0.5f - gap;
    if (rBody < 3.0f) return;
    auto body = Rectangle<float> (rBody * 2.0f, rBody * 2.0f).withCentre (c);

    g.setColour (Colours::black.withAlpha (0.5f));                 // Schlagschatten
    g.fillEllipse (body.translated (0.0f, 1.6f).expanded (0.8f));

    g.setGradientFill (ColourGradient (theme::knobTop, c.x, body.getY(),
                                       theme::knobBot, c.x, body.getBottom(), false));
    g.fillEllipse (body);

    if (! small)                                                    // eingelassene Kappe
    {
        auto cap = body.reduced (rBody * 0.24f);
        g.setGradientFill (ColourGradient (theme::knobBot.brighter (0.1f), c.x, cap.getY(),
                                           theme::knobTop.darker (0.15f), c.x, cap.getBottom(), false));
        g.fillEllipse (cap);
    }

    g.setGradientFill (ColourGradient (theme::edgeHi.brighter (0.25f), c.x, body.getY(),
                                       theme::edge.darker (0.4f), c.x, body.getBottom(), false));
    g.drawEllipse (body.reduced (0.5f), 1.0f);                      // Kantenlicht oben

    // --- Zeiger ---
    const float pIn  = rBody * (small ? 0.2f : 0.38f);
    const float pOut = rBody * 0.9f;
    const auto pointerCol = enabled ? accent.interpolatedWith (Colours::white, 0.45f) : theme::dim;
    g.setColour (pointerCol);
    Path ptr;
    ptr.startNewSubPath (polar (c, pIn, angle));
    ptr.lineTo (polar (c, pOut, angle));
    g.strokePath (ptr, PathStrokeType (jmax (2.0f, d * 0.038f), PathStrokeType::curved, PathStrokeType::rounded));
}

juce::Label* KickforgeLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    const bool small = s.getProperties()["size"].toString() == "small";
    l->setFont (fonts().get (W::SemiBold, small ? 11.5f : 12.5f, 0.02f));
    l->setColour (juce::Label::textColourId,               theme::text.withAlpha (0.88f));
    l->setColour (juce::Label::backgroundColourId,         juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId,            juce::Colours::transparentBlack);
    l->setColour (juce::TextEditor::backgroundColourId,    theme::recess);
    l->setColour (juce::TextEditor::textColourId,          theme::text);
    l->setColour (juce::TextEditor::highlightColourId,     theme::brand.withAlpha (0.4f));
    l->setColour (juce::TextEditor::outlineColourId,       theme::edge);
    l->setColour (juce::TextEditor::focusedOutlineColourId,theme::brand);
    return l;
}

juce::Slider::SliderLayout KickforgeLookAndFeel::getSliderLayout (juce::Slider& s)
{
    // Rotary: Knob oben, Wert direkt darunter (ohne großen Abstand)
    juce::Slider::SliderLayout layout;
    auto b = s.getLocalBounds();
    const int tbH = s.getTextBoxHeight();
    if (s.getTextBoxPosition() == juce::Slider::TextBoxBelow && tbH > 0)
    {
        layout.textBoxBounds = b.removeFromBottom (tbH);
        layout.sliderBounds  = b;
    }
    else
    {
        layout.sliderBounds = b;
    }
    return layout;
}

// =========================================================================
// Toggle als LED-Schalter
// =========================================================================
void KickforgeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                             bool highlighted, bool)
{
    using namespace juce;
    const auto accent = b.findColour (ToggleButton::tickColourId);
    const bool on     = b.getToggleState();
    auto r = b.getLocalBounds().toFloat();

    const float ledH = jmin (9.0f, r.getHeight() - 6.0f);
    const float ledW = ledH * 1.9f;
    auto led = Rectangle<float> (r.getX() + 2.0f, r.getCentreY() - ledH * 0.5f, ledW, ledH);

    if (on)
    {
        g.setColour (accent.withAlpha (0.22f));
        g.fillRoundedRectangle (led.expanded (3.0f), 3.0f);       // Glow
        g.setGradientFill (ColourGradient (accent.brighter (0.35f), led.getX(), led.getY(),
                                           accent.darker (0.2f),   led.getX(), led.getBottom(), false));
        g.fillRoundedRectangle (led, 1.5f);
    }
    else
    {
        g.setColour (theme::recess);
        g.fillRoundedRectangle (led, 1.5f);
        g.setColour (highlighted ? theme::edgeHi.brighter (0.2f) : theme::edgeHi);
        g.drawRoundedRectangle (led, 1.5f, 1.0f);
    }

    g.setColour (on ? theme::text : (highlighted ? theme::text.withAlpha (0.75f) : theme::dim));
    g.setFont (fonts().get (W::Bold, 12.5f, 0.08f));
    g.drawText (b.getButtonText(),
                r.withTrimmedLeft (ledW + 9.0f).toNearestInt(),
                Justification::centredLeft, false);
}

// =========================================================================
// Buttons: kantig, Primary = orange gefüllt
// =========================================================================
void KickforgeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                                 bool highlighted, bool down)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool primary = (bool) b.getProperties()["primary"];
    const auto shape = theme::chamferRect (r, jmin (7.0f, r.getHeight() * 0.28f));

    if (primary)
    {
        auto top = theme::brandHi, bot = theme::brandLo;
        if (highlighted) { top = top.brighter (0.12f); bot = bot.brighter (0.12f); }
        if (down)        { top = top.darker (0.25f);  bot = bot.darker (0.25f); }
        g.setGradientFill (ColourGradient (top, 0.0f, r.getY(), bot, 0.0f, r.getBottom(), false));
        g.fillPath (shape);
        g.setColour (Colours::white.withAlpha (0.25f));
        g.drawLine (r.getX() + 8.0f, r.getY() + 1.0f, r.getRight() - 2.0f, r.getY() + 1.0f, 1.0f);
        return;
    }

    auto top = Colour (0xff221f1d), bot = Colour (0xff161413);
    if (down) { top = theme::brand.withAlpha (0.35f); bot = theme::brand.withAlpha (0.2f); }
    g.setGradientFill (ColourGradient (top, 0.0f, r.getY(), bot, 0.0f, r.getBottom(), false));
    g.fillPath (shape);
    g.setColour (highlighted ? theme::brand.withAlpha (0.7f) : theme::edgeHi);
    g.strokePath (shape, PathStrokeType (1.0f));
}

juce::Font KickforgeLookAndFeel::getTextButtonFont (juce::TextButton& b, int h)
{
    const bool primary = (bool) b.getProperties()["primary"];
    return fonts().get (W::Bold, juce::jmin (primary ? 16.0f : 13.5f, (float) h * 0.55f), 0.1f);
}

void KickforgeLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool primary = (bool) b.getProperties()["primary"];
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (primary ? juce::Colour (0xff1a0d05)
                         : (b.isEnabled() ? theme::text : theme::faint));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

// =========================================================================
// ComboBox: eingelassenes Feld mit oranger Chevron
// =========================================================================
void KickforgeLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                         int, int, int, int, juce::ComboBox& box)
{
    using namespace juce;
    auto r = Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const auto shape = theme::chamferRect (r, jmin (5.0f, r.getHeight() * 0.25f));

    g.setColour (box.findColour (ComboBox::backgroundColourId));
    g.fillPath (shape);
    g.setColour (box.isMouseOver (true) ? theme::edgeHi.brighter (0.25f)
                                        : box.findColour (ComboBox::outlineColourId));
    g.strokePath (shape, PathStrokeType (1.0f));

    // Chevron
    const float cx = (float) width - 12.0f, cy = (float) height * 0.5f;
    Path chev;
    chev.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chev.lineTo (cx, cy + 2.5f);
    chev.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (box.findColour (ComboBox::arrowColourId));
    g.strokePath (chev, PathStrokeType (1.8f, PathStrokeType::mitered, PathStrokeType::square));
}

juce::Font KickforgeLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return fonts().get (W::SemiBold, juce::jmin (14.0f, (float) box.getHeight() * 0.6f), 0.02f);
}

void KickforgeLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (7, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font KickforgeLookAndFeel::getPopupMenuFont()
{
    return fonts().get (W::SemiBold, 14.5f, 0.02f);
}

void KickforgeLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (theme::edgeHi);
    g.drawRect (0, 0, width, height, 1);
}

// =========================================================================
// Fonts
// =========================================================================
juce::Font KickforgeLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

juce::Typeface::Ptr KickforgeLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    // Alles ohne explizite Schrift (AlertWindows, FileChooser-Fallbacks …)
    // läuft ebenfalls in Chakra Petch.
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return f.isBold() ? fonts().bold : fonts().semibold;
    return LookAndFeel_V4::getTypefaceForFont (f);
}
