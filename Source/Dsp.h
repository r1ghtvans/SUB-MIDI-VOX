// PEPEL VOX - весь DSP без JUCE.
// Params, детектор высоты (YIN), PSOLA, вокодер, гранулятор, эффекты, SubVox,
// трекер нот, MIDI-рекордер и клипы. В аудиопотоке нет аллокаций и блокировок.
#pragma once
#include "ParamList.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace pepel
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float dbToLin (float db) { return std::pow (10.0f, db * 0.05f); }
inline float midiToHz (float m) { return 440.0f * std::exp2 ((m - 69.0f) / 12.0f); }
inline float hzToMidi (float f) { return 69.0f + 12.0f * std::log2 (f / 440.0f); }
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }
inline float finite (float v) { return std::isfinite (v) ? v : 0.0f; }

inline float softClip (float x)
{
    const float ax = std::fabs (x);
    if (ax < 0.9f) return x;
    const float y = 0.9f + 0.1f * std::tanh ((ax - 0.9f) * 10.0f);
    return x < 0 ? -y : y;
}

struct Params
{
#define X(id, label, mn, mx, def, step, skew, fmt) float id = (float) (def);
    PEPEL_PARAMS (X)
#undef X
};

// ------------------------------------------------------------------ ноты/лады
inline const bool* scaleMask (int s)
{
    static const bool m[6][12] = {
        { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 },
        { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 }, { 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0 },
        { 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0 }, { 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0 } };
    return m[s < 0 ? 0 : (s > 5 ? 5 : s)];
}
inline bool inScale (int note, int key, int scale) { return scaleMask (scale)[(((note - key) % 12) + 12) % 12]; }

// ближайшая нота лада; prev >= 0 даёт гистерезис
inline float nearestScaleNote (float midi, int key, int scale, float prev = -1.0f)
{
    if (prev >= 0 && inScale ((int) prev, key, scale) && std::fabs (midi - prev) < 0.65f) return prev;
    const int base = (int) std::floor (midi);
    float best = (float) base, bd = 1e9f;
    for (int n = base - 3; n <= base + 4; ++n)
        if (inScale (n, key, scale))
        {
            const float d = std::fabs ((float) n - midi);
            if (d < bd) { bd = d; best = (float) n; }
        }
    return best;
}

struct Rng
{
    uint32_t s = 2463534242u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xFFFFFF) / 16777216.0f; }
    float bi() { return next() * 2.0f - 1.0f; }
};

inline float polyblep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
inline float blepSaw (float ph, float dt) { return 2.0f * ph - 1.0f - polyblep (ph, dt); }

// ------------------------------------------------------------------ детектор высоты (YIN)
class PitchDetector
{
public:
    static constexpr int kWin = 256, kMaxTau = 200, kBuf = 1024, kHop = 64;

    void prepare (double sampleRate)
    {
        D = std::max (1, (int) std::lround (sampleRate / 12000.0));
        dsr = sampleRate / D;
        tauMin = std::max (2, (int) std::ceil (dsr / 1000.0));
        tauMax = std::min (kMaxTau, (int) std::floor (dsr / 75.0));
        lpA = 1.0f - std::exp (-kTwoPi * 3500.0f / (float) sampleRate);
        hopSamples = kHop * D;
        reset();
    }
    void reset()
    {
        std::fill (std::begin (buf), std::end (buf), 0.0f);
        dn = 0; acc = 0; cnt = 0; hop = 0; lp = 0; f0 = 0; voiced = false; unvoicedRun = 0; nHist = 0;
    }
    int hopSize() const { return hopSamples; }
    int delaySamples() const { return (int) ((kWin / 2 + kHop) * D); }

    bool push (float x)
    {
        lp += lpA * (x - lp);
        acc += lp;
        if (++cnt < D) return false;
        buf[dn & (kBuf - 1)] = acc / (float) D;
        acc = 0; cnt = 0; ++dn;
        if (++hop < kHop) return false;
        hop = 0;
        if (dn < kWin + tauMax) return false;
        analyse();
        return true;
    }

    float f0 = 0.0f;
    bool voiced = false;
    float conf = 0.0f;

private:
    void analyse()
    {
        float xs[kWin + kMaxTau], d[kMaxTau + 2], cm[kMaxTau + 2];
        const int base = dn - (kWin + tauMax);
        for (int j = 0; j < kWin + tauMax; ++j) xs[j] = buf[(base + j) & (kBuf - 1)];
        d[0] = 0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0;
            for (int j = tauMax; j < tauMax + kWin; ++j) { const float df = xs[j] - xs[j - tau]; s += df * df; }
            d[tau] = s;
        }
        float run = 0; cm[0] = 1;
        for (int tau = 1; tau <= tauMax; ++tau) { run += d[tau]; cm[tau] = run > 1e-9f ? d[tau] * tau / run : 1.0f; }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (cm[tau] < 0.15f)
            {
                while (tau + 1 < tauMax && cm[tau + 1] < cm[tau]) ++tau;
                best = tau; break;
            }
        if (best < 0)
        {
            float m = 1e9f;
            for (int tau = tauMin; tau < tauMax; ++tau) if (cm[tau] < m) { m = cm[tau]; best = tau; }
            if (m > 0.3f) best = -1;
        }
        float raw = 0;
        if (best >= 1 && best + 1 <= tauMax)
        {
            const float s0 = cm[best - 1], s1 = cm[best], s2 = cm[best + 1];
            const float den = 2.0f * (s0 - 2.0f * s1 + s2);
            const float delta = std::fabs (den) > 1e-9f ? clampf ((s0 - s2) / den, -1.0f, 1.0f) : 0.0f;
            raw = (float) (dsr / (best + delta));
            conf = 1.0f - s1;
        }
        else conf = 0;

