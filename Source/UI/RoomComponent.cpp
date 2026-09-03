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

    void gesture (juce::AudioProcessorValueTreeState& apvts, const char* id, bool begin)
    {
        if (auto* p = apvts.getParameter (id))
            begin ? p->beginChangeGesture() : p->endChangeGesture();
    }

    // Radius um einen Punkt, innerhalb dessen er sich anfassen laesst.
    constexpr float grabRadius = 20.0f;
}

RoomComponent::RoomComponent (HoerplatzProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

Geometry::Point RoomComponent::paramPoint (const char* idX, const char* idY) const
{
    return { processor.apvts.getRawParameterValue (idX)->load(),
             processor.apvts.getRawParameterValue (idY)->load() };
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

    // Welt (0,0) liegt in der Mitte der vorderen Wand.
    v.origin = { v.room.getCentreX(), v.room.getY() };
    return v;
}

juce::Point<float> RoomComponent::worldToScreen (const View& v, float wx, float wy) const
{
    return { v.origin.x + wx * v.scale, v.origin.y + wy * v.scale };
}

juce::Point<float> RoomComponent::worldToScreen (const View& v, Geometry::Point p) const
{
    return worldToScreen (v, (float) p.x, (float) p.y);
}

juce::Point<float> RoomComponent::screenToWorld (const View& v, juce::Point<float> p) const
{
    return { (p.x - v.origin.x) / v.scale, (p.y - v.origin.y) / v.scale };
}

RoomComponent::Handle RoomComponent::handleAt (juce::Point<float> screenPos) const
{
    const auto v = makeView();

    const std::pair<Handle, juce::Point<float>> points[]
    {
        { Handle::listener,     worldToScreen (v, paramPoint (Params::listenerX, Params::listenerY)) },
        { Handle::leftSpeaker,  worldToScreen (v, paramPoint (Params::leftX,     Params::leftY)) },
        { Handle::rightSpeaker, worldToScreen (v, paramPoint (Params::rightX,    Params::rightY)) }
    };

    Handle best = Handle::none;
    float bestDistance = grabRadius;
    for (const auto& [handle, pos] : points)
    {
        const float d = pos.getDistanceFrom (screenPos);
        if (d < bestDistance)
        {
            bestDistance = d;
            best = handle;
        }
    }
    return best;
}

void RoomComponent::pollClipping()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    for (int ch = 0; ch < 2; ++ch)
        if (processor.clipped[ch].exchange (false))
            clipTime[ch] = now;
}

void RoomComponent::drawSpeaker (juce::Graphics& g, juce::Point<float> pos,
                                 float angleToListener, float sizePx, bool highlighted,
                                 float clipGlow) const
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

    auto t = juce::AffineTransform::rotation (angleToListener).translated (pos);

    // Uebersteuert diese Seite, faerbt sich die Box und klingt langsam
    // zurueck - so sieht man auch den kurzen Ausschlag, den man sonst
    // verpasst.
    const auto colour = Theme::amber.interpolatedWith (Theme::pink, clipGlow);

    g.setColour (colour.withAlpha (juce::jmax (highlighted ? 0.30f : 0.16f, clipGlow * 0.45f)));
    g.fillPath (box, t);
    g.setColour (colour.withAlpha (juce::jmax (highlighted ? 1.0f : 0.85f, clipGlow)));
    g.strokePath (box, juce::PathStrokeType (highlighted || clipGlow > 0.05f ? 1.8f : 1.4f), t);

    // Chassis als kleiner Kreis auf der Schallwand.
    juce::Path chassis;
    chassis.addEllipse (-w * 0.16f, d * 0.16f, w * 0.32f, w * 0.32f);
    g.setColour (colour.withAlpha (0.55f));
    g.strokePath (chassis, juce::PathStrokeType (1.0f), t);
}

