#include "PluginEditor.h"
#include "UI/Theme.h"

#include <cmath>

namespace
{
    // Feste Zeichenbreite: beim Ziehen soll sich in der Anzeige nur die
    // Ziffer aendern, nie ihre Lage.
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

void ReadoutPanel::paint (juce::Graphics& g)
{
    const auto& t = texts (lang);
    const auto a = processor.currentAlignment();
    const bool noDelay = processor.apvts.getRawParameterValue (Params::bypassDelay)->load() > 0.5f;
    const bool noGain  = processor.apvts.getRawParameterValue (Params::bypassGain)->load()  > 0.5f;

    auto area = getLocalBounds().toFloat();
    g.setColour (Theme::surface);
    g.fillRoundedRectangle (area, Theme::corner);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (area.reduced (0.5f), Theme::corner, 1.0f);

    auto body = area.reduced (10.0f, 6.0f);
    const float rowH = 16.0f;
    const float col1 = 70.0f;

    auto row = [&] (float y) { return juce::Rectangle<float> (body.getX(), y, body.getWidth(), rowH); };
    float y = body.getY();

    auto line = [&] (const juce::String& name, const juce::String& value, juce::Colour c)
    {
        g.setFont (monoFont (11.0f));
        g.setColour (Theme::textDim.withAlpha (0.55f));
        g.drawText (name, row (y).withWidth (col1), juce::Justification::centredLeft);
        g.setColour (c);
        g.drawText (value, row (y).withTrimmedLeft (col1), juce::Justification::centredRight);
        y += rowH;
    };

    // Korrigiert wird immer nur die naehere Box - welche das ist, steht oben,
    // darunter um wieviel.
    const bool centred = a.centred();
    const juce::String side = centred ? juce::String (t.centred)
                                      : juce::String (a.leftIsNearer() ? t.sideLeft : t.sideRight);

    // Gedaempft: hier wird nichts eingestellt, hier steht nur, was die
    // Einstellung gerade bedeutet.
    const auto valueColour = Theme::textDim.withAlpha (0.8f);

    line (t.correction, side, centred ? Theme::textDim.withAlpha (0.5f)
                                      : Theme::amber.withAlpha (0.7f));
    line (t.delayRow, noDelay ? juce::String (t.off) : fixed (a.delaySeconds() * 1000.0, 2, "ms"),
          noDelay ? Theme::textDim.withAlpha (0.4f) : valueColour);
    line (t.gainRow, noGain ? juce::String (t.off)
                            : fixed (juce::Decibels::gainToDecibels (a.gainRatio()), 1, "dB"),
          noGain ? Theme::textDim.withAlpha (0.4f) : valueColour);
}

juce::Label* HoerplatzEditor::MonoLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (monoFont (12.0f));
    label->setJustificationType (juce::Justification::centredRight);
    label->setBorderSize ({ 1, 4, 1, 8 });
    return label;
}

void HoerplatzEditor::MonoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y,
                                                         int width, int height, float sliderPos,
                                                         float startAngle, float endAngle,
                                                         juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float ring = radius - 3.0f;

    juce::Path back;
    back.addCentredArc (centre.x, centre.y, ring, ring, 0.0f, startAngle, endAngle, true);
    g.setColour (Theme::surface2);
    g.strokePath (back, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, ring, ring, 0.0f, startAngle, angle, true);
    g.setColour (Theme::cyan);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    const juce::Point<float> tip { centre.x + std::sin (angle) * (ring - 2.0f),
                                   centre.y - std::cos (angle) * (ring - 2.0f) };
    g.setColour (Theme::cyan);
    g.drawLine ({ centre + (tip - centre) * 0.35f, tip }, 2.0f);
}