        if (raw > 0)
        {
            hist[nHist % 3] = raw; ++nHist; unvoicedRun = 0;
            if (nHist >= 3)
            {
                float a = hist[0], b = hist[1], c = hist[2];
                f0 = std::max (std::min (a, b), std::min (std::max (a, b), c));
            }
            else f0 = raw;
            voiced = true;
        }
        else if (++unvoicedRun >= 2) { voiced = false; nHist = 0; }
    }

    int D = 4, tauMin = 12, tauMax = 160, dn = 0, cnt = 0, hop = 0, unvoicedRun = 0, nHist = 0, hopSamples = 256;
    double dsr = 12000.0;
    float lpA = 0.3f, lp = 0, acc = 0, hist[3] = { 0, 0, 0 };
    float buf[kBuf] {};
};

// ------------------------------------------------------------------ PSOLA
class PsolaCore
{
public:
    static constexpr int N = 16384, M = N - 1;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        maxT = sr / 75.0f; minT = sr / 1000.0f;
        lat = std::min ((int) std::ceil (2.75f * maxT) + 16, N / 2 - 64);
        reset();
    }
    void reset()
    {
        std::fill (std::begin (buf), std::end (buf), 0.0f);
        n = 0; predMark = -1.0; nm = 0; curT = sr / 150.0f;
    }
    int latency() const { return lat; }

    void push (float x, float f0)
    {
        buf[n & M] = x; ++n;
        curT = clampf (f0 > 0 ? sr / f0 : sr / 150.0f, minT, maxT);
        if (predMark < 0) predMark = (double) n + curT;
        const int q = std::max (1, (int) (curT * 0.25f));
        int guard = 0;
        while (predMark + q <= (double) (n - 1) && guard++ < 4)
        {
            const long long c = (long long) predMark;
            long long bi = c; float bv = -1e9f;
            for (long long k = c - q; k <= c + q; ++k)
            {
                const float v = buf[k & M];
                if (v > bv) { bv = v; bi = k; }
            }
            const int idx = (int) (nm & 63);
            markPos[idx] = bi; markT[idx] = (int) curT; ++nm;
            predMark = (double) bi + curT;
        }
        if (predMark + q < (double) (n - 1) - 4 * maxT) predMark = (double) n + curT; // ресинхронизация
    }

    float sr = 48000.0f, maxT = 640.0f, minT = 48.0f, curT = 320.0f;
    int lat = 1776;
    long long n = 0, nm = 0;
    double predMark = -1.0;
    long long markPos[64] {};
    int markT[64] {};
    float buf[N] {};
};

struct PsolaVoice
{
    static constexpr int AN = 8192, AM = AN - 1;
    void reset() { std::fill (std::begin (acc), std::end (acc), 0.0f); synNext = -1.0; }

    float render (PsolaCore& c, float ratio)
    {
        const long long e = c.n - 1 - c.lat;
        if (e < 0) return 0.0f;
        if (synNext < (double) e - 2.0 * c.maxT - 8.0)
        {
            std::fill (std::begin (acc), std::end (acc), 0.0f);
            synNext = (double) e;
        }
        ratio = clampf (ratio, 0.25f, 4.0f);
        const float norm = 1.0f / ratio;
        int guard = 0;
        while (synNext <= (double) e + c.curT + 1.0 && guard++ < 6)
        {
            int bi = -1; double bd = 1e18;
            const int cnt = (int) std::min<long long> (c.nm, 64);
            for (int k = 0; k < cnt; ++k)
            {
                const int idx = (int) ((c.nm - 1 - k) & 63);
                if (c.markPos[idx] + c.markT[idx] > c.n - 1) continue;
                const double d = std::fabs ((double) c.markPos[idx] - synNext);
                if (d < bd) { bd = d; bi = idx; }
            }
            if (bi < 0 || bd > 1.5 * c.maxT) break;
            const long long m = c.markPos[bi];
            const int T = std::max (8, c.markT[bi]);
            const long long o0 = (long long) std::llround (synNext) - T;
            const float inv = kTwoPi / (float) (2 * T);
            for (int i = 0; i < 2 * T; ++i)
            {
                const float w = 0.5f * (1.0f - std::cos (inv * ((float) i + 0.5f)));
                acc[(o0 + i) & AM] += w * c.buf[(m - T + i) & PsolaCore::M] * norm;
            }
            synNext += (double) T / ratio;
        }
        const float y = acc[e & AM];
        acc[e & AM] = 0.0f;
        return y;
    }

    float acc[AN] {};
    double synNext = -1.0;
};

