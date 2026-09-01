// Schreibt das Testgeraeusch samt Ausklang in eine WAV-Datei - zum Anhoeren,
// ohne dafuer einen Host zu starten. Das Gegenstueck zu ui_shot, nur fuers
// Ohr statt fuers Auge.
//
// Aufruf: tone_demo <datei.wav> [sekunden-an]
#include "../Source/TestTone.h"

#include <juce_audio_formats/juce_audio_formats.h>

int main (int argc, char** argv)
{
    const juce::String path = argc > 1 ? argv[1] : "tone_demo.wav";
    const double secondsOn = argc > 2 ? std::atof (argv[2]) : 3.0;
    constexpr double sr = 48000.0;
    constexpr int block = 512;

    TestTone tone;
    tone.prepare (sr);
    tone.setActive (true);

    juce::AudioBuffer<float> out (2, (int) (sr * (secondsOn + 5.0)));
    out.clear();

    std::vector<float> mix ((size_t) block, 0.0f);
    juce::AudioBuffer<float> chunk (2, block);

    int written = 0;
    while (written + block <= out.getNumSamples())
    {
        if (written >= (int) (secondsOn * sr))
            tone.setActive (false);

        chunk.clear();
        tone.render (chunk.getWritePointer (0), chunk.getWritePointer (1), block, mix.data());

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
                out.setSample (ch, written + i, chunk.getSample (ch, i) * mix[(size_t) i]);

        written += block;
    }

    juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (path));
    file.deleteFile();

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
    if (stream == nullptr)
    {
        std::puts ("konnte die Datei nicht anlegen");
        return 1;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (stream.release(), sr, 2, 24, {}, 0));
    if (writer == nullptr)
    {
        std::puts ("konnte den Schreiber nicht anlegen");
        return 1;
    }

    writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
    writer.reset();

    std::printf ("geschrieben: %s (%.1f s)\n", file.getFullPathName().toRawUTF8(),
                 out.getNumSamples() / sr);
    return 0;
}