HoerplatzEditor::HoerplatzEditor (HoerplatzProcessor& p)
    : AudioProcessorEditor (&p), plugin (p), room (p), readout (p)
{
    setLookAndFeel (&monoLnf);

    // Sprache und Hilfe kommen aus dem gespeicherten Zustand.
    lang = (int) plugin.apvts.state.getProperty (Params::language, 0) == 1 ? Lang::en : Lang::de;
    showHelp = (bool) plugin.apvts.state.getProperty (Params::showHelp, true);

    addAndMakeVisible (room);
    addAndMakeVisible (readout);

    setUpRow (speakerDistance, Params::speakerDistance, true);

    // Kennung, damit die Pruefung den Regler findet, ohne die Reihenfolge
    // der Kindkomponenten zu erraten.
    speakerDistance.slider.setComponentID ("speakerDistance");

    setUpRow (roomWidth, Params::roomWidth, false);
    setUpRow (roomDepth, Params::roomDepth, false);
    setUpRow (listenerX, Params::listenerX, true);
    setUpRow (listenerY, Params::listenerY, true);

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

    gainAmountKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainAmountKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 13);
    gainAmountKnob.setColour (juce::Slider::rotarySliderFillColourId, Theme::cyan);
    gainAmountKnob.setColour (juce::Slider::rotarySliderOutlineColourId, Theme::surface2);
    gainAmountKnob.setColour (juce::Slider::thumbColourId, Theme::cyan);
    gainAmountKnob.setColour (juce::Slider::textBoxTextColourId, Theme::text);
    gainAmountKnob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    gainAmountKnob.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (gainAmountKnob);
    gainAmountAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        plugin.apvts, Params::gainAmount, gainAmountKnob);

    gainAmountLabel.setFont (juce::Font (juce::FontOptions (Theme::smallText)));
    gainAmountLabel.setColour (juce::Label::textColourId, Theme::textDim);
    gainAmountLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (gainAmountLabel);

    areaLabel.setFont (monoFont (11.0f));
    areaLabel.setColour (juce::Label::textColourId, Theme::textDim.withAlpha (0.55f));
    addAndMakeVisible (areaLabel);

    testToneButton.setClickingTogglesState (true);
    testToneButton.setColour (juce::TextButton::buttonColourId, Theme::surface);
    testToneButton.setColour (juce::TextButton::buttonOnColourId, Theme::amber.withAlpha (0.22f));
    testToneButton.setColour (juce::TextButton::textColourOffId, Theme::textDim);
    testToneButton.setColour (juce::TextButton::textColourOnId, Theme::amber);
    testToneButton.onClick = [this]
    {
        plugin.testTone.setActive (testToneButton.getToggleState());
    };
    addAndMakeVisible (testToneButton);

    // Sprache und Hilfe: zwei kleine Schalter in der Kopfzeile.
    for (auto* b : { &langButton, &helpButton })
    {
        b->setColour (juce::TextButton::buttonColourId, Theme::surface);
        b->setColour (juce::TextButton::buttonOnColourId, Theme::cyan.withAlpha (0.22f));
        b->setColour (juce::TextButton::textColourOffId, Theme::textDim);
        b->setColour (juce::TextButton::textColourOnId, Theme::cyan);
        addAndMakeVisible (*b);
    }
    helpButton.setButtonText ("?");
    helpButton.setClickingTogglesState (true);
    helpButton.setToggleState (showHelp, juce::dontSendNotification);
    helpButton.onClick = [this]
    {
        showHelp = helpButton.getToggleState();
        plugin.apvts.state.setProperty (Params::showHelp, showHelp, nullptr);
        applyHelp();
    };
    langButton.onClick = [this]
    {
        lang = (lang == Lang::de ? Lang::en : Lang::de);
        plugin.apvts.state.setProperty (Params::language, lang == Lang::en ? 1 : 0, nullptr);
        applyLanguage();
    };

    applyLanguage();
    applyHelp();
    updateArea();

    setSize (940, 560);
    setResizable (true, true);
    // Klein genug, dass nur noch das Noetige dasteht: Boxenabstand,
    // Hoerplatz, die beiden Bypaesse mit dem Ausgleich, das Testgeraeusch.
    setResizeLimits (300, 250, 1800, 1200);

    startTimerHz (30);
}

HoerplatzEditor::~HoerplatzEditor()
{
    setLookAndFeel (nullptr);
}

void HoerplatzEditor::styleSlider (juce::Slider& s, bool primary)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 66, 18);
    s.setColour (juce::Slider::trackColourId, primary ? Theme::cyan.withAlpha (0.55f)
                                                      : juce::Colours::white.withAlpha (0.16f));
    s.setColour (juce::Slider::backgroundColourId, Theme::surface2);
    s.setColour (juce::Slider::thumbColourId, primary ? Theme::cyan
                                                      : Theme::textDim.withAlpha (0.6f));
    s.setColour (juce::Slider::textBoxTextColourId, primary ? Theme::text
                                                            : Theme::textDim.withAlpha (0.7f));
    s.setColour (juce::Slider::textBoxOutlineColourId, primary ? Theme::line
                                                              : juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId, primary ? Theme::surface
                                                                  : juce::Colours::transparentBlack);
}