// ------------------------------------------------------------------ фильтры
struct Biquad
{
    float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void bandpass (float f, float q, float sr)
    {
        const float w = kTwoPi * clampf (f, 20.0f, sr * 0.45f) / sr, al = std::sin (w) / (2 * q), a0 = 1 + al;
        b0 = al / a0; b1 = 0; b2 = -al / a0; a1 = -2 * std::cos (w) / a0; a2 = (1 - al) / a0;
    }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

struct Svf
{
    float s1 = 0, s2 = 0, a1 = 0, a2 = 0, a3 = 0;
    void set (float f, float q, float sr)
    {
        const float g = std::tan (kPi * clampf (f, 20.0f, sr * 0.45f) / sr), k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    float lp (float x)
    {
        const float v3 = x - s2, v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2 * v1 - s1; s2 = 2 * v2 - s2;
        return v2;
    }
};

// ------------------------------------------------------------------ вокодер
class Vocoder
{
public:
    static constexpr int kMax = 16;
    void prepare (double sampleRate, int bands, float fLo, float fHi)
    {
        sr = (float) sampleRate; nb = std::min (bands, kMax);
        for (int i = 0; i < nb; ++i)
        {
            const float f = fLo * std::pow (fHi / fLo, (float) i / (float) (nb - 1));
            mod[i].bandpass (f, 4.5f, sr); car[i].bandpass (f, 4.5f, sr); env[i] = 0; mod[i].z1 = mod[i].z2 = car[i].z1 = car[i].z2 = 0;
        }
        atk = 1.0f - std::exp (-1.0f / (0.003f * sr));
        rel = 1.0f - std::exp (-1.0f / (0.02f * sr));
    }
    float process (float m, float c)
    {
        float out = 0;
        for (int i = 0; i < nb; ++i)
        {
            const float a = std::fabs (mod[i].process (m));
            env[i] += (a > env[i] ? atk : rel) * (a - env[i]);
            out += car[i].process (c) * env[i];
        }
        return out * 6.0f;
    }
private:
    int nb = 16;
    float sr = 48000, atk = 0.1f, rel = 0.01f, env[kMax] {};
    Biquad mod[kMax], car[kMax];
};

// ------------------------------------------------------------------ гранулятор
class Granular
{
public:
    static constexpr int N = 65536, M = N - 1, kG = 24;
    void prepare (double sampleRate) { sr = (float) sampleRate; reset(); }
    void reset()
    {
        std::fill (std::begin (buf), std::end (buf), 0.0f);
        for (auto& g : gr) g.on = false;
        w = 0; spawn = 0;
    }
    float process (float in, float sizeMs, float dens, float scat)
    {
        buf[w] = in; w = (w + 1) & M;
        const float len = std::max (64.0f, sizeMs * 0.001f * sr);
        const float overlap = 1.0f + dens * 5.0f;
        spawn += overlap / len;
        while (spawn >= 1.0f)
        {
            spawn -= 1.0f;
            for (auto& g : gr)
                if (! g.on)
                {
                    g.on = true; g.age = 0; g.len = (int) len;
                    const float back = len * 1.2f + rng.next() * scat * 0.25f * sr;
                    g.pos = (float) w - std::min (back, (float) N * 0.9f);
                    g.inc = std::exp2 (rng.bi() * scat * 7.0f / 12.0f);
                    g.amp = 1.0f / std::sqrt (overlap);
                    break;
                }
        }
        float out = 0;
        for (auto& g : gr)
            if (g.on)
            {
                const float win = 0.5f * (1.0f - std::cos (kTwoPi * (float) g.age / (float) g.len));
                float p = g.pos; while (p < 0) p += N; while (p >= N) p -= N;
                const int i0 = (int) p, i1 = (i0 + 1) & M; const float fr = p - (float) i0;
                out += (buf[i0] * (1 - fr) + buf[i1] * fr) * win * g.amp;
                g.pos += g.inc;
                if (++g.age >= g.len) g.on = false;
            }
        return out;
    }
private:
    struct G { bool on = false; float pos = 0, inc = 1, amp = 1; int age = 0, len = 1; };
    float sr = 48000, spawn = 0; int w = 0; G gr[kG]; Rng rng; float buf[N] {};
};

// ------------------------------------------------------------------ эффекты
struct BitCrusher
{
    float hold = 0; int cnt = 0;
    float process (float x, float bits, int div)
    {
        if (cnt <= 0) { hold = x; cnt = div; }
        --cnt;
        const float lv = std::exp2 (bits - 1.0f);
        return std::round (hold * lv) / lv;
    }
};

struct Phaser
{
    float z[4] {}, fbv = 0, ph = 0;
    float process (float x, float rate, float depth, float mix, float sr, float phaseOff)
    {
        ph += rate / sr; if (ph >= 1) ph -= 1;
        const float lfo = 0.5f + 0.5f * std::sin (kTwoPi * (ph + phaseOff));
        const float f = 200.0f * std::pow (15.0f, lfo * depth);
        const float t = std::tan (kPi * std::min (f, sr * 0.4f) / sr), a = (t - 1.0f) / (t + 1.0f);
        float s = x + 0.5f * fbv;
        for (auto& st : z) { const float y = a * s + st; st = s - a * y; s = y; }
        fbv = finite (s);
        return lerpf (x, 0.5f * (x + s), mix);
    }
};

struct Flanger
{
    static constexpr int N = 4096;
    float buf[N] {}, ph = 0, fb = 0; int w = 0;
    float process (float x, float rate, float depth, float mix, float sr, float phaseOff)
    {
        ph += rate / sr; if (ph >= 1) ph -= 1;
        const float lfo = 0.5f + 0.5f * std::sin (kTwoPi * (ph + phaseOff));
        const float dly = (0.0006f + depth * 0.0032f * lfo) * sr;
        buf[w] = x + 0.45f * fb;
        float rp = (float) w - dly; if (rp < 0) rp += N;
        const int i0 = (int) rp, i1 = (i0 + 1) % N; const float fr = rp - (float) i0;
        const float d = buf[i0] * (1 - fr) + buf[i1] * fr;
        fb = finite (d);
        w = (w + 1) % N;
        return (x + mix * 0.7f * d) / (1.0f + 0.4f * mix);
    }
};

struct DelayLine
{
    static constexpr int N = 16384, M = N - 1;
    float buf[N] {}; int w = 0;
    void reset() { std::fill (std::begin (buf), std::end (buf), 0.0f); w = 0; }
    float process (float x, int d) { buf[w & M] = x; const float y = buf[(w - d) & M]; w = (w + 1) & M; return y; }
};

struct Sm
{
    float v = 0, t = 0, a = 0.01f;
    void setRate (float sr, float ms = 10.0f) { a = 1.0f - std::exp (-1.0f / (ms * 0.001f * sr)); }
    void snap (float x) { v = t = x; }
    float next() { v += a * (t - v); return v; }
};

// ------------------------------------------------------------------ события нот
struct NoteEv { int pos = 0, note = 0, vel = 0; bool on = true; int back = 0; };

class NoteTracker
{
public:
    void reset() { cur = -1; cand = -1; candCount = 0; silence = 0; candDb = -90; }
    int currentNote() const { return cur; }

