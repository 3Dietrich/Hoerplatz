#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Parameter-Kennungen und das Layout an einer Stelle. Die Bereiche sind
// bewusst weit gefasst: der genannte Einsatzbereich sind Raeume von 5 bis
// 90 m2, die Regler koennen aber deutlich darueber hinaus, damit sich
// niemand an einer erfundenen Obergrenze stoesst.
namespace Params
{
    inline constexpr const char* speakerDistance = "spkDist";
    inline constexpr const char* roomWidth       = "roomW";
    inline constexpr const char* roomDepth       = "roomD";
    inline constexpr const char* listenerX       = "posX";
    inline constexpr const char* listenerY       = "posY";
    inline constexpr const char* bypassDelay     = "byDelay";
    inline constexpr const char* bypassGain      = "byGain";

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
