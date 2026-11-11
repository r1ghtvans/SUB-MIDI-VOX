// PEPEL VOX - ID параметров (X-макросы), ParameterLayout, фабричные пресеты.
#pragma once
#include "Dsp.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace pepel
{
namespace ids
{
#define X(id, label, mn, mx, def, step, skew, fmt) inline constexpr const char* id = #id;
    PEPEL_PARAMS (X)
#undef X
}

inline Fmt fmtOf (const juce::String& id)
{
#define X(pid, label, mn, mx, def, step, skew, fmt) if (id == #pid) return Fmt::fmt;
    PEPEL_PARAMS (X)
#undef X
    return Fmt::Pct;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
#define X(id, label, mn, mx, def, step, skew, fmt)                                                             \
    v.push_back (std::make_unique<juce::AudioParameterFloat> (                                                  \
        juce::ParameterID { #id, 1 }, label,                                                                    \
        juce::NormalisableRange<float> ((float) (mn), (float) (mx), (float) (step), (float) (skew)),            \
        (float) (def),                                                                                          \
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float val, int)                  \
            { return juce::String (formatValue (Fmt::fmt, val)); })));
    PEPEL_PARAMS (X)
#undef X
    return { v.begin(), v.end() };
}

// Применить пресет к дереву параметров (message thread)
inline void applyPreset (juce::AudioProcessorValueTreeState& apvts, int index)
{
    const auto& all = presets();
    if (index < 0 || index >= (int) all.size()) return;
#define X(id, label, mn, mx, def, step, skew, fmt)                                                             \
    if (auto* prm = apvts.getParameter (#id)) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (def)));
    PEPEL_PARAMS (X)
#undef X
    for (auto& e : all[(size_t) index].e)
        if (auto* prm = apvts.getParameter (e.id)) prm->setValueNotifyingHost (prm->convertTo0to1 (e.v));
}
} // namespace pepel