    void frame (bool voiced, float midi, float levelDb, int minFrames, int shift, bool lock, int key, int scale,
                int pos, int hopSamples, int detDelay, NoteEv* ev, int& nEv, int cap)
    {
        auto emit = [&] (bool on, int voiceNote, int vel, int back)
        {
            if (nEv < cap)
                ev[nEv++] = { pos, std::clamp (voiceNote + shift, 0, 127), vel, on, back };
        };
        auto velOf = [] (float db) { return std::clamp ((int) (40.0f + (db + 45.0f) / 37.0f * 87.0f), 1, 127); };

        if (voiced)
        {
            silence = 0;
            const float mm = lock ? nearestScaleNote (midi, key, scale) : midi;
            const int mi = (int) std::lround (mm);
            if (cur < 0)
            {
                if (mi == cand) ++candCount; else { cand = mi; candCount = 1; candDb = levelDb; }
                candDb = std::max (candDb, levelDb);
                if (candCount >= minFrames)
                {
                    emit (true, cand, velOf (candDb), candCount * hopSamples + detDelay);
                    cur = cand; cand = -1; candCount = 0;
                }
            }
            else if (std::fabs (mm - (float) cur) < 0.7f) { cand = -1; candCount = 0; }
            else
            {
                if (mi == cand) ++candCount; else { cand = mi; candCount = 1; candDb = levelDb; }
                candDb = std::max (candDb, levelDb);
                if (candCount >= std::max (4, minFrames))
                {
                    const int back = candCount * hopSamples + detDelay;
                    emit (false, cur, 0, back);
                    emit (true, cand, velOf (candDb), back);
                    cur = cand; cand = -1; candCount = 0;
                }
            }
        }
        else
        {
            ++silence;
            if (cur >= 0) { if (silence >= 6) { emit (false, cur, 0, silence * hopSamples + detDelay); cur = -1; cand = -1; candCount = 0; } }
            else { cand = -1; candCount = 0; }
        }
    }
    void forceOff (int pos, int shift, NoteEv* ev, int& nEv, int cap)
    {
        if (cur >= 0 && nEv < cap) ev[nEv++] = { pos, std::clamp (cur + shift, 0, 127), 0, false, 0 };
        cur = -1; cand = -1; candCount = 0;
    }
private:
    int cur = -1, cand = -1, candCount = 0, silence = 0; float candDb = -90;
};

// ------------------------------------------------------------------ SubVox
class SubSynth
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        voc.prepare (sampleRate, 12, 110.0f, 5000.0f);
        reset();
    }
    void reset()
    {
        midiSm = 36; gateEnv = 0; amp = 0; punch = 0; decay = 0; wasGate = false;
        ph1 = ph2 = ph3 = lfoPh = 0; svf = Svf(); prevMode = -1;
    }

