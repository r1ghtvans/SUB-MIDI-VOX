// PEPEL VOX - список параметров (X-макросы), форматирование и фабричные пресеты.
// Чистый C++ без JUCE: используется и в DSP, и в тестах, и в плагине.
#pragma once
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace pepel
{
enum class Fmt { Pct, Ct, St, Ms, Hz2, Hz0, Db, Note, Scale, Mode, Bits, Div, Oct, Quant, Bpm, OnOff };

// X(id, "Название", min, max, default, step, skew, Fmt)
#define PEPEL_PARAMS(X)                                                        \
    /* AUTO-TUNE */                                                            \
    X(atKey,      "Key",      0,    11,   0,    1,    1.0f, Note)              \
    X(atScale,    "Scale",    0,    5,    0,    1,    1.0f, Scale)             \
    X(atAmount,   "Amount",   0,    100,  0,    1,    1.0f, Pct)               \
    X(atSpeed,    "Speed",    0,    200,  30,   1,    1.0f, Ms)                \
    X(atDetune,   "Detune",   -100, 100,  0,    0.1f, 1.0f, Ct)                \
    /* PITCH + HARMONY */                                                      \
    X(pitShift,   "Pitch",    -12,  12,   0,    1,    1.0f, St)                \
    X(pitHarm,    "Harmony",  0,    100,  0,    1,    1.0f, Pct)               \
    X(pitInt1,    "Voice 2",  -12,  12,   4,    1,    1.0f, St)                \
    X(pitInt2,    "Voice 3",  -12,  12,   7,    1,    1.0f, St)                \
    X(pitVoc,     "Vocoder",  0,    100,  0,    1,    1.0f, Pct)               \
    /* GRANULAR */                                                             \
    X(grSize,     "Grain",    20,   200,  80,   1,    1.0f, Ms)                \
    X(grDens,     "Density",  0,    100,  50,   1,    1.0f, Pct)               \
    X(grScat,     "Scatter",  0,    100,  30,   1,    1.0f, Pct)               \
    X(grMix,      "Mix",      0,    100,  0,    1,    1.0f, Pct)               \
    /* BITCRUSHER */                                                           \
    X(bcBits,     "Bits",     4,    16,   16,   1,    1.0f, Bits)              \
    X(bcRate,     "Rate",     1,    32,   1,    1,    1.0f, Div)               \
    X(bcMix,      "Mix",      0,    100,  0,    1,    1.0f, Pct)               \
    /* PHASER */                                                               \
    X(phRate,     "Rate",     0.05f, 5,   0.4f, 0.01f, 0.5f, Hz2)              \
    X(phDepth,    "Depth",    0,    100,  50,   1,    1.0f, Pct)               \
    X(phMix,      "Mix",      0,    100,  0,    1,    1.0f, Pct)               \
    /* FLANGER */                                                              \
    X(flRate,     "Rate",     0.05f, 5,   0.25f, 0.01f, 0.5f, Hz2)             \
    X(flDepth,    "Depth",    0,    100,  50,   1,    1.0f, Pct)               \
    X(flMix,      "Mix",      0,    100,  0,    1,    1.0f, Pct)               \
    /* MASTER */                                                               \
    X(inGain,     "Input",    -24,  24,   0,    0.1f, 1.0f, Db)                \
    X(outGain,    "Output",   -24,  12,   0,    0.1f, 1.0f, Db)                \
    X(dryWet,     "Dry/Wet",  0,    100,  100,  1,    1.0f, Pct)               \
    /* DISTORTION */                                                           \
    X(diDrive,    "Drive",    0,    100,  30,   1,    1.0f, Pct)               \
    X(diTone,     "Tone",     0,    100,  60,   1,    1.0f, Pct)               \
    X(diMix,      "Mix",      0,    100,  0,    1,    1.0f, Pct)               \
    /* EXCITER */                                                              \
    X(exAmt,      "Amount",   0,    100,  0,    1,    1.0f, Pct)               \
    X(exFreq,     "Freq",     1500, 9000, 4000, 10,   0.6f, Hz0)               \
    /* TREMOLO */                                                              \
    X(trRate,     "Rate",     0.5f, 15,   5,    0.01f, 0.6f, Hz2)              \
    X(trDepth,    "Depth",    0,    100,  0,    1,    1.0f, Pct)               \
    /* SUBVOX */                                                               \
    X(sbMode,     "Mode",     0,    5,    0,    1,    1.0f, Mode)              \
    X(sbOct,      "Octave",   -3,   0,    -1,   1,    1.0f, Oct)               \
    X(sbLevel,    "Sub",      0,    100,  0,    1,    1.0f, Pct)               \
    X(sbVoice,    "Voice",    0,    100,  100,  1,    1.0f, Pct)               \
    X(sbLock,     "Syn-Lock", 0,    100,  0,    1,    1.0f, Pct)               \
    X(sbGlide,    "Glide",    0,    300,  40,   1,    1.0f, Ms)                \
    X(sbDrive,    "Drive",    0,    100,  20,   1,    1.0f, Pct)               \
    X(sbPunch,    "Punch",    0,    100,  30,   1,    1.0f, Pct)               \
    /* SUB FILTER */                                                           \
    X(sbCut,      "Cutoff",   40,   8000, 1200, 1,    0.4f, Hz0)               \
    X(sbReso,     "Reso",     0,    100,  20,   1,    1.0f, Pct)               \
    X(sbFollow,   "Follow",   0,    100,  40,   1,    1.0f, Pct)               \
    X(sbGate,     "Gate",     -60,  -10,  -42,  1,    1.0f, Db)                \
    /* WOBBLE */                                                               \
    X(sbWobRate,  "Rate",     0.1f, 12,   2,    0.01f, 0.5f, Hz2)              \
    X(sbWobDepth, "Depth",    0,    100,  0,    1,    1.0f, Pct)               \
    X(sbDetune,   "Detune",   0,    50,   12,   0.1f, 1.0f, Ct)                \
    /* MIDI */                                                                 \
    X(mdGrid,     "Grid",     0,    4,    3,    1,    1.0f, Quant)             \
    X(mdBpm,      "Tempo",    60,   200,  140,  1,    1.0f, Bpm)               \
    X(mdTransp,   "Transp",   -36,  36,   0,    1,    1.0f, St)                \
    X(mdMinLen,   "Min Len",  20,   300,  60,   1,    1.0f, Ms)                \
    X(mdOut,      "MIDI Out", 0,    1,    0,    1,    1.0f, OnOff)

inline const char* noteName(int n)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((n % 12) + 12) % 12];
}
inline const char* scaleName(int s)
{
    static const char* n[6] = { "CHROM", "MAJOR", "MINOR", "PENT+", "PENT-", "BLUES" };
    return n[s < 0 ? 0 : (s > 5 ? 5 : s)];
}
inline const char* modeName(int m)
{
    static const char* n[6] = { "SINE", "808", "REESE", "GROWL", "8-BIT", "TALK" };
    return n[m < 0 ? 0 : (m > 5 ? 5 : m)];
}
inline const char* quantName(int q)
{
    static const char* n[5] = { "OFF", "1/4", "1/8", "1/16", "1/32" };
    return n[q < 0 ? 0 : (q > 4 ? 4 : q)];
}

