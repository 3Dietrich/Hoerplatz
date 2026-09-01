#include "PluginEditor.h"
#include "UI/Theme.h"

#include <cmath>

namespace
{
    // Feste Zeichenbreite und feste Stellenzahl: beim Ziehen soll sich in der
    // Anzeige nur die Ziffer aendern, nie ihre Lage.
    juce::Font monoFont (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
    }

    juce::String fixed (double value, int decimals, const juce::String& unit)
    {
        // Ein Wert dicht an null soll als "0.00" erscheinen, nicht als "-0.00".
        const double snap = std::pow (10.0, -decimals) * 0.5;
        if (std::abs (value) < snap)
            value = 0.0;
        return juce::String (value, decimals) + " " + unit;
    }
}

juce::Label* HoerplatzEditor::MonoLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (monoFont (12.0f));
    label->setJustificationType (juce::Justification::centredRight);
    label->setBorderSize ({ 1, 4, 1, 8 });
    return label;
}

void ReadoutPanel::paint (juce::Graphics& g)
{
    const auto a = processor.currentAlignment();
    const bool noDelay = processor.apvts.getRawParameterValue (Params::bypassDelay)->load() > 0.5f;
    const bool noGain  = processor.apvts.getRawParameterValue (Params::bypassGain)->load()  > 0.5f;

    auto area = getLocalBounds().toFloat();
    g.setColour (Theme::surface);
    g.fillRoundedRectangle (area, Theme::corner);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (area.reduced (0.5f), Theme::corner, 1.0f);

    auto body = area.reduced (10.0f, 8.0f);
    const float rowH = 17.0f;
    const float col1 = 74.0f;   // Beschriftung
    const float colW = 76.0f;   // je Wertspalte

    auto row = [&] (float y) { return juce::Rectangle<float> (body.getX(), y, body.getWidth(), rowH); };

    float y = body.getY();

    // Kopfzeile
    g.setFont (monoFont (11.0f));
    g.setColour (Theme::textDim.withAlpha (0.7f));
    g.drawText ("links",  row (y).withX (body.getX() + col1).withWidth (colW), juce::Justification::centredRight);
    g.drawText ("rechts", row (y).withX (body.getX() + col1 + colW).withWidth (colW), juce::Justification::centredRight);
    y += rowH;

    auto line = [&] (const juce::String& name, const juce::String& l, const juce::String& r, juce::Colour c)
    {
        g.setFont (monoFont (12.0f));
        g.setColour (Theme::textDim);
        g.drawText (name, row (y).withWidth (col1), juce::Justification::centredLeft);
        g.setColour (c);
        g.drawText (l, row (y).withX (body.getX() + col1).withWidth (colW), juce::Justification::centredRight);
        g.drawText (r, row (y).withX (body.getX() + col1 + colW).withWidth (colW), juce::Justification::centredRight);
        y += rowH;
    };

    line ("Weg",      fixed (a.distL, 2, "m"), fixed (a.distR, 2, "m"), Theme::text);
    line ("Laufzeit", noDelay ? juce::String ("--") : fixed (a.delayL * 1000.0, 2, "ms"),
                      noDelay ? juce::String ("--") : fixed (a.delayR * 1000.0, 2, "ms"),
          noDelay ? Theme::textDim.withAlpha (0.5f) : Theme::cyan);
    line ("Pegel",    noGain ? juce::String ("--") : fixed (juce::Decibels::gainToDecibels (a.gainL), 1, "dB"),
                      noGain ? juce::String ("--") : fixed (juce::Decibels::gainToDecibels (a.gainR), 1, "dB"),
          noGain ? Theme::textDim.withAlpha (0.5f) : Theme::cyan);

    y += 4.0f;

    // Stereobasis: 60 Grad ist das gleichseitige Dreieck, der uebliche
    // Bezugspunkt. Der Wert faerbt sich, solange er in dessen Naehe liegt.
    const bool goodBase = a.baseAngleDeg > 50.0 && a.baseAngleDeg < 70.0;
    line ("Basis",    fixed (a.baseAngleDeg, 1, juce::String::fromUTF8 ("°")), "",
          goodBase ? Theme::cyan : Theme::amber);
    line ("Blick",    fixed (a.headAngleDeg, 1, juce::String::fromUTF8 ("°")), "", Theme::text);
}