    float tick (const Params& p, float midiTarget, bool gate, float lvl01, float bright01, float voiceIn)
    {
        const int mode = (int) std::lround (p.sbMode);
        if (mode != prevMode) { prevMode = mode; }
        const float glide = p.sbGlide < 0.5f ? 1.0f : 1.0f - std::exp (-1.0f / (p.sbGlide * 0.001f * sr));
        if (gate && ! wasGate) { midiSm = midiTarget; punch = 1.0f; decay = 1.0f; }
        else if (gate) midiSm += glide * (midiTarget - midiSm);
        wasGate = gate;

        const float aA = 1.0f - std::exp (-1.0f / (0.004f * sr));
        const float aR = 1.0f - std::exp (-1.0f / ((mode == 1 ? 0.25f : 0.06f) * sr));
        amp += (gate ? aA : aR) * ((gate ? 1.0f : 0.0f) - amp);
        punch *= std::exp (-1.0f / (0.03f * sr));
        decay *= std::exp (-1.0f / (0.45f * sr));
        const float decayAmp = mode == 1 ? 0.35f + 0.65f * decay : 1.0f;
        if (amp < 1e-5f && ! gate) return 0.0f;

        const float pk = p.sbPunch * 0.01f;
        float freq = midiToHz (midiSm) * (1.0f + pk * punch * (mode == 1 ? 0.9f : 0.3f));
        freq = clampf (freq, 18.0f, 2500.0f);
        const float dt = freq / sr;
        auto adv = [] (float& ph, float inc) { ph += inc; if (ph >= 1.0f) ph -= 1.0f; };

        lfoPh += p.sbWobRate / sr; if (lfoPh >= 1.0f) lfoPh -= 1.0f;
        const float lfo = std::sin (kTwoPi * lfoPh), wob = p.sbWobDepth * 0.01f;
        float s = 0;
        switch (mode)
        {
            case 0: adv (ph1, dt); s = std::sin (kTwoPi * ph1); break;
            case 1: adv (ph1, dt); s = std::sin (kTwoPi * ph1); break;
            case 2:
            {
                const float u = std::exp2 (p.sbDetune / 1200.0f);
                adv (ph1, dt); adv (ph2, dt * u); adv (ph3, dt / u);
                s = (blepSaw (ph1, dt) + blepSaw (ph2, dt * u) + blepSaw (ph3, dt / u)) * 0.38f + 0.45f * std::sin (kTwoPi * ph1);
                break;
            }
            case 3:
            {
                adv (ph1, dt);
                const float idx = 1.2f + bright01 * 5.0f + lfo * wob * 2.0f + lvl01 * 1.5f;
                s = std::sin (kTwoPi * ph1 + idx * std::sin (kTwoPi * 2.0f * ph1));
                break;
            }
            case 4:
            {
                adv (ph1, dt);
                const float duty = clampf (0.25f + 0.2f * lfo * wob, 0.08f, 0.5f);
                float ph2x = ph1 + duty; if (ph2x >= 1.0f) ph2x -= 1.0f;
                s = 0.8f * (blepSaw (ph1, dt) - blepSaw (ph2x, dt)) + 0.3f * std::sin (kTwoPi * ph1);
                s = std::round (s * 8.0f) / 8.0f;
                break;
            }
            default:
            {
                adv (ph1, dt);
                const float car = 0.7f * blepSaw (ph1, dt) + 0.4f * std::sin (kTwoPi * ph1);
                s = voc.process (voiceIn, car) * 0.8f + 0.35f * std::sin (kTwoPi * ph1);
                break;
            }
        }

        const float fc = clampf (p.sbCut * std::exp2 (p.sbFollow * 0.01f * bright01 * 3.0f + wob * lfo * 2.0f), 30.0f, 16000.0f);
        svf.set (fc, 0.6f + p.sbReso * 0.09f, sr);
        s = svf.lp (s);
        const float g = 1.0f + p.sbDrive * 0.1f;
        s = std::tanh (s * g) / std::tanh (g);
        return finite (s * amp * decayAmp * (0.55f + 0.45f * lvl01));
    }
private:
    float sr = 48000, midiSm = 36, gateEnv = 0, amp = 0, punch = 0, decay = 0, ph1 = 0, ph2 = 0, ph3 = 0, lfoPh = 0;
    bool wasGate = false; int prevMode = -1; Svf svf; Vocoder voc;
};

// ------------------------------------------------------------------ MIDI-клипы
struct MidiNote { float start = 0, len = 0; int note = 60, vel = 100; };
struct Clip
{
    std::vector<MidiNote> notes;
    float lengthBeats = 4.0f;
    bool empty() const { return notes.empty(); }
};

inline float gridBeats (int g)
{
    static const float t[5] = { 0.0f, 1.0f, 0.5f, 0.25f, 0.125f };
    return t[g < 0 ? 0 : (g > 4 ? 4 : g)];
}

inline Clip quantizeClip (const Clip& c, int grid)
{
    Clip q = c;
    const float g = gridBeats (grid);
    if (g > 0)
        for (auto& n : q.notes)
        {
            const float s = std::round (n.start / g) * g, e = std::round ((n.start + n.len) / g) * g;
            n.start = s; n.len = std::max (g, e - s);
        }
    std::stable_sort (q.notes.begin(), q.notes.end(), [] (const MidiNote& a, const MidiNote& b) { return a.start < b.start; });
    float end = 0; for (auto& n : q.notes) end = std::max (end, n.start + n.len);
    q.lengthBeats = std::max (4.0f, std::ceil (std::max (c.lengthBeats, end) / 4.0f) * 4.0f);
    return q;
}