void RoomComponent::paint (juce::Graphics& g)
{
    const auto v = makeView();
    const auto left     = paramPoint (Params::leftX,     Params::leftY);
    const auto right    = paramPoint (Params::rightX,    Params::rightY);
    const auto listener = paramPoint (Params::listenerX, Params::listenerY);
    const auto a = Geometry::compute (left, right, listener);

    g.fillAll (Theme::ground);

    // Raum
    g.setColour (Theme::surface);
    g.fillRoundedRectangle (v.room, Theme::corner);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (v.room, Theme::corner, 1.0f);

    const auto lPos = worldToScreen (v, left);
    const auto rPos = worldToScreen (v, right);
    const auto hPos = worldToScreen (v, listener);

    // Mittelsenkrechte der Boxenverbindung: auf ihr steht der Hoerplatz
    // symmetrisch zu beiden Boxen. Zurueckhaltend gestrichelt, sie ist eine
    // Orientierung, keine Vorschrift.
    {
        const juce::Point<float> mid { (lPos.x + rPos.x) * 0.5f, (lPos.y + rPos.y) * 0.5f };
        auto dir = rPos - lPos;
        const float len = dir.getDistanceFromOrigin();
        if (len > 1.0f)
        {
            const juce::Point<float> normal { -dir.y / len, dir.x / len };
            const float reach = v.room.getWidth() + v.room.getHeight();
            const juce::Line<float> axis { mid, mid + normal * (normal.y >= 0.0f ? reach : -reach) };

            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (v.room.toNearestInt());
            const float dashes[] = { 3.0f, 5.0f };
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.drawDashedLine (axis, dashes, 2, 1.0f);
        }
    }

    // Massband zwischen den Boxen.
    {
        const float dashes[] = { 2.0f, 4.0f };
        g.setColour (Theme::line);
        g.drawDashedLine ({ lPos, rPos }, dashes, 2, 1.0f);

        const juce::Point<float> mid { (lPos.x + rPos.x) * 0.5f, (lPos.y + rPos.y) * 0.5f };
        g.setColour (Theme::textDim);
        g.setFont (juce::FontOptions (Theme::smallText));
        g.drawText (metreText (Geometry::speakerDistance (left, right)),
                    juce::Rectangle<float> (mid.x - 44.0f, mid.y - 18.0f, 88.0f, 16.0f),
                    juce::Justification::centred);
    }

    // Wege vom Hoerplatz zu den Boxen, mit ihrer Laenge beschriftet. Beide
    // gleich hell: es sind zwei Wege zu zwei Ohren, keiner ist wichtiger als
    // der andere. Welcher der laengere ist, sagen die Zahlen.
    //
    // Im kleinen Grundriss bleiben die Linien und die Zahlen gehen: sie
    // ruecken dort so dicht zusammen, dass sie uebereinander stehen. Der
    // Grundriss zeigt dann die Lage, die Zahlen stehen bei den Reglern.
    const bool showLengths = v.room.getWidth() >= 130.0f && v.room.getHeight() >= 110.0f;

    for (int side = 0; side < 2; ++side)
    {
        const auto from = (side == 0 ? lPos : rPos);
        const double dist = (side == 0 ? a.distL : a.distR);

        g.setColour (juce::Colours::white.withAlpha (0.24f));
        g.drawLine ({ from, hPos }, 1.2f);

        if (! showLengths)
            continue;

        const auto mid = from + (hPos - from) * 0.55f;
        g.setColour (Theme::textDim.withAlpha (0.85f));
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), Theme::smallText, juce::Font::plain));
        g.drawText (metreText (dist), juce::Rectangle<float> (mid.x - 38.0f, mid.y - 16.0f, 76.0f, 16.0f),
                    juce::Justification::centred);
    }

    // Boxen und Kopf stehen im Massstab des Raumes: eine Box ist 30 cm
    // breit, ein Kopf 17,5 cm im Durchmesser. Nach unten sind sie begrenzt,
    // damit sie in einem grossen Raum sichtbar und greifbar bleiben; nach
    // oben nicht, sonst waeren die Groessenverhaeltnisse im kleinen Raum
    // falsch.
    const float spkSize = std::max (10.0f, 0.30f * v.scale);
    const auto activeHandle = (grabbed != Handle::none ? grabbed : hovered);

    // Nachleuchten der Uebersteuerung: eine Sekunde lang, dann ist es weg.
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto glow = [&] (int channel)
    {
        return (float) juce::jlimit (0.0, 1.0, 1.0 - (now - clipTime[channel]) / 1.0);
    };

    drawSpeaker (g, lPos, std::atan2 (- (hPos.x - lPos.x), hPos.y - lPos.y), spkSize,
                 activeHandle == Handle::leftSpeaker, glow (0));
    drawSpeaker (g, rPos, std::atan2 (- (hPos.x - rPos.x), hPos.y - rPos.y), spkSize,
                 activeHandle == Handle::rightSpeaker, glow (1));

    // L und R stehen an der Box, und zwar der Kanal, den sie traegt. Sitzt
    // man hinter der Aufstellung, steht die linke Box vom Platz aus rechts;
    // ist der Schalter an, gehen die Kanaele mit und die Buchstaben mit
    // ihnen. Ist er aus, bleiben sie stehen - dann sieht man, dass das Bild
    // spiegelverkehrt steht.
    {
        const auto& t = texts (lang);
        const bool follows = processor.apvts.getRawParameterValue (Params::followHead)->load() > 0.5f;
        const bool swapped = follows && a.mirrored;

        const auto letter = [&] (juce::Point<float> pos, bool leftSpeaker)
        {
            // Der Buchstabe steht hinter der Box, vom Hoerplatz aus gesehen -
            // dort ist Platz, die Wege zum Hoerplatz bleiben frei.
            auto away = pos - hPos;
            const float len = juce::jmax (1.0f, away.getDistanceFromOrigin());
            const auto at = pos + away * ((spkSize * 0.62f + 9.0f) / len);

            g.setColour (Theme::amber.withAlpha (0.85f));
            g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                          Theme::smallText + 1.0f, juce::Font::bold));
            g.drawText (leftSpeaker != swapped ? t.speakerL : t.speakerR,
                        juce::Rectangle<float> (at.x - 9.0f, at.y - 8.0f, 18.0f, 16.0f),
                        juce::Justification::centred);
        };

        letter (lPos, true);
        letter (rPos, false);
    }

    // Hoerplatz: Fadenkreuz plus Kopf von oben in der besten Mittenstellung.
    const float headR = std::max (6.0f, 0.0875f * v.scale);
    {
        const bool active = (activeHandle == Handle::listener);
        g.setColour (Theme::cyan.withAlpha (active ? 0.85f : 0.55f));
        const float arm = headR * 2.1f;
        g.drawLine (hPos.x - arm, hPos.y, hPos.x + arm, hPos.y, 1.0f);
        g.drawLine (hPos.x, hPos.y - arm, hPos.x, hPos.y + arm, 1.0f);

        HeadSymbol::Style style;
        style.headColour = Theme::cyan;
        style.earColour  = Theme::cyan.withAlpha (0.8f);
        style.fillColour = Theme::cyan.withAlpha (active ? 0.20f : 0.10f);
        style.lineThickness = active ? 2.0f : 1.5f;

        // Weltwinkel: 0 = geradeaus zur vorderen Wand, positiv nach rechts.
        // Auf dem Bildschirm zeigt "geradeaus" nach oben, also -90 Grad.
        const float screenAngle = juce::degreesToRadians ((float) a.headAngleDeg - 90.0f);
        HeadSymbol::draw (g, hPos, headR, screenAngle, style);
    }
}

