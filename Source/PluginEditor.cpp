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

    auto body = area.reduced (10.0f, 8.0f);
    const float rowH = 18.0f;
    const float col1 = 74.0f;

    auto row = [&] (float y) { return juce::Rectangle<float> (body.getX(), y, body.getWidth(), rowH); };
    float y = body.getY();

    auto line = [&] (const juce::String& name, const juce::String& value, juce::Colour c)
    {
        g.setFont (monoFont (12.0f));
        g.setColour (Theme::textDim);
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

    line (t.correction, side, centred ? Theme::textDim : Theme::amber);
    line (t.delayRow, noDelay ? juce::String (t.off) : fixed (a.delaySeconds() * 1000.0, 2, "ms"),
          noDelay ? Theme::textDim.withAlpha (0.5f) : Theme::cyan);
    line (t.gainRow, noGain ? juce::String (t.off)
                            : fixed (juce::Decibels::gainToDecibels (a.gainRatio()), 1, "dB"),
          noGain ? Theme::textDim.withAlpha (0.5f) : Theme::cyan);
}

juce::Label* HoerplatzEditor::MonoLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (monoFont (12.0f));
    label->setJustificationType (juce::Justification::centredRight);
    label->setBorderSize ({ 1, 4, 1, 8 });
    return label;
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

    // Boxenabstand: kein eigener Parameter, sondern ein Griff an beide Boxen.
    styleSlider (speakerDistance.slider);
    speakerDistance.slider.setRange (0.30, 24.0, 0.01);
    speakerDistance.slider.textFromValueFunction = [] (double v) { return juce::String (v, 2) + " m"; };
    speakerDistance.slider.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue(); };
    speakerDistance.slider.onValueChange = [this]
    {
        if (speakerDistance.slider.isMouseButtonDown() || speakerDistance.slider.hasKeyboardFocus (false))
            setSpeakerDistance ((float) speakerDistance.slider.getValue());
    };
    speakerDistance.slider.onDragStart = [this]
    {
        for (auto* id : { Params::leftX, Params::leftY, Params::rightX, Params::rightY })
            if (auto* prm = plugin.apvts.getParameter (id)) prm->beginChangeGesture();
    };
    speakerDistance.slider.onDragEnd = [this]
    {
        for (auto* id : { Params::leftX, Params::leftY, Params::rightX, Params::rightY })
            if (auto* prm = plugin.apvts.getParameter (id)) prm->endChangeGesture();
    };
    addAndMakeVisible (speakerDistance.label);
    addAndMakeVisible (speakerDistance.slider);

    setUpRow (roomWidth, Params::roomWidth, {});
    setUpRow (roomDepth, Params::roomDepth, {});
    setUpRow (listenerX, Params::listenerX, {});
    setUpRow (listenerY, Params::listenerY, {});

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

    areaLabel.setFont (monoFont (12.0f));
    areaLabel.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (areaLabel);

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
    updateSpeakerDistanceSlider();

    setSize (940, 560);
    setResizable (true, true);
    setResizeLimits (760, 460, 1800, 1200);

    startTimerHz (30);
}

HoerplatzEditor::~HoerplatzEditor()
{
    setLookAndFeel (nullptr);
}

void HoerplatzEditor::styleSlider (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 18);
    s.setColour (juce::Slider::trackColourId, Theme::cyan.withAlpha (0.55f));
    s.setColour (juce::Slider::backgroundColourId, Theme::surface2);
    s.setColour (juce::Slider::thumbColourId, Theme::cyan);
    s.setColour (juce::Slider::textBoxTextColourId, Theme::text);
    s.setColour (juce::Slider::textBoxOutlineColourId, Theme::line);
    s.setColour (juce::Slider::textBoxBackgroundColourId, Theme::surface);
}

void HoerplatzEditor::setUpRow (Row& r, const char* paramId, const juce::String& tooltip)
{
    r.label.setFont (juce::Font (juce::FontOptions (12.0f)));
    r.label.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (r.label);

    styleSlider (r.slider);
    r.slider.setTooltip (tooltip);
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

void HoerplatzEditor::updateSpeakerDistanceSlider()
{
    if (speakerDistance.slider.isMouseButtonDown())
        return;

    const Geometry::Point l { plugin.apvts.getRawParameterValue (Params::leftX)->load(),
                              plugin.apvts.getRawParameterValue (Params::leftY)->load() };
    const Geometry::Point r { plugin.apvts.getRawParameterValue (Params::rightX)->load(),
                              plugin.apvts.getRawParameterValue (Params::rightY)->load() };

    speakerDistance.slider.setValue (Geometry::speakerDistance (l, r), juce::dontSendNotification);
}

void HoerplatzEditor::setSpeakerDistance (float metres)
{
    auto get = [this] (const char* id) { return plugin.apvts.getRawParameterValue (id)->load(); };
    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = plugin.apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };

    const juce::Point<float> l { get (Params::leftX),  get (Params::leftY) };
    const juce::Point<float> r { get (Params::rightX), get (Params::rightY) };

    const auto centre = (l + r) * 0.5f;
    auto direction = r - l;
    const float length = direction.getDistanceFromOrigin();

    // Stehen beide Boxen aufeinander, gibt es keine Richtung, an der sich
    // das Auseinanderziehen orientieren koennte - dann waagerecht.
    direction = (length > 0.001f ? direction / length : juce::Point<float> { 1.0f, 0.0f });

    const float W = get (Params::roomWidth);
    const float D = get (Params::roomDepth);
    auto clampToRoom = [&] (juce::Point<float> p)
    {
        return juce::Point<float> { juce::jlimit (-0.5f * W + 0.10f, 0.5f * W - 0.10f, p.x),
                                    juce::jlimit (0.10f, D - 0.10f, p.y) };
    };

    const auto newL = clampToRoom (centre - direction * (metres * 0.5f));
    const auto newR = clampToRoom (centre + direction * (metres * 0.5f));

    set (Params::leftX,  newL.x);
    set (Params::leftY,  newL.y);
    set (Params::rightX, newR.x);
    set (Params::rightY, newR.y);
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
    side.removeFromTop (10);

    readout.setBounds (side.removeFromTop (juce::jmin (70, side.getHeight())));
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

    updateArea();
    updateSpeakerDistanceSlider();
    room.repaint();
    readout.repaint();
}