inline std::string clipToText (const Clip& c)
{
    std::ostringstream o;
    o << "PEPELMIDI1 " << c.lengthBeats << " " << c.notes.size() << "\n";
    for (auto& n : c.notes) o << n.start << " " << n.len << " " << n.note << " " << n.vel << "\n";
    return o.str();
}
inline bool clipFromText (const std::string& s, Clip& out)
{
    std::istringstream i (s);
    std::string tag; size_t cnt = 0; float len = 4;
    if (! (i >> tag >> len >> cnt) || tag != "PEPELMIDI1" || cnt > 4096) return false;
    Clip c; c.lengthBeats = clampf (len, 1.0f, 512.0f);
    for (size_t k = 0; k < cnt; ++k)
    {
        MidiNote n;
        if (! (i >> n.start >> n.len >> n.note >> n.vel)) return false;
        n.note = std::clamp (n.note, 0, 127); n.vel = std::clamp (n.vel, 1, 127);
        if (n.start < 0 || n.len <= 0) continue;
        c.notes.push_back (n);
    }
    out = c;
    return true;
}

// Рекордер (запись из аудиопотока без аллокаций; читает UI через count)
class MidiRecorder
{
public:
    static constexpr int kCap = 2048;
    void begin()
    {
        count.store (0, std::memory_order_release);
        samples = 0;
        for (int i = 0; i < 128; ++i) open[i] = -1.0;
    }
    void advance (int n) { samples += n; }
    double sampleCount() const { return samples; }

    void noteOn (int note, int vel, double atSample, double sr, double bpm)
    {
        if (open[note] >= 0) noteOff (note, atSample, sr, bpm);
        open[note] = std::max (0.0, atSample) / sr * bpm / 60.0; openVel[note] = vel;
    }
    void noteOff (int note, double atSample, double sr, double bpm)
    {
        if (open[note] < 0) return;
        const double e = std::max (0.0, atSample) / sr * bpm / 60.0;
        const int idx = count.load (std::memory_order_relaxed);
        if (idx < kCap && e > open[note])
        {
            notes[idx] = { (float) open[note], (float) (e - open[note]), note, openVel[note] };
            count.store (idx + 1, std::memory_order_release);
        }
        open[note] = -1.0;
    }
    void finishAll (double sr, double bpm)
    {
        for (int i = 0; i < 128; ++i) if (open[i] >= 0) noteOff (i, samples, sr, bpm);
    }
    double beats (double sr, double bpm) const { return samples / sr * bpm / 60.0; }

    MidiNote notes[kCap];
    std::atomic<int> count { 0 };
private:
    double samples = 0, open[128] {}; int openVel[128] {};
};