inline std::string formatValue(Fmt f, float v)
{
    char b[48];
    int i = (int) std::lround(v);
    switch (f)
    {
        case Fmt::Pct:   std::snprintf(b, sizeof b, "%d%%", i); break;
        case Fmt::Ct:    std::snprintf(b, sizeof b, "%.1f ct", (double) v); break;
        case Fmt::St:    std::snprintf(b, sizeof b, "%+d st", i); break;
        case Fmt::Ms:    std::snprintf(b, sizeof b, "%d ms", i); break;
        case Fmt::Hz2:   std::snprintf(b, sizeof b, "%.2f Hz", (double) v); break;
        case Fmt::Hz0:   std::snprintf(b, sizeof b, "%d Hz", i); break;
        case Fmt::Db:    std::snprintf(b, sizeof b, "%+.1f dB", (double) v); break;
        case Fmt::Note:  std::snprintf(b, sizeof b, "%s", noteName(i)); break;
        case Fmt::Scale: std::snprintf(b, sizeof b, "%s", scaleName(i)); break;
        case Fmt::Mode:  std::snprintf(b, sizeof b, "%s", modeName(i)); break;
        case Fmt::Bits:  std::snprintf(b, sizeof b, "%d bit", i); break;
        case Fmt::Div:   std::snprintf(b, sizeof b, "1/%d", i); break;
        case Fmt::Oct:   std::snprintf(b, sizeof b, "%+d oct", i); break;
        case Fmt::Quant: std::snprintf(b, sizeof b, "%s", quantName(i)); break;
        case Fmt::Bpm:   std::snprintf(b, sizeof b, "%d bpm", i); break;
        case Fmt::OnOff: std::snprintf(b, sizeof b, "%s", i ? "ON" : "OFF"); break;
    }
    return b;
}