HoerplatzEditor::HoerplatzEditor (HoerplatzProcessor& p)
    : AudioProcessorEditor (&p), plugin (p), room (p), readout (p)
{
    addAndMakeVisible (room);
    addAndMakeVisible (readout);

    setUpRow (speakerDistance, Params::speakerDistance, "Boxenabstand");
    setUpRow (roomWidth,       Params::roomWidth,       "Raumbreite");
    setUpRow (roomDepth,       Params::roomDepth,       "Raumtiefe");
    setUpRow (listenerX,       Params::listenerX,       juce::String::fromUTF8 ("Hörplatz seitlich"));
    setUpRow (listenerY,       Params::listenerY,       juce::String::fromUTF8 ("Hörplatz Abstand"));

    for (auto* b : { &bypassDelay, &bypassGain })
    {
        b->setColour (juce::ToggleButton::textColourId, Theme::textDim);
        b->setColour (juce::ToggleButton::tickColourId, Theme::pink);
        b->setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::white.withAlpha (0.28f));
        addAndMakeVisible (*b);
    }
    bypassDelayAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        plugin.apvts, Params::bypassDelay, bypassDelay);
    bypassGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        plugin.apvts, Params::bypassGain, bypassGain);

    setLookAndFeel (&monoLnf);

    areaLabel.setFont (monoFont (12.0f));
    areaLabel.setColour (juce::Label::textColourId, Theme::textDim);
    areaLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (areaLabel);

    hintLabel.setFont (juce::Font (juce::FontOptions (11.5f)));
    hintLabel.setColour (juce::Label::textColourId, Theme::textDim.withAlpha (0.65f));
    hintLabel.setJustificationType (juce::Justification::topLeft);
    hintLabel.setText (juce::String::fromUTF8 (
        "Im Grundriss ziehen setzt den H\u00f6rplatz. Die weiter entfernte Box gibt den Takt vor: "
        "die n\u00e4here wird verz\u00f6gert und abgesenkt, bis beide gleichzeitig und gleich laut ankommen. "
        "Der Kopf steht in der besten Mittenstellung f\u00fcr diesen Platz."),
        juce::dontSendNotification);
    addAndMakeVisible (hintLabel);

    updateArea();

    setSize (940, 600);
    setResizable (true, true);
    setResizeLimits (760, 480, 1800, 1200);

    startTimerHz (30);
}

HoerplatzEditor::~HoerplatzEditor()
{
    setLookAndFeel (nullptr);
}

void HoerplatzEditor::setUpRow (Row& r, const char* paramId, const juce::String& text)
{
    r.label.setText (text, juce::dontSendNotification);
    r.label.setFont (juce::Font (juce::FontOptions (12.0f)));
    r.label.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (r.label);

    r.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    r.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 18);
    r.slider.setColour (juce::Slider::trackColourId, Theme::cyan.withAlpha (0.55f));
    r.slider.setColour (juce::Slider::backgroundColourId, Theme::surface2);
    r.slider.setColour (juce::Slider::thumbColourId, Theme::cyan);
    r.slider.setColour (juce::Slider::textBoxTextColourId, Theme::text);
    r.slider.setColour (juce::Slider::textBoxOutlineColourId, Theme::line);
    r.slider.setColour (juce::Slider::textBoxBackgroundColourId, Theme::surface);
    addAndMakeVisible (r.slider);

    r.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        plugin.apvts, paramId, r.slider);
}

void HoerplatzEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::ground);

    auto header = getLocalBounds().removeFromTop (34).toFloat();
    g.setColour (Theme::surface2);
    g.fillRect (header);
    g.setColour (Theme::cyan);
    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    g.drawText (juce::String::fromUTF8 ("Hörplatz"), header.reduced (12.0f, 0.0f),
                juce::Justification::centredLeft);
    g.setColour (Theme::textDim.withAlpha (0.75f));
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (juce::String::fromUTF8 ("Laufzeit und Pegel beider Boxen auf den Sitzplatz ausgerichtet"),
                header.reduced (100.0f, 0.0f), juce::Justification::centredLeft);
}

void HoerplatzEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (34);

    auto side = area.removeFromRight (300).reduced (10);
    room.setBounds (area.reduced (6));

    auto place = [&] (Row& r)
    {
        r.label.setBounds (side.removeFromTop (15));
        r.slider.setBounds (side.removeFromTop (22));
        side.removeFromTop (6);
    };

    place (speakerDistance);
    place (roomWidth);
    place (roomDepth);
    areaLabel.setBounds (side.removeFromTop (18));
    side.removeFromTop (6);
    place (listenerX);
    place (listenerY);

    side.removeFromTop (4);
    bypassDelay.setBounds (side.removeFromTop (22));
    bypassGain.setBounds (side.removeFromTop (22));
    side.removeFromTop (8);

    readout.setBounds (side.removeFromTop (juce::jmin (120, side.getHeight())));
    side.removeFromTop (10);
    hintLabel.setBounds (side);
}

void HoerplatzEditor::updateArea()
{
    const float w = plugin.apvts.getRawParameterValue (Params::roomWidth)->load();
    const float d = plugin.apvts.getRawParameterValue (Params::roomDepth)->load();
    areaLabel.setText (juce::String::fromUTF8 ("Fläche  ") + juce::String (w * d, 1)
                       + juce::String::fromUTF8 (" m²"), juce::dontSendNotification);
}

void HoerplatzEditor::timerCallback()
{
    updateArea();
    room.repaint();
    readout.repaint();
}