// ------------------------------------------------------------------ Engine
class Engine
{
public:
    static constexpr int kMaxEv = 64;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        det.prepare (sampleRate);
        core.prepare (sampleRate);
        vMain.reset(); vH1.reset(); vH2.reset();
        voc.prepare (sampleRate, 16, 120.0f, 7500.0f);
        gran.prepare (sampleRate);
        sub.prepare (sampleRate);
        dlyL.reset(); dlyR.reset(); dlySub.reset();
        phL = Phaser(); phR = Phaser(); flL = Flanger(); flR = Flanger();
        bc = BitCrusher(); tracker.reset();
        for (Sm* s : { &gIn, &gOut, &dw, &mVoc, &mHarm, &mGr, &mBc, &mDi, &mEx, &mPh, &mFl, &dTr, &mSubLvl, &mSubVoice })
            s->setRate (sr, 12.0f);
        phL.ph = 0; phR.ph = 0.0f;
        subDelay = std::max (0, core.latency() - (int) (0.0145f * sr));
        envIn = 0; hpS = 0; bright = 0; dirtSm = 0; reset();
    }
    void reset()
    {
        wP = 0; corrSm = 0; corrTarget = 0; prevTarget = -1; voicedSm = 0; carPh = 0; carPh2 = 0; subGateHold = 0;
        subMidi = 36; subGate = false; lastF0 = 0; lvl01 = 0;
    }
    int latency() const { return core.latency(); }

    void setParams (const Params& np)
    {
        p = np;
        gIn.t = dbToLin (p.inGain); gOut.t = dbToLin (p.outGain); dw.t = p.dryWet * 0.01f;
        mVoc.t = p.pitVoc * 0.01f; mHarm.t = p.pitHarm * 0.01f; mGr.t = p.grMix * 0.01f; mBc.t = p.bcMix * 0.01f;
        mDi.t = p.diMix * 0.01f; mEx.t = p.exAmt * 0.01f; mPh.t = p.phMix * 0.01f; mFl.t = p.flMix * 0.01f;
        dTr.t = p.trDepth * 0.01f; mSubLvl.t = p.sbLevel * 0.01f; mSubVoice.t = p.sbVoice * 0.01f;
    }
    void setExternal (bool active, int midi, bool gate, float vel01) { ext = active; extMidi = (float) midi; extGate = gate; extVel = vel01; }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        nEv = 0; peakIn = 0; peakOut = 0;
        const int key = (int) std::lround (p.atKey), scale = (int) std::lround (p.atScale);
        const int ctrlShift = 12 * (int) std::lround (p.sbOct) + (int) std::lround (p.mdTransp);
        const int minFrames = std::max (2, (int) std::lround (p.mdMinLen * 0.001f * sr / det.hopSize()));
        const float aSpeed = p.atSpeed < 0.5f ? 1.0f : 1.0f - std::exp (-1.0f / (p.atSpeed * 0.001f * sr));
        const float aEnvA = 1.0f - std::exp (-1.0f / (0.002f * sr)), aEnvR = 1.0f - std::exp (-1.0f / (0.04f * sr));
        const float aHp = 1.0f - std::exp (-kTwoPi * 1800.0f / sr);
        const float aBr = 1.0f - std::exp (-1.0f / (0.03f * sr));
        const int crushDiv = std::max (1, (int) std::lround (p.bcRate));
        const float crushBits = std::round (p.bcBits);
        const int dly = core.latency();
        const float exA = 1.0f - std::exp (-kTwoPi * p.exFreq / sr);
        const float toneA = 1.0f - std::exp (-kTwoPi * (800.0f + std::pow (p.diTone * 0.01f, 2.0f) * 11000.0f) / sr);
        const float dr = p.diDrive * 0.01f;
        const float dG = 1.0f + dr * 15.0f, dC = 1.0f / (1.0f + dr * 1.5f);
        const float amtAt = p.atAmount * 0.01f, pitchSt = p.pitShift, detSt = p.atDetune * 0.01f;
        const float i1 = p.pitInt1, i2 = p.pitInt2;
        const float lockAmt = p.sbLock * 0.01f;

        for (int i = 0; i < n; ++i)
        {
            const float gi = gIn.next();
            const float l = inL[i] * gi, r = (inR ? inR[i] : inL[i]) * gi, mono = 0.5f * (l + r);
            peakIn = std::max (peakIn, std::fabs (mono));

            const float a = std::fabs (mono);
            envIn += (a > envIn ? aEnvA : aEnvR) * (a - envIn);
            hpS += aHp * (mono - hpS);
            hpEnv += aBr * (std::fabs (mono - hpS) - hpEnv);
            bright += aBr * (clampf (hpEnv / (envIn + 1e-5f) * 1.5f, 0.0f, 1.0f) - bright);

            if (det.push (mono)) frameUpdate (i, key, scale, minFrames, ctrlShift, lockAmt);

            const float dl = dlyL.process (l, dly), drr = dlyR.process (r, dly), dm = 0.5f * (dl + drr);
            core.push (mono, det.voiced ? det.f0 : 0.0f);

            // автотюн + питч
            corrSm += aSpeed * (corrTarget - corrSm);
            const float semis = corrSm * amtAt + pitchSt + detSt;
            const float ratio = std::exp2 (semis / 12.0f);
            const float yP = vMain.render (core, ratio);
            wP += 0.01f * (((det.voiced && std::fabs (semis) > 0.02f) ? 1.0f : 0.0f) - wP);
            float voice = lerpf (dm, yP, wP);

            const float hm = mHarm.next();
            if (hm > 0.002f)
            {
                const float h1 = vH1.render (core, ratio * std::exp2 (i1 / 12.0f));
                const float h2 = vH2.render (core, ratio * std::exp2 (i2 / 12.0f));
                voice += hm * 0.7f * (h1 + h2) * wP;
            }
            else { vH1.synNext = -1; vH2.synNext = -1; }

            // вокодер
            const float vm = mVoc.next();
            voicedSm += 0.002f * ((det.voiced ? 1.0f : 0.0f) - voicedSm);
            if (vm > 0.002f)
            {
                const float f = (det.voiced ? det.f0 : lastF0 > 0 ? lastF0 : 140.0f) * ratio;
                const float dt = clampf (f / sr, 0.0001f, 0.2f);
                carPh += dt; if (carPh >= 1) carPh -= 1;
                carPh2 += dt * 1.006f; if (carPh2 >= 1) carPh2 -= 1;
                const float saw = 0.5f * (blepSaw (carPh, dt) + blepSaw (carPh2, dt * 1.006f));
                const float car = lerpf (rng.bi() * 0.5f, saw, voicedSm);
                const float sib = dm - lpS; lpS += 0.45f * (dm - lpS);
                voice = lerpf (voice, voc.process (dm, car) + 0.5f * sib * (1.0f - voicedSm * 0.7f), vm);
            }
            if (det.voiced) lastF0 = det.f0;

            float x = voice;
            // гранулятор
            const float gm = mGr.next();
            const float gr = gran.process (x, p.grSize, p.grDens * 0.01f, p.grScat * 0.01f);
            if (gm > 0.002f) x = lerpf (x, gr, gm);
            // bitcrusher
            const float bm = mBc.next();
            if (bm > 0.002f) x = lerpf (x, bc.process (x, crushBits, crushDiv), bm);
            // distortion
            const float dmx = mDi.next();
            if (dmx > 0.002f) { const float d = std::tanh (x * dG) * dC; toneLp += toneA * (d - toneLp); x = lerpf (x, toneLp * 1.4f, dmx); }
            // exciter
            const float em = mEx.next();
            if (em > 0.002f) { exLp += exA * (x - exLp); const float hp = x - exLp; x += em * 0.9f * std::tanh (hp * 4.0f) * 0.5f; }
            // tremolo
            const float tdp = dTr.next();
            trPh += p.trRate / sr; if (trPh >= 1) trPh -= 1;
            if (tdp > 0.002f) x *= 1.0f - tdp * 0.5f * (1.0f - std::cos (kTwoPi * trPh));
            // phaser / flanger
            const float pm = mPh.next(), fm = mFl.next();
            float wl = x, wr = x;
            if (pm > 0.002f) { wl = phL.process (wl, p.phRate, p.phDepth * 0.01f, pm, sr, 0.0f); wr = phR.process (wr, p.phRate, p.phDepth * 0.01f, pm, sr, 0.25f); }
            if (fm > 0.002f) { wl = flL.process (wl, p.flRate, p.flDepth * 0.01f, fm, sr, 0.0f); wr = flR.process (wr, p.flRate, p.flDepth * 0.01f, fm, sr, 0.3f); }

            const float d = dw.next();
            float oL = lerpf (dl, wl, d), oR = lerpf (drr, wr, d);

            // SubVox
            const float vg = mSubVoice.next(), sl = mSubLvl.next();
            oL *= vg; oR *= vg;
            if (sl > 0.001f || subActive)
            {
                float sGate = ext ? extGate : subGate, tgt = ext ? extMidi : subMidi, lv = ext ? extVel : lvl01;
                const float sv = sub.tick (p, tgt, sGate, lv, bright, dm);
                const float sd = dlySub.process (sv, ext ? 0 : subDelay);
                oL += sd * sl; oR += sd * sl;
                subActive = std::fabs (sv) > 1e-5f;
            }

            const float go = gOut.next();
            oL = softClip (finite (oL * go)); oR = softClip (finite (oR * go));
            peakOut = std::max (peakOut, std::max (std::fabs (oL), std::fabs (oR)));
            outL[i] = oL; if (outR) outR[i] = oR;
        }

        // «грязь»
        const float wet = p.dryWet * 0.01f;
        const float bcA = (mBcT() ) * clampf ((16.0f - p.bcBits) / 12.0f * 0.6f + (p.bcRate - 1.0f) / 31.0f * 0.6f, 0.0f, 1.0f);
        float sum = wet * (bcA * 0.9f + p.diMix * 0.01f * p.diDrive * 0.01f * 1.0f + p.exAmt * 0.01f * 0.35f
                           + p.grMix * 0.01f * (0.5f + 0.5f * p.grScat * 0.01f) * 0.8f
                           + 0.3f * (p.phMix * p.phDepth + p.flMix * p.flDepth + p.trDepth * 100.0f) * 0.0001f
                           + p.atAmount * 0.01f * (1.0f - p.atSpeed / 200.0f) * 0.2f + p.pitVoc * 0.01f * 0.2f);
        sum += p.sbLevel * 0.01f * (p.sbDrive * 0.01f * 0.5f + p.sbLock * 0.01f * 0.2f + (p.sbMode >= 2.5f ? 0.15f : 0.0f));
        const float target = clampf (1.0f - std::exp (-1.4f * sum), 0.0f, 1.0f);
        dirtSm += (1.0f - std::exp (-(float) n / (0.35f * sr))) * (target - dirtSm);
        dirt = dirtSm;
        inLevel = peakIn; outLevel = peakOut;
        liveNote = tracker.currentNote();
    }

    NoteEv ev[kMaxEv];
    int nEv = 0;
    float dirt = 0, inLevel = 0, outLevel = 0;
    float detectedF0() const { return det.f0; }
    int liveNote = -1;
    int detDelay() const { return det.delaySamples(); }