void HoerplatzEditor::setUpRow (Row& r, const char* paramId, bool primary)
{
    r.label.setFont (juce::Font (juce::FontOptions (primary ? 12.0f : 11.0f)));
    r.label.setColour (juce::Label::textColourId, primary ? Theme::textDim
                                                          : Theme::textDim.withAlpha (0.55f));
    addAndMakeVisible (r.label);

    styleSlider (r.slider, primary);
    addAndMakeVisible (r.slider);

    r.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        plugin.apvts, paramId, r.slider);
}

void HoerplatzEditor::applyLanguage()
{
    const auto& t = texts (lang);

    speakerDistance.label.setText (t.speakerDistance, juce::dontSendNotification);
    roomWidth.label.setText (t.roomWidth, juce::dontSendNotification);
    roomDepth.label.setText (t.roomDepth, juce::dontSendNotification);
    listenerX.label.setText (t.listenerX, juce::dontSendNotification);
    listenerY.label.setText (t.listenerY, juce::dontSendNotification);

    gainAmountLabel.setText (t.gainAmount, juce::dontSendNotification);
    testToneButton.setButtonText (t.testTone);
    testToneButton.setTooltip (t.helpTestTone);
    gainAmountKnob.setTooltip (t.helpGainAmount);

    bypassDelay.setButtonText (t.bypassDelay);
    bypassGain.setButtonText (t.bypassGain);

    speakerDistance.slider.setTooltip (t.helpSpeakerDistance);
    roomWidth.slider.setTooltip (t.helpRoomWidth);
    roomDepth.slider.setTooltip (t.helpRoomDepth);
    listenerX.slider.setTooltip (t.helpListenerX);
    listenerY.slider.setTooltip (t.helpListenerY);
    bypassDelay.setTooltip (t.helpBypassDelay);
    bypassGain.setTooltip (t.helpBypassGain);
    readout.setTooltip (t.helpReadout);
    helpButton.setTooltip (t.helpHints);
    langButton.setTooltip (t.helpLang);

    // Der Schalter zeigt, wohin er fuehrt.
    langButton.setButtonText (lang == Lang::de ? "EN" : "DE");

    room.setLang (lang);
    readout.setLang (lang);

    updateArea();
    repaint();
}

void HoerplatzEditor::applyHelp()
{
    // Ohne Hilfe gibt es kein Fenster, das die Sprechblasen zeigt - die
    // Texte selbst bleiben an ihren Bedienelementen haengen.
    if (showHelp)
        tooltips = std::make_unique<juce::TooltipWindow> (this, 700);
    else
        tooltips.reset();
}

void HoerplatzEditor::updateArea()
{
    const auto& t = texts (lang);
    const float w = plugin.apvts.getRawParameterValue (Params::roomWidth)->load();
    const float d = plugin.apvts.getRawParameterValue (Params::roomDepth)->load();
    areaLabel.setText (juce::String (t.area) + "  " + juce::String (w * d, 1)
                       + juce::String::fromUTF8 (" m²"), juce::dontSendNotification);
}

