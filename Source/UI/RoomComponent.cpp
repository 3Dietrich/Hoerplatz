#include "RoomComponent.h"
#include "HeadSymbol.h"
#include "Theme.h"
#include "../PluginProcessor.h"

namespace
{
    juce::String metreText (double m)
    {
        return juce::String (m, m < 10.0 ? 2 : 1) + " m";
    }

    void setParam (juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }
}

RoomComponent::RoomComponent (HoerplatzProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

RoomComponent::View RoomComponent::makeView() const
{
    const float W = processor.apvts.getRawParameterValue (Params::roomWidth)->load();
    const float D = processor.apvts.getRawParameterValue (Params::roomDepth)->load();

    auto area = getLocalBounds().toFloat().reduced (26.0f, 22.0f);

    View v;
    v.scale = juce::jmin (area.getWidth() / W, area.getHeight() / D);

    const float pw = W * v.scale;
    const float ph = D * v.scale;
    v.room = juce::Rectangle<float> (area.getCentreX() - pw * 0.5f,
                                     area.getCentreY() - ph * 0.5f, pw, ph);

    // Welt (0,0) liegt in der Mitte der Boxenebene, also frontGap unterhalb
    // der vorderen Wand.
    v.origin = { v.room.getCentreX(), v.room.getY() + frontGap * v.scale };
    return v;
}

juce::Point<float> RoomComponent::worldToScreen (const View& v, float wx, float wy) const
{
    return { v.origin.x + wx * v.scale, v.origin.y + wy * v.scale };
}

juce::Point<float> RoomComponent::screenToWorld (const View& v, juce::Point<float> p) const
{
    return { (p.x - v.origin.x) / v.scale, (p.y - v.origin.y) / v.scale };
}

void RoomComponent::drawSpeaker (juce::Graphics& g, juce::Point<float> pos,
                                 float angleToListener, float sizePx) const
{
    // Lokale Zeichnung: Schallrichtung entlang +y (nach unten), hinten
    // schmal, vorne breit - die Box von oben. Die Drehung richtet sie auf
    // den Hoerplatz aus.
    const float w = sizePx;
    const float d = sizePx * 0.72f;

    juce::Path box;
    box.startNewSubPath (-w * 0.32f, -d * 0.5f);
    box.lineTo          ( w * 0.32f, -d * 0.5f);
    box.lineTo          ( w * 0.50f,  d * 0.5f);
    box.lineTo          (-w * 0.50f,  d * 0.5f);
    box.closeSubPath();

    // Bildschirmwinkel: 0 Grad Drehung heisst "strahlt nach unten in den
    // Raum", positive Werte drehen im Uhrzeigersinn.
    auto t = juce::AffineTransform::rotation (angleToListener).translated (pos);

    g.setColour (Theme::amber.withAlpha (0.16f));
    g.fillPath (box, t);
    g.setColour (Theme::amber.withAlpha (0.85f));
    g.strokePath (box, juce::PathStrokeType (1.4f), t);

    // Chassis als kleiner Kreis auf der Schallwand.
    juce::Path chassis;
    chassis.addEllipse (-w * 0.16f, d * 0.16f, w * 0.32f, w * 0.32f);
    g.setColour (Theme::amber.withAlpha (0.55f));
    g.strokePath (chassis, juce::PathStrokeType (1.0f), t);
}

void RoomComponent::paint (juce::Graphics& g)
{
    const auto v = makeView();
    const float spk = processor.apvts.getRawParameterValue (Params::speakerDistance)->load();
    const float px  = processor.apvts.getRawParameterValue (Params::listenerX)->load();
    const float py  = processor.apvts.getRawParameterValue (Params::listenerY)->load();
    const auto  a   = processor.currentAlignment();

    g.fillAll (Theme::ground);

    // Raum
    g.setColour (Theme::surface);
    g.fillRoundedRectangle (v.room, Theme::corner);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (v.room, Theme::corner, 1.0f);

    const auto lPos = worldToScreen (v, -0.5f * spk, 0.0f);
    const auto rPos = worldToScreen (v,  0.5f * spk, 0.0f);
    const auto hPos = worldToScreen (v, px, py);

    // Mittelachse zwischen den Boxen, gestrichelt und sehr zurueckhaltend.
    {
        const float dashes[] = { 3.0f, 5.0f };
        const juce::Line<float> axis { { v.origin.x, v.origin.y },
                                      { v.origin.x, v.room.getBottom() - 4.0f } };
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawDashedLine (axis, dashes, 2, 1.0f);
    }

    // Massband fuer den Boxenabstand oberhalb der Boxen.
    {
        const float y = v.room.getY() + 8.0f;
        g.setColour (Theme::line);
        g.drawLine (lPos.x, y, rPos.x, y, 1.0f);
        g.drawLine (lPos.x, y - 3.0f, lPos.x, y + 3.0f, 1.0f);
        g.drawLine (rPos.x, y - 3.0f, rPos.x, y + 3.0f, 1.0f);

        g.setColour (Theme::textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (metreText (spk), juce::Rectangle<float> (lPos.x, y - 15.0f, rPos.x - lPos.x, 13.0f),
                    juce::Justification::centred);
    }

    // Wege vom Hoerplatz zu den Boxen, mit ihrer Laenge beschriftet. Der
    // laengere Weg ist der, an dem sich alles ausrichtet - er bekommt die
    // kraeftigere Linie.
    const bool leftIsFar = a.distL >= a.distR;
    for (int side = 0; side < 2; ++side)
    {
        const auto from = (side == 0 ? lPos : rPos);
        const bool isFar = (side == 0 ? leftIsFar : ! leftIsFar);
        const double dist = (side == 0 ? a.distL : a.distR);

        g.setColour (juce::Colours::white.withAlpha (isFar ? 0.30f : 0.14f));
        g.drawLine ({ from, hPos }, isFar ? 1.4f : 1.0f);

        const auto mid = from + (hPos - from) * 0.55f;
        g.setColour (Theme::textDim.withAlpha (isFar ? 0.95f : 0.6f));
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
        g.drawText (metreText (dist), juce::Rectangle<float> (mid.x - 34.0f, mid.y - 14.0f, 68.0f, 13.0f),
                    juce::Justification::centred);
    }

    // Boxen, jeweils auf den Hoerplatz ausgerichtet.
    const float spkSize = juce::jlimit (13.0f, 40.0f, 0.30f * v.scale);
    drawSpeaker (g, lPos, std::atan2 (- (hPos.x - lPos.x), hPos.y - lPos.y), spkSize);
    drawSpeaker (g, rPos, std::atan2 (- (hPos.x - rPos.x), hPos.y - rPos.y), spkSize);

    // Hoerplatz: Fadenkreuz plus Kopf von oben in der besten Mittenstellung.
    const float headR = juce::jlimit (11.0f, 26.0f, 0.20f * v.scale);
    {
        g.setColour (Theme::cyan.withAlpha (0.55f));
        const float arm = headR * 2.1f;
        g.drawLine (hPos.x - arm, hPos.y, hPos.x + arm, hPos.y, 1.0f);
        g.drawLine (hPos.x, hPos.y - arm, hPos.x, hPos.y + arm, 1.0f);

        HeadSymbol::Style style;
        style.headColour = Theme::cyan;
        style.earColour  = Theme::cyan.withAlpha (0.8f);
        style.fillColour = Theme::cyan.withAlpha (0.10f);
        style.lineThickness = 1.5f;

        // Weltwinkel: 0 = geradeaus zur Boxenebene, positiv nach rechts.
        // Auf dem Bildschirm zeigt "geradeaus" nach oben, also -90 Grad.
        const float screenAngle = juce::degreesToRadians ((float) a.headAngleDeg - 90.0f);
        HeadSymbol::draw (g, hPos, headR, screenAngle, style);
    }
}

void RoomComponent::setListenerFromMouse (juce::Point<float> screenPos)
{
    const auto v = makeView();
    auto w = screenToWorld (v, screenPos);

    const float W = processor.apvts.getRawParameterValue (Params::roomWidth)->load();
    const float D = processor.apvts.getRawParameterValue (Params::roomDepth)->load();

    // Innerhalb des Raumes bleiben, und vor der Boxenebene: hinter den Boxen
    // gibt es keinen Hoerplatz, den diese Rechnung noch sinnvoll bedient.
    const float x = juce::jlimit (-0.5f * W + 0.10f, 0.5f * W - 0.10f, w.x);
    const float y = juce::jlimit (0.10f, D - frontGap - 0.10f, w.y);

    setParam (processor.apvts, Params::listenerX, x);
    setParam (processor.apvts, Params::listenerY, y);
}

void RoomComponent::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    if (auto* p = processor.apvts.getParameter (Params::listenerX)) p->beginChangeGesture();
    if (auto* p = processor.apvts.getParameter (Params::listenerY)) p->beginChangeGesture();
    setListenerFromMouse (e.position);
    repaint();
}

void RoomComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;
    setListenerFromMouse (e.position);
    repaint();
}

void RoomComponent::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;
    dragging = false;
    if (auto* p = processor.apvts.getParameter (Params::listenerX)) p->endChangeGesture();
    if (auto* p = processor.apvts.getParameter (Params::listenerY)) p->endChangeGesture();
}