// ---------------------------------------------------------------- пресеты
struct PresetEntry { const char* id; float v; };
struct Preset { std::string name; std::vector<PresetEntry> e; };

inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> p = {
        { "CLEAR SKY", {} },
        { "SOFT TUNE", { { "atAmount", 60 }, { "atSpeed", 70 }, { "atScale", 1 } } },
        { "ROBOT SNAP", { { "atAmount", 100 }, { "atSpeed", 0 }, { "pitVoc", 45 }, { "diMix", 20 } } },
        { "ASH CHOIR", { { "atAmount", 80 }, { "atScale", 1 }, { "pitHarm", 65 }, { "pitInt1", 4 }, { "pitInt2", 7 },
                         { "flMix", 25 } } },
        { "GLITCH RAIN", { { "grMix", 55 }, { "grScat", 70 }, { "bcBits", 8 }, { "bcRate", 4 }, { "bcMix", 40 } } },
        { "VHS DREAM", { { "phMix", 55 }, { "flMix", 45 }, { "trDepth", 35 }, { "exAmt", 30 }, { "dryWet", 85 } } },
        { "BLOOD STORM", { { "atAmount", 100 }, { "atSpeed", 0 }, { "grMix", 50 }, { "bcBits", 6 }, { "bcRate", 6 },
                           { "bcMix", 65 }, { "diDrive", 80 }, { "diMix", 70 }, { "exAmt", 60 }, { "phMix", 50 },
                           { "flMix", 50 }, { "trDepth", 40 }, { "pitVoc", 40 } } },
        { "SUB SINE", { { "sbMode", 0 }, { "sbOct", -2 }, { "sbLevel", 90 }, { "sbVoice", 25 }, { "sbDrive", 10 } } },
        { "SUB 808", { { "sbMode", 1 }, { "sbOct", -2 }, { "sbLevel", 90 }, { "sbVoice", 20 }, { "sbGlide", 90 },
                       { "sbPunch", 70 }, { "sbDrive", 35 } } },
        { "REESE VOX", { { "sbMode", 2 }, { "sbOct", -2 }, { "sbLevel", 85 }, { "sbVoice", 20 }, { "sbCut", 700 },
                         { "sbReso", 35 }, { "sbWobDepth", 45 }, { "sbWobRate", 1.5f }, { "sbDetune", 18 } } },
        { "GROWL VOX", { { "sbMode", 3 }, { "sbOct", -1 }, { "sbLevel", 80 }, { "sbVoice", 20 }, { "sbFollow", 85 },
                         { "sbWobDepth", 30 }, { "sbDrive", 45 } } },
        { "8BIT SUB", { { "sbMode", 4 }, { "sbOct", -2 }, { "sbLevel", 80 }, { "sbVoice", 0 }, { "sbCut", 2500 },
                        { "sbLock", 100 }, { "atScale", 1 } } },
        { "TALK BASS", { { "sbMode", 5 }, { "sbOct", -1 }, { "sbLevel", 100 }, { "sbVoice", 15 }, { "sbCut", 3000 },
                         { "sbDrive", 30 } } },
    };
    return p;
}
} // namespace pepel