private:
    float mBcT() const { return p.bcMix * 0.01f; }

    void frameUpdate (int pos, int key, int scale, int minFrames, int shift, float lock)
    {
        const bool voiced = det.voiced;
        const float lvlDb = 20.0f * std::log10 (envIn + 1e-9f);
        const bool open = voiced && lvlDb > p.sbGate;
        float midi = 0;
        if (voiced)
        {
            midi = hzToMidi (det.f0);
            prevTarget = nearestScaleNote (midi, key, scale, prevTarget);
            corrTarget = prevTarget - midi;
            const float snapped = nearestScaleNote (midi, key, scale);
            subMidi = lerpf (midi, snapped, lock) + 12.0f * std::round (p.sbOct);
        }
        if (open) subGateHold = 4; else if (subGateHold > 0) --subGateHold;
        subGate = subGateHold > 0;
        lvl01 = clampf ((lvlDb + 45.0f) / 35.0f, 0.0f, 1.0f);
        tracker.frame (open, midi, lvlDb, minFrames, shift, lock > 0.5f, key, scale, pos, det.hopSize(), det.delaySamples(), ev, nEv, kMaxEv);
    }

    float sr = 48000;
    Params p;
    PitchDetector det; PsolaCore core; PsolaVoice vMain, vH1, vH2; Vocoder voc; Granular gran; SubSynth sub; NoteTracker tracker;
    DelayLine dlyL, dlyR, dlySub; Phaser phL, phR; Flanger flL, flR; BitCrusher bc; Rng rng;
    Sm gIn, gOut, dw, mVoc, mHarm, mGr, mBc, mDi, mEx, mPh, mFl, dTr, mSubLvl, mSubVoice;
    float envIn = 0, hpS = 0, hpEnv = 0, bright = 0, wP = 0, corrSm = 0, corrTarget = 0, prevTarget = -1, voicedSm = 0;
    float carPh = 0, carPh2 = 0, lpS = 0, toneLp = 0, exLp = 0, trPh = 0, lastF0 = 0, lvl01 = 0, subMidi = 36, dirtSm = 0, peakIn = 0, peakOut = 0;
    int subGateHold = 0, subDelay = 0;
    bool subGate = false, subActive = false, ext = false, extGate = false;
    float extMidi = 36, extVel = 0.8f;
};
} // namespace pepel
