// PepelDspTest: проверка DSP без JUCE.
#include "../Source/Dsp.h"
#include <cstdio>
#include <cstdlib>
using namespace pepel;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("  FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } else { std::printf("  ok:   "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static const double SR = 48000.0;

static std::vector<float> voiceTone (double f, double sec, float amp = 0.5f)
{
    std::vector<float> v ((size_t) (sec * SR));
    for (size_t i = 0; i < v.size(); ++i)
    {
        double s = 0;
        for (int h = 1; h <= 8; ++h) s += std::sin (2 * M_PI * f * h * (double) i / SR + h) / h;
        v[i] = (float) (s * 0.5 * amp);
    }
    return v;
}

static std::vector<float> run (Engine& e, const std::vector<float>& in, std::vector<NoteEv>* evs = nullptr)
{
    std::vector<float> out (in.size());
    for (size_t pos = 0; pos < in.size(); pos += 256)
    {
        int n = (int) std::min<size_t> (256, in.size() - pos);
        e.process (in.data() + pos, nullptr, out.data() + pos, nullptr, n);
        if (evs) for (int k = 0; k < e.nEv; ++k) { NoteEv x = e.ev[k]; x.pos += (int) pos; evs->push_back (x); }
    }
    return out;
}

// оценка частоты по переходам через ноль (с ФНЧ для устойчивости)
static double estFreq (const std::vector<float>& v, size_t a, size_t b, bool lowpass = true)
{
    double lp = 0; int cross = 0; double prev = 0; size_t first = 0, last = 0;
    for (size_t i = a; i < b; ++i)
    {
        lp += (lowpass ? 0.05 : 1.0) * (v[i] - lp);
        if (prev < 0 && lp >= 0) { if (!cross) first = i; last = i; ++cross; }
        prev = lp;
    }
    return cross > 2 ? (cross - 1) * SR / (double) (last - first) : 0.0;
}
// независимая оценка: YIN по участку, медиана
static double yinFreq (const std::vector<float>& v, size_t a, size_t b)
{
    PitchDetector d; d.prepare (SR); std::vector<float> fs;
    for (size_t i = a; i < b; ++i) if (d.push (v[i]) && d.voiced) fs.push_back (d.f0);
    if (fs.empty()) return 0; std::sort (fs.begin(), fs.end()); return fs[fs.size() / 2];
}
static double rms (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += v[i] * v[i];
    return std::sqrt (s / (double) (b - a));
}
static bool sane (const std::vector<float>& v)
{
    for (float x : v) if (!std::isfinite (x) || std::fabs (x) > 1.0f) return false;
    return true;
}

int main()
{
    std::printf("[1] Тюнинг в ноту\n");
    {
        Engine e; e.prepare (SR); Params p; p.atAmount = 100; p.atSpeed = 0; p.atScale = 0; e.setParams (p);
        auto in = voiceTone (226.0, 2.0);
        auto out = run (e, in);
        const double f = estFreq (out, (size_t) (1.2 * SR), (size_t) (1.9 * SR));
        CHECK (std::fabs (f - 220.0) < 3.0, "226 Hz -> A3 220 Hz, измерено %.2f Hz", f);
        std::printf("      latency=%d samples\n", e.latency());
    }
    std::printf("[2] Высота сохраняется при силе 0\n");
    {
        Engine e; e.prepare (SR); Params p; p.atAmount = 0; e.setParams (p);
        auto in = voiceTone (226.0, 1.5);
        auto out = run (e, in);
        const double f = estFreq (out, (size_t) (0.8 * SR), (size_t) (1.4 * SR));
        CHECK (std::fabs (f - 226.0) < 2.0, "226 Hz остаётся 226 Hz, измерено %.2f Hz", f);
    }
    std::printf("[3] Уровень\n");
    {
        Engine e; e.prepare (SR); Params p; p.atAmount = 100; p.atSpeed = 0; e.setParams (p);
        auto in = voiceTone (226.0, 2.0);
        auto out = run (e, in);
        const double a = rms (in, (size_t) (1.2 * SR), (size_t) (1.9 * SR)), b = rms (out, (size_t) (1.2 * SR), (size_t) (1.9 * SR));
        const double db = 20 * std::log10 (b / a);
        CHECK (std::fabs (db) < 3.0, "уровень после тюнинга %.2f dB от входа", db);
    }
    std::printf("[4] Питч +12 st (октава)\n");
    {
        Engine e; e.prepare (SR); Params p; p.pitShift = 12; e.setParams (p);
        auto in = voiceTone (150.0, 2.0);
        auto out = run (e, in);
        const double f = yinFreq (out, (size_t) (1.2 * SR), (size_t) (1.9 * SR));
        CHECK (std::fabs (f - 300.0) < 6.0, "150 Hz +12 st -> 300 Hz, измерено %.1f Hz", f);
    }
    std::printf("[5] Нет NaN/inf и перегрузов на максимальных настройках\n");
    {
        Engine e; e.prepare (SR); Params p;
        p.atAmount = 100; p.atSpeed = 0; p.pitHarm = 100; p.pitVoc = 100; p.grMix = 100; p.bcMix = 100; p.bcBits = 4; p.bcRate = 32;
        p.diMix = 100; p.diDrive = 100; p.exAmt = 100; p.phMix = 100; p.flMix = 100; p.trDepth = 100; p.inGain = 24; p.outGain = 12;
        p.sbLevel = 100; p.sbDrive = 100; p.sbReso = 100; p.sbWobDepth = 100; p.sbVoice = 100;
        for (int mode = 0; mode < 6; ++mode)
        {
            p.sbMode = (float) mode; e.setParams (p);
            std::vector<float> in ((size_t) (1.5 * SR)); unsigned s = 1;
            for (size_t i = 0; i < in.size(); ++i) { s = s * 1664525u + 1013904223u; in[i] = ((s >> 8) / 8388608.0f - 1.0f) * 1.5f + (float) std::sin (i * 0.03) * 1.5f; }
            auto out = run (e, in);
            CHECK (sane (out), "режим SubVox %d: все выборки конечны и |x| <= 1", mode);
        }
        std::printf("      dirt=%.3f\n", e.dirt);
        CHECK (e.dirt > 0.8f, "грязь на максимуме > 0.8 (%.3f)", e.dirt);
    }
    std::printf("[6] Чистый звук: dirt около нуля\n");
    {
        Engine e; e.prepare (SR); Params p; e.setParams (p);
        auto in = voiceTone (200.0, 1.5); run (e, in);
        CHECK (e.dirt < 0.05f, "dirt чистого пресета %.3f", e.dirt);
    }
    std::printf("[7] SubVox: сабовая частота и все режимы звучат\n");
    {
        for (int mode = 0; mode < 6; ++mode)
        {
            Engine e; e.prepare (SR); Params p; p.sbLevel = 100; p.sbVoice = 0; p.sbOct = -1; p.sbMode = (float) mode; p.sbPunch = 0; p.sbCut = 4000; e.setParams (p);
            auto in = voiceTone (110.0, 2.0);
            auto out = run (e, in);
            const double r = rms (out, (size_t) (1.0 * SR), (size_t) (1.9 * SR));
            std::printf("      mode %d (%s): rms=%.3f f=%.1f\n", mode, modeName (mode), r, estFreq (out, (size_t) (1.0 * SR), (size_t) (1.9 * SR)));
            CHECK (r > 0.02, "режим %s даёт сигнал", modeName (mode));
            if (mode == 0 || mode == 1)
                CHECK (std::fabs (estFreq (out, (size_t) (1.0 * SR), (size_t) (1.9 * SR)) - 55.0) < 2.0, "%s: 110 Hz на -1 октаву = 55 Hz", modeName (mode));
        }
    }
    std::printf("[8] Трекер нот (голос -> MIDI)\n");
    {
        Engine e; e.prepare (SR); Params p; p.sbOct = 0; p.mdTransp = 0; e.setParams (p);
        std::vector<float> in;
        const double freqs[3] = { 220.0, 261.63, 329.63 };
        for (double f : freqs) { auto t = voiceTone (f, 0.45); in.insert (in.end(), t.begin(), t.end()); in.insert (in.end(), (size_t) (0.15 * SR), 0.0f); }
        std::vector<NoteEv> evs; run (e, in, &evs);
        std::vector<int> ons; int offs = 0;
        for (auto& x : evs) { if (x.on) ons.push_back (x.note); else ++offs; }
        std::printf("      on:"); for (int n : ons) std::printf(" %d", n); std::printf("  offs=%d\n", offs);
        CHECK (ons.size() == 3 && ons[0] == 57 && ons[1] == 60 && ons[2] == 64, "A3 C4 E4 -> MIDI 57 60 64");
        CHECK (offs == 3, "три note-off");
        // запись и смещение
        Engine e2; e2.prepare (SR); Params p2; p2.sbOct = -2; p2.mdTransp = 3; e2.setParams (p2);
        std::vector<NoteEv> evs2; run (e2, voiceTone (220.0, 0.5), &evs2);
        CHECK (!evs2.empty() && evs2[0].note == 57 - 24 + 3, "октава -2 и транспонирование +3 -> %d", evs2.empty() ? -1 : evs2[0].note);
    }
    std::printf("[9] MIDI-рекордер, квантование и буфер обмена\n");
    {
        MidiRecorder rec; rec.begin();
        rec.noteOn (40, 100, 0.1 * SR, SR, 120.0); rec.noteOff (40, 0.45 * SR, SR, 120.0);
        rec.noteOn (43, 90, 0.52 * SR, SR, 120.0); rec.finishAll (SR, 120.0);
        Clip c; for (int i = 0; i < rec.count.load(); ++i) c.notes.push_back (rec.notes[i]);
        CHECK (c.notes.size() >= 1, "записано нот: %zu", c.notes.size());
        Clip q = quantizeClip (c, 3);
        const float g = 0.25f; bool ok = !q.notes.empty();
        for (auto& n : q.notes) ok &= std::fabs (n.start / g - std::round (n.start / g)) < 1e-4f;
        CHECK (ok, "квантование 1/16 ставит старты на сетку");
        Clip back; const bool pr = clipFromText (clipToText (q), back);
        CHECK (pr && back.notes.size() == q.notes.size() && back.notes[0].note == q.notes[0].note, "текстовый буфер обмена туда-обратно");
        CHECK (!clipFromText ("мусор", back), "мусор в буфере отвергается");
    }
    std::printf("\n%s (%d провалов)\n", fails ? "ТЕСТ НЕ ПРОЙДЕН" : "ВСЕ ТЕСТЫ ПРОЙДЕНЫ", fails);
    return fails ? 1 : 0;
}