void HoerplatzEditor::paint (juce::Graphics& g)
{
    const auto& t = texts (lang);

    g.fillAll (Theme::ground);

    auto header = getLocalBounds().removeFromTop (34).toFloat();
    g.setColour (Theme::surface2);
    g.fillRect (header);

    g.setColour (Theme::cyan);
    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    g.drawText (t.title, header.reduced (12.0f, 0.0f), juce::Justification::centredLeft);

    // Der Untertitel steht nur da, wo er ohne Gedraenge hinpasst.
    auto subtitleArea = header.withTrimmedLeft (100.0f).withTrimmedRight (90.0f);
    if (subtitleArea.getWidth() > 220.0f)
    {
        g.setColour (Theme::textDim.withAlpha (0.75f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText (t.subtitle, subtitleArea, juce::Justification::centredLeft);
    }
}

void HoerplatzEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (34);

    auto buttons = header.removeFromRight (78).reduced (4, 6);
    helpButton.setBounds (buttons.removeFromRight (26));
    buttons.removeFromRight (4);
    langButton.setBounds (buttons.removeFromRight (34));

    // Der Grundriss braucht Platz, um etwas zu zeigen. Ist das Fenster zu
    // eng oder zu flach, faellt er weg - und mit ihm die Raummasse, die ohne
    // ihn nichts mehr bedeuten.
    const bool showPlan = getWidth() >= 620 && getHeight() >= 360;
    room.setVisible (showPlan);
    for (auto* r : { &roomWidth, &roomDepth })
    {
        r->label.setVisible (showPlan);
        r->slider.setVisible (showPlan);
    }
    areaLabel.setVisible (showPlan);

    auto side = area;
    if (showPlan)
    {
        side = area.removeFromRight (juce::jmin (300, area.getWidth() / 2));
        room.setBounds (area.reduced (6));
    }
    side = side.reduced (10, 8);

    // Aus der Hoehe folgt, wieviel hineinpasst. Zuerst wird das Zahlenfeld
    // aufgegeben, dann ruecken Beschriftung und Regler in eine Zeile
    // zusammen. Was bleibt, ist das, womit man einstellt.
    constexpr int bypassHeight = 78;
    constexpr int testHeight = 26;
    const int rowCount = 3 + (showPlan ? 2 : 0);
    const int extras = showPlan ? 24 : 0;              // Flaechenanzeige

    const auto needed = [&] (int rowHeight, bool withReadout)
    {
        return rowCount * rowHeight + extras + 4 + bypassHeight
             + 12 + testHeight + (withReadout ? 78 : 0);
    };

    int rowHeight = 43;
    bool showReadout = true;
    if (needed (rowHeight, true) > side.getHeight())
    {
        showReadout = false;
        if (needed (rowHeight, false) > side.getHeight())
        {
            rowHeight = 25;
            showReadout = needed (rowHeight, true) <= side.getHeight();
        }
    }
    const bool tightRows = rowHeight < 43;
    readout.setVisible (showReadout);

    auto place = [&] (Row& r)
    {
        auto line = side.removeFromTop (tightRows ? 22 : 37);
        if (tightRows)
        {
            r.label.setBounds (line.removeFromLeft (juce::jmin (92, line.getWidth() / 2)));
            r.slider.setBounds (line);
        }
        else
        {
            r.label.setBounds (line.removeFromTop (15));
            r.slider.setBounds (line);
        }
        side.removeFromTop (tightRows ? 3 : 6);
    };

    place (speakerDistance);
    if (showPlan)
    {
        place (roomWidth);
        place (roomDepth);
        areaLabel.setBounds (side.removeFromTop (18));
        side.removeFromTop (6);
    }
    place (listenerX);
    place (listenerY);

    side.removeFromTop (4);
    auto bypassArea = side.removeFromTop (juce::jmin (bypassHeight, side.getHeight()));
    auto knobArea = bypassArea.removeFromRight (78);
    gainAmountLabel.setBounds (knobArea.removeFromTop (16));
    gainAmountKnob.setBounds (knobArea);

    // Die beiden Schalter stehen mittig zum Knopf daneben.
    auto switches = bypassArea.withSizeKeepingCentre (bypassArea.getWidth(), 44);
    bypassDelay.setBounds (switches.removeFromTop (22));
    bypassGain.setBounds (switches.removeFromTop (22));

    // Das Testgeraeusch steht ueber dem Zahlenfeld: man greift danach,
    // waehrend man einstellt, und liest darunter ab, was dabei herauskommt.
    side.removeFromTop (12);
    testToneButton.setBounds (side.removeFromTop (juce::jmin (testHeight, side.getHeight())));

    if (showReadout)
    {
        side.removeFromTop (10);
        readout.setBounds (side.removeFromTop (juce::jmin (68, side.getHeight())));
    }
}

void HoerplatzEditor::timerCallback()
{
    // Ein geladenes Preset kann Sprache und Hilfe mitbringen.
    const auto storedLang = (int) plugin.apvts.state.getProperty (Params::language, 0) == 1 ? Lang::en : Lang::de;
    if (storedLang != lang)
    {
        lang = storedLang;
        applyLanguage();
    }
    const bool storedHelp = (bool) plugin.apvts.state.getProperty (Params::showHelp, true);
    if (storedHelp != showHelp)
    {
        showHelp = storedHelp;
        helpButton.setToggleState (showHelp, juce::dontSendNotification);
        applyHelp();
    }

    // Solange der Ausklang laeuft, leuchtet der Knopf nach - man sieht, dass
    // das Geraeusch noch nicht ganz vorbei ist.
    const bool sounding = plugin.testTone.isSounding();
    if (sounding != tailWasSounding)
    {
        tailWasSounding = sounding;
        const bool fading = sounding && ! testToneButton.getToggleState();
        testToneButton.setColour (juce::TextButton::textColourOffId,
                                  fading ? Theme::amber.withAlpha (0.55f) : Theme::textDim);
        testToneButton.repaint();
    }

    updateArea();
    room.pollClipping();
    room.repaint();
    readout.repaint();
}
