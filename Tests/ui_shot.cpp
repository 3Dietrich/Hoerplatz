// Zeichnet die Oberflaeche einmal in ein Bild, ohne ein Fenster zu oeffnen.
// Damit laesst sich das Layout pruefen und ein Bild fuers README erzeugen,
// ohne dass auf dem Bildschirm etwas aufpoppt.
//
// Aufruf:  ui_shot <ausgabe.png> [name=wert ...]
// Namen:   w, h (Fenstergroesse), scale (Vielfaches fuer scharfe Bilder),
//          lx, ly / rx, ry (Standorte der Boxen), rw, rd (Raumbreite/-tiefe),
//          x, y (Hoerplatz), en=1 (englische Beschriftung)
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <string>

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI init;

    const juce::String path = argc > 1 ? argv[1] : "ui_shot.png";

    std::map<std::string, double> arg;
    for (int i = 2; i < argc; ++i)
    {
        const std::string s = argv[i];
        const auto eq = s.find ('=');
        if (eq != std::string::npos)
            arg[s.substr (0, eq)] = std::atof (s.substr (eq + 1).c_str());
    }
    const auto value = [&] (const char* name, double fallback)
    {
        const auto it = arg.find (name);
        return it == arg.end() ? fallback : it->second;
    };

    const int   w     = (int) value ("w", 940.0);
    const int   h     = (int) value ("h", 600.0);
    const float scale = (float) value ("scale", 1.0);

    HoerplatzProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto set = [&] (const char* id, const char* name)
    {
        const auto it = arg.find (name);
        if (it == arg.end())
            return;
        if (auto* p = processor.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) it->second));
    };
    set (Params::leftX,  "lx");
    set (Params::leftY,  "ly");
    set (Params::rightX, "rx");
    set (Params::rightY, "ry");
    set (Params::roomWidth,  "rw");
    set (Params::roomDepth,  "rd");
    set (Params::listenerX,  "x");
    set (Params::listenerY,  "y");

    if (value ("en", 0.0) > 0.5)
        processor.apvts.state.setProperty (Params::language, 1, nullptr);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (w, h);

    juce::Image image (juce::Image::ARGB, juce::roundToInt ((float) w * scale),
                       juce::roundToInt ((float) h * scale), true);
    {
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale (scale));
        editor->paintEntireComponent (g, true);
    }

    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (path));
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    if (! png.writeImageToStream (image, stream))
    {
        std::puts ("konnte das Bild nicht schreiben");
        return 1;
    }

    std::printf ("geschrieben: %s\n", out.getFullPathName().toRawUTF8());
    return 0;
}