void RoomComponent::dragTo (juce::Point<float> screenPos)
{
    const auto v = makeView();
    const auto w = screenToWorld (v, screenPos);

    const float W = processor.apvts.getRawParameterValue (Params::roomWidth)->load();
    const float D = processor.apvts.getRawParameterValue (Params::roomDepth)->load();

    // Innerhalb des Raumes bleiben.
    const float x = juce::jlimit (-0.5f * W + 0.10f, 0.5f * W - 0.10f, w.x);
    const float y = juce::jlimit (0.10f, D - 0.10f, w.y);

    switch (grabbed)
    {
        case Handle::leftSpeaker:
            setParam (processor.apvts, Params::leftX, x);
            setParam (processor.apvts, Params::leftY, y);
            break;
        case Handle::rightSpeaker:
            setParam (processor.apvts, Params::rightX, x);
            setParam (processor.apvts, Params::rightY, y);
            break;
        case Handle::listener:
            setParam (processor.apvts, Params::listenerX, x);
            setParam (processor.apvts, Params::listenerY, y);
            break;
        case Handle::none:
            break;
    }
}

void RoomComponent::mouseMove (const juce::MouseEvent& e)
{
    const auto h = handleAt (e.position);
    if (h != hovered)
    {
        hovered = h;
        repaint();
    }
}

void RoomComponent::mouseExit (const juce::MouseEvent&)
{
    if (hovered != Handle::none)
    {
        hovered = Handle::none;
        repaint();
    }
}

void RoomComponent::mouseDown (const juce::MouseEvent& e)
{
    // Wer neben alle Griffe klickt, setzt damit den Hoerplatz - der haeufigste
    // Handgriff soll ohne Zielen gehen.
    const auto h = handleAt (e.position);
    grabbed = (h == Handle::none ? Handle::listener : h);

    switch (grabbed)
    {
        case Handle::leftSpeaker:
            gesture (processor.apvts, Params::leftX, true);
            gesture (processor.apvts, Params::leftY, true);
            break;
        case Handle::rightSpeaker:
            gesture (processor.apvts, Params::rightX, true);
            gesture (processor.apvts, Params::rightY, true);
            break;
        case Handle::listener:
        case Handle::none:
            gesture (processor.apvts, Params::listenerX, true);
            gesture (processor.apvts, Params::listenerY, true);
            break;
    }

    dragTo (e.position);
    repaint();
}

void RoomComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (grabbed == Handle::none)
        return;
    dragTo (e.position);
    repaint();
}

void RoomComponent::mouseUp (const juce::MouseEvent& e)
{
    if (grabbed == Handle::none)
        return;

    switch (grabbed)
    {
        case Handle::leftSpeaker:
            gesture (processor.apvts, Params::leftX, false);
            gesture (processor.apvts, Params::leftY, false);
            break;
        case Handle::rightSpeaker:
            gesture (processor.apvts, Params::rightX, false);
            gesture (processor.apvts, Params::rightY, false);
            break;
        case Handle::listener:
        case Handle::none:
            gesture (processor.apvts, Params::listenerX, false);
            gesture (processor.apvts, Params::listenerY, false);
            break;
    }

    grabbed = Handle::none;
    hovered = handleAt (e.position);
    repaint();
}

juce::String RoomComponent::getTooltip()
{
    const auto& t = texts (lang);
    switch (hovered)
    {
        case Handle::leftSpeaker:
        case Handle::rightSpeaker: return t.helpSpeaker;
        case Handle::listener:     return t.helpListener;
        case Handle::none:         break;
    }
    return t.helpRoomPlan;
}
