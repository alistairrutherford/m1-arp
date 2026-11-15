#pragma once

#include "Model.h"
#include <memory>
#include <vector>

namespace m1
{

inline int delaySamplesFromKnob (int knob, double sr)
{
    const double ms = 5.0 + clampInt (knob, 0, 99) / 99.0 * 995.0;
    return std::max (1, (int) std::llround (ms * 0.001 * sr));
}

// Effects whose wet signal is added to the dry one (reverbs, delays and the
// modulated delays). Their balance keeps the dry signal at full level up to
// 50:50 and the wet at full level from 50:50, so a reverb does not make the
// program quieter. The other types (EQ, drive, exciter, tremolo, rotary)
// already carry the whole signal and crossfade linearly.
inline bool effectIsAdditive (int type)
{
    return (type >= 1 && type <= 17) || type == 24 || type >= 26;
}

inline float effectDryGain (int type, int balance)
{
    const float t = clampInt (balance, 0, 98) / 98.0f;
    return effectIsAdditive (type) ? std::min (1.0f, 2.0f * (1.0f - t)) : 1.0f - t;
}

inline float effectWetGain (int type, int balance)
{
    const float t = clampInt (balance, 0, 98) / 98.0f;
    return effectIsAdditive (type) ? std::min (1.0f, 2.0f * t) : t;
}

class FxUnit
{
public:
    void prepare (double sr, int maxBlock, bool withInner = true)
    {
        sampleRate = sr > 0 ? sr : 44100.0;
        const int n = std::max (8, (int) (sampleRate * 2.1));
        buf.assign ((size_t) n, 0.0f);
        bufR.assign ((size_t) n, 0.0f);
        const double scale = sampleRate / 44100.0 * 1.45;
        for (int i = 0; i < kCombs; ++i)
        {
            comb[i].assign ((size_t) ((kCombTuning[i] + kStereoSpread) * scale) + 4, 0.0f);
            combR[i].assign ((size_t) ((kCombTuning[i] + kStereoSpread) * scale) + 4, 0.0f);
        }
        for (int i = 0; i < kAllpasses; ++i)
        {
            allpass[i].assign ((size_t) ((kAllpassTuning[i] + kStereoSpread) * scale) + 4, 0.0f);
            allpassR[i].assign ((size_t) ((kAllpassTuning[i] + kStereoSpread) * scale) + 4, 0.0f);
        }
        const int cap = std::max (maxBlock, 1);
        wetL.assign ((size_t) cap, 0.0f);
        wetR.assign ((size_t) cap, 0.0f);
        scratchL.assign ((size_t) cap, 0.0f);
        scratchR.assign ((size_t) cap, 0.0f);
        if (withInner)
        {
            if (inner == nullptr)
                inner = std::make_unique<FxUnit>();
            inner->prepare (sr, maxBlock, false);
        }
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        for (int i = 0; i < kCombs; ++i)
        {
            std::fill (comb[i].begin(), comb[i].end(), 0.0f);
            std::fill (combR[i].begin(), combR[i].end(), 0.0f);
            cw[i] = cwR[i] = 0;
            combLp[i] = combLpR[i] = 0.0f;
        }
        for (int i = 0; i < kAllpasses; ++i)
        {
            std::fill (allpass[i].begin(), allpass[i].end(), 0.0f);
            std::fill (allpassR[i].begin(), allpassR[i].end(), 0.0f);
            aw[i] = awR[i] = 0;
        }
        w = wR = 0;
        phase = phase2 = 0.0;
        low = lowR = high = highR = 0.0f;
        xLow = xLowR = 0.0f;
        rotorHorn = rotorDrum = -1.0;
        for (int i = 0; i < 4; ++i) ap[i] = apR[i] = 0.0f;
        apOut = apOutR = 0.0f;
        if (inner != nullptr)
            inner->reset();
    }

    void set (Fx fx) { spec = fx; }
    Fx specNow() const { return spec; }

    void process (float* L, float* R, int n)
    {
        if (n <= 0 || spec.type == 0 || effectWetGain (spec.type, spec.balance) <= 0.0f)
            return;
        if ((int) wetL.size() < n)
            return;
        renderWet (L, R, n);
        const float dry = effectDryGain (spec.type, spec.balance);
        const float wet = effectWetGain (spec.type, spec.balance);
        for (int i = 0; i < n; ++i)
        {
            L[i] = L[i] * dry + wetL[(size_t) i] * wet;
            R[i] = R[i] * dry + wetR[(size_t) i] * wet;
        }
    }

    void renderWetInto (const float* L, const float* R, float* oL, float* oR, int n)
    {
        if (n <= 0 || spec.type == 0 || effectWetGain (spec.type, spec.balance) <= 0.0f || (int) wetL.size() < n)
        {
            for (int i = 0; i < n; ++i) { oL[i] = 0; oR[i] = 0; }
            return;
        }
        renderWet (L, R, n);
        const float wet = effectWetGain (spec.type, spec.balance);
        for (int i = 0; i < n; ++i)
        {
            oL[i] = wetL[(size_t) i] * wet;
            oR[i] = wetR[(size_t) i] * wet;
        }
    }

    // Wet output of the last render, for combination effects.
    const float* wetLeft() const { return wetL.data(); }
    const float* wetRight() const { return wetR.data(); }
    void renderWetOnly (const float* L, const float* R, int n) { renderWet (L, R, n); }

private:
    static constexpr int kCombs = 8;
    static constexpr int kAllpasses = 4;
    static constexpr int kStereoSpread = 23;
    static constexpr int kCombTuning[kCombs] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    static constexpr int kAllpassTuning[kAllpasses] = { 556, 441, 341, 225 };

    void renderWet (const float* L, const float* R, int n)
    {
        const int type = spec.type;
        if (type >= 1 && type <= 6) renderReverb (L, R, n, type);
        else if (type >= 7 && type <= 9) renderEarly (L, R, n, type);
        else if (type == 10 || type == 11) renderDelay (L, R, n, type == 11);
        else if (type == 12 || type == 13) renderChorus (L, R, n, type == 13 ? 2 : 1);
        else if (type == 24) renderEnsemble (L, R, n);
        else if (type == 14 || type == 15) renderFlanger (L, R, n, type == 15);
        else if (type == 16 || type == 17) renderPhaser (L, R, n, type == 17);
        else if (type == 18 || type == 19) renderTremolo (L, R, n, type == 18);
        else if (type == 20) renderEq (L, R, n);
        else if (type == 21 || type == 22) renderDrive (L, R, n, type == 22);
        else if (type == 23) renderExciter (L, R, n);
        else if (type == 25) renderRotary (L, R, n);
        else if (type >= 26 && type <= 33) renderCombo (L, R, n, type);
        else
            for (int i = 0; i < n; ++i) { wetL[(size_t) i] = L[i]; wetR[(size_t) i] = R[i]; }
    }

    // ------------------------------------------------------------ helpers

    static float readDelay (const std::vector<float>& b, int write, int delay)
    {
        const int size = (int) b.size();
        if (size <= 1) return 0.0f;
        delay = std::max (1, std::min (delay, size - 1));
        int i = write - delay;
        if (i < 0) i += size;
        return b[(size_t) i];
    }

    // Linear-interpolated read for modulated delays (no zipper noise).
    static float readFrac (const std::vector<float>& b, int write, double delay)
    {
        const int size = (int) b.size();
        if (size <= 2) return 0.0f;
        delay = std::max (1.0, std::min (delay, (double) size - 2.0));
        const int d0 = (int) delay;
        const float frac = (float) (delay - d0);
        int i0 = write - d0;
        if (i0 < 0) i0 += size;
        int i1 = i0 - 1;
        if (i1 < 0) i1 += size;
        return b[(size_t) i0] + frac * (b[(size_t) i1] - b[(size_t) i0]);
    }

    static void writeDelay (std::vector<float>& b, int& write, float x)
    {
        if (b.empty()) return;
        b[(size_t) write] = x;
        if (++write >= (int) b.size()) write = 0;
    }

    static float lfo (double ph, bool triangle)
    {
        const double p = ph - std::floor (ph);
        if (triangle)
            return (float) (p < 0.5 ? p * 4.0 - 1.0 : 3.0 - p * 4.0);
        return (float) std::sin (p * 6.283185307179586);
    }

    double knobHz (int knob, double lo, double hi) const
    {
        const double x = clampInt (knob, 0, 99) / 99.0;
        return lo * std::pow (hi / lo, x);
    }

    // ------------------------------------------------------------- reverb

    void renderReverb (const float* L, const float* R, int n, int type)
    {
        static const float sizes[7] = { 1.0f, 1.0f, 1.08f, 1.3f, 0.55f, 0.75f, 0.9f };
        static const float bright[7] = { 0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.25f };
        const float size = sizes[clampInt (type, 0, 6)];
        const float time = clampInt (spec.p[0], 0, 99) / 99.0f;
        const float feedback = std::min (0.975f, 0.72f + 0.255f * time + (size - 1.0f) * 0.05f);
        const float damp = std::max (0.0f, std::min (0.9f, 0.1f + 0.65f * spec.p[3] / 99.0f - bright[type]));
        const int pre = std::max (1, (int) (spec.p[1] / 99.0f * 0.15f * sampleRate));
        const float early = spec.p[2] / 99.0f;
        const double scale = sampleRate / 44100.0 * size;
        int lenL[kCombs], lenR[kCombs], apL[kAllpasses], apR2[kAllpasses];
        for (int c = 0; c < kCombs; ++c)
        {
            lenL[c] = std::max (8, std::min ((int) (kCombTuning[c] * scale), (int) comb[c].size() - 1));
            lenR[c] = std::max (8, std::min ((int) ((kCombTuning[c] + kStereoSpread) * scale), (int) combR[c].size() - 1));
        }
        for (int a = 0; a < kAllpasses; ++a)
        {
            apL[a] = std::max (4, std::min ((int) (kAllpassTuning[a] * scale), (int) allpass[a].size() - 1));
            apR2[a] = std::max (4, std::min ((int) ((kAllpassTuning[a] + kStereoSpread) * scale), (int) allpassR[a].size() - 1));
        }
        const int e1 = std::max (1, (int) (sampleRate * 0.0113 * size));
        const int e2 = std::max (1, (int) (sampleRate * 0.0191 * size));
        const int e3 = std::max (1, (int) (sampleRate * 0.0277 * size));
        const int e4 = std::max (1, (int) (sampleRate * 0.0359 * size));

        for (int i = 0; i < n; ++i)
        {
            writeDelay (buf, w, 0.5f * (L[i] + R[i]));
            const float in = readDelay (buf, w, pre);
            float mod = 0.0f;
            if (type == 2)
            {
                phase += 0.5 / sampleRate;
                mod = 0.0025f * (float) std::sin (phase * 6.283185307179586);
            }
            const float x = in * 0.03f;
            float outL = 0.0f, outR = 0.0f;
            for (int c = 0; c < kCombs; ++c)
            {
                const float yl = readDelay (comb[c], cw[c], lenL[c]);
                combLp[c] = yl * (1.0f - damp) + combLp[c] * damp;
                writeDelay (comb[c], cw[c], x + combLp[c] * (feedback + mod));
                outL += yl;
                const float yr = readDelay (combR[c], cwR[c], lenR[c]);
                combLpR[c] = yr * (1.0f - damp) + combLpR[c] * damp;
                writeDelay (combR[c], cwR[c], x + combLpR[c] * (feedback - mod));
                outR += yr;
            }
            for (int a = 0; a < kAllpasses; ++a)
            {
                const float bl = readDelay (allpass[a], aw[a], apL[a]);
                writeDelay (allpass[a], aw[a], outL + bl * 0.5f);
                outL = bl - outL;
                const float br = readDelay (allpassR[a], awR[a], apR2[a]);
                writeDelay (allpassR[a], awR[a], outR + br * 0.5f);
                outR = br - outR;
            }
            const float erL = 0.5f * readDelay (buf, w, pre + e1) + 0.35f * readDelay (buf, w, pre + e3);
            const float erR = 0.5f * readDelay (buf, w, pre + e2) + 0.35f * readDelay (buf, w, pre + e4);
            wetL[(size_t) i] = outL * 1.6f + early * erL;
            wetR[(size_t) i] = outR * 1.6f + early * erR;
        }
    }

    void renderEarly (const float* L, const float* R, int n, int type)
    {
        static const float taps[3][6] = {
            { 0.007f, 0.013f, 0.019f, 0.027f, 0.035f, 0.044f },
            { 0.005f, 0.011f, 0.023f, 0.031f, 0.047f, 0.061f },
            { 0.009f, 0.015f, 0.021f, 0.037f, 0.052f, 0.079f }
        };
        const int which = clampInt (type - 7, 0, 2);
        const float scale = 0.5f + spec.p[0] / 99.0f * 1.5f;
        const int pre = (int) (spec.p[1] / 99.0f * 0.1f * sampleRate);
        for (int i = 0; i < n; ++i)
        {
            writeDelay (buf, w, 0.5f * (L[i] + R[i]));
            float eL = 0.0f, eR = 0.0f;
            for (int t = 0; t < 6; ++t)
            {
                const int d = std::max (1, pre + (int) (taps[which][t] * scale * sampleRate));
                const float g = 0.9f - t * 0.12f;
                ((t & 1) ? eR : eL) += g * readDelay (buf, w, d);
                ((t & 1) ? eL : eR) += 0.4f * g * readDelay (buf, w, d + 37);
            }
            wetL[(size_t) i] = eL * 0.8f;
            wetR[(size_t) i] = eR * 0.8f;
        }
    }

    // -------------------------------------------------------------- delay

    void renderDelay (const float* L, const float* R, int n, bool cross)
    {
        const int dL = delaySamplesFromKnob (spec.p[0], sampleRate);
        const int dR = delaySamplesFromKnob (spec.p[1] > 0 ? spec.p[1] : spec.p[0], sampleRate);
        const float fb = std::min (0.95f, spec.p[2] / 99.0f);
        const float damp = spec.p[3] / 99.0f;
        for (int i = 0; i < n; ++i)
        {
            const float l = readDelay (buf, w, dL);
            const float r = readDelay (bufR, wR, dR);
            low += (1.0f - damp) * (l - low);
            lowR += (1.0f - damp) * (r - lowR);
            const float dl = l - damp * (l - low);
            const float dr = r - damp * (r - lowR);
            wetL[(size_t) i] = dl;
            wetR[(size_t) i] = dr;
            if (cross)
            {
                writeDelay (buf, w, R[i] + dr * fb);
                writeDelay (bufR, wR, L[i] + dl * fb);
            }
            else
            {
                writeDelay (buf, w, L[i] + dl * fb);
                writeDelay (bufR, wR, R[i] + dr * fb);
            }
        }
    }

    // ---------------------------------------------------- modulated delays

    void renderChorus (const float* L, const float* R, int n, int voices)
    {
        const double depth = spec.p[0] / 99.0 * 0.005 * sampleRate;
        const double speed = knobHz (spec.p[1], 0.08, 6.0);
        const double base = (0.006 + spec.p[2] / 99.0 * 0.022) * sampleRate;
        const bool triangle = spec.p[3] >= 50;
        const float norm = 1.0f / (float) voices;
        for (int i = 0; i < n; ++i)
        {
            writeDelay (buf, w, L[i]);
            writeDelay (bufR, wR, R[i]);
            float accL = 0.0f, accR = 0.0f;
            for (int v = 0; v < voices; ++v)
            {
                const double off = v / (double) voices;
                accL += readFrac (buf, w, base + depth * (0.5 + 0.5 * lfo (phase + off, triangle)));
                accR += readFrac (bufR, wR, base + depth * (0.5 + 0.5 * lfo (phase + off + 0.25, triangle)));
            }
            phase += speed / sampleRate;
            if (phase >= 1.0) phase -= 1.0;
            wetL[(size_t) i] = accL * norm;
            wetR[(size_t) i] = accR * norm;
        }
    }

    void renderEnsemble (const float* L, const float* R, int n)
    {
        const double depth = (0.0008 + spec.p[0] / 99.0 * 0.004) * sampleRate;
        const double base = 0.012 * sampleRate;
        const double vib = 0.00025 * sampleRate;
        for (int i = 0; i < n; ++i)
        {
            const float mono = 0.5f * (L[i] + R[i]);
            writeDelay (buf, w, mono);
            float accL = 0.0f, accR = 0.0f;
            for (int v = 0; v < 3; ++v)
            {
                const double off = v / 3.0;
                const double fast = vib * lfo (phase2 + off * 0.7, false);
                accL += readFrac (buf, w, base + depth * (0.5 + 0.5 * lfo (phase + off, false)) + fast);
                accR += readFrac (buf, w, base + depth * (0.5 + 0.5 * lfo (phase + off + 1.0 / 6.0, false)) - fast);
            }
            phase += 0.55 / sampleRate;
            phase2 += 6.1 / sampleRate;
            if (phase >= 1.0) phase -= 1.0;
            if (phase2 >= 1.0) phase2 -= 1.0;
            wetL[(size_t) i] = accL * 0.45f;
            wetR[(size_t) i] = accR * 0.45f;
        }
    }

    void renderFlanger (const float* L, const float* R, int n, bool cross)
    {
        const double depth = spec.p[0] / 99.0 * 0.004 * sampleRate;
        const double speed = knobHz (spec.p[1], 0.03, 4.0);
        const double base = (0.0003 + spec.p[2] / 99.0 * 0.004) * sampleRate;
        const float fb = (spec.p[3] - 50) / 55.0f;
        const bool triangle = spec.p[4] >= 50;
        for (int i = 0; i < n; ++i)
        {
            const double dl = base + depth * (0.5 + 0.5 * lfo (phase, triangle));
            const double dr = base + depth * (0.5 + 0.5 * lfo (phase + (cross ? 0.5 : 0.0), triangle));
            const float l = readFrac (buf, w, dl);
            const float r = readFrac (bufR, wR, dr);
            wetL[(size_t) i] = l;
            wetR[(size_t) i] = r;
            if (cross)
            {
                writeDelay (buf, w, L[i] + r * fb);
                writeDelay (bufR, wR, R[i] + l * fb);
            }
            else
            {
                writeDelay (buf, w, L[i] + l * fb);
                writeDelay (bufR, wR, R[i] + r * fb);
            }
            phase += speed / sampleRate;
            if (phase >= 1.0) phase -= 1.0;
        }
    }

    void renderPhaser (const float* L, const float* R, int n, bool wide)
    {
        const float manual = spec.p[0] / 99.0f;
        const double speed = knobHz (spec.p[1], 0.03, 5.0);
        const float depth = spec.p[2] / 99.0f;
        const float fb = (spec.p[3] - 50) / 60.0f;
        const bool triangle = spec.p[4] >= 50;
        for (int i = 0; i < n; ++i)
        {
            auto coef = [&] (double ph)
            {
                const float sweep = 0.5f + 0.5f * lfo (ph, triangle);
                const float pos = std::max (0.0f, std::min (1.0f, 0.15f + 0.6f * manual + 0.4f * depth * (sweep - 0.5f)));
                const double hz = 100.0 * std::pow (60.0, (double) pos);
                const double t = std::tan (3.141592653589793 * std::min (hz, sampleRate * 0.45) / sampleRate);
                return (float) ((t - 1.0) / (t + 1.0));
            };
            const float aL = coef (phase);
            const float aR = coef (phase + (wide ? 0.5 : 0.25));
            float x = L[i] + apOut * fb;
            float y = R[i] + apOutR * fb;
            for (int s = 0; s < 4; ++s)
            {
                const float o = aL * x + ap[s];
                ap[s] = x - aL * o;
                x = o;
                const float oR = aR * y + apR[s];
                apR[s] = y - aR * oR;
                y = oR;
            }
            apOut = x;
            apOutR = y;
            wetL[(size_t) i] = x;
            wetR[(size_t) i] = y;
            phase += speed / sampleRate;
            if (phase >= 1.0) phase -= 1.0;
        }
    }

    void renderTremolo (const float* L, const float* R, int n, bool invert)
    {
        const float depth = spec.p[0] / 99.0f;
        const double speed = knobHz (spec.p[1], 0.2, 12.0);
        const bool triangle = spec.p[2] >= 50;
        const float shape = spec.p[3] / 99.0f;
        for (int i = 0; i < n; ++i)
        {
            auto gain = [&] (double ph)
            {
                float m = 0.5f + 0.5f * lfo (ph, triangle);
                if (shape > 0.0f)
                    m = std::max (0.0f, std::min (1.0f, (m - 0.5f) * (1.0f + shape * 6.0f) + 0.5f));
                return 1.0f - depth * m;
            };
            wetL[(size_t) i] = L[i] * gain (phase);
            wetR[(size_t) i] = R[i] * gain (invert ? phase + 0.5 : phase);
            phase += speed / sampleRate;
            if (phase >= 1.0) phase -= 1.0;
        }
    }

    // ------------------------------------------------------ static effects

    void renderEq (const float* L, const float* R, int n)
    {
        const float gLow = std::pow (10.0f, eqDb (spec.p[0]) / 20.0f) - 1.0f;
        const float gHigh = std::pow (10.0f, eqDb (spec.p[4]) / 20.0f) - 1.0f;
        static const float lowHz[3] = { 250, 500, 1000 };
        static const float highHz[3] = { 1000, 2000, 4000 };
        const float lFc = lowHz[clampInt (spec.p[1] / 34, 0, 2)];
        const float hFc = highHz[clampInt (spec.p[5] / 34, 0, 2)];
        const float gl = (float) (1.0 - std::exp (-2.0 * 3.141592653589793 * lFc / sampleRate));
        const float gh = (float) (1.0 - std::exp (-2.0 * 3.141592653589793 * hFc / sampleRate));
        for (int i = 0; i < n; ++i)
        {
            low += gl * (L[i] - low);
            lowR += gl * (R[i] - lowR);
            high += gh * (L[i] - high);
            highR += gh * (R[i] - highR);
            wetL[(size_t) i] = L[i] + low * gLow + (L[i] - high) * gHigh;
            wetR[(size_t) i] = R[i] + lowR * gLow + (R[i] - highR) * gHigh;
        }
    }

    void renderDrive (const float* L, const float* R, int n, bool hard)
    {
        const float drive = 1.0f + spec.p[0] / 99.0f * (hard ? 30.0f : 8.0f);
        const float level = 0.25f + spec.p[1] / 99.0f * 0.75f;
        const float tone = (float) (1.0 - std::exp (-2.0 * 3.141592653589793 * (hard ? 5500.0 : 7500.0) / sampleRate));
        const float pre = (float) (1.0 - std::exp (-2.0 * 3.141592653589793 * 120.0 / sampleRate));
        const float norm = 1.0f / std::tanh (drive * 0.6f);
        for (int i = 0; i < n; ++i)
        {
            auto shape = [&] (float x)
            {
                if (hard)
                    return std::tanh (x * drive) * norm * 0.8f;
                const float y = x * drive * 0.6f;
                return (y / (1.0f + std::abs (y))) * norm * 1.2f;
            };
            xLow += pre * (L[i] - xLow);
            xLowR += pre * (R[i] - xLowR);
            low += tone * (shape (L[i] - xLow * 0.5f) - low);
            lowR += tone * (shape (R[i] - xLowR * 0.5f) - lowR);
            wetL[(size_t) i] = low * level;
            wetR[(size_t) i] = lowR * level;
        }
    }

    void renderExciter (const float* L, const float* R, int n)
    {
        const float blend = spec.p[0] / 99.0f;
        const float emph = spec.p[1] / 99.0f;
        const float g = (float) (1.0 - std::exp (-2.0 * 3.141592653589793 * (2000.0 + 3000.0 * emph) / sampleRate));
        for (int i = 0; i < n; ++i)
        {
            high += g * (L[i] - high);
            highR += g * (R[i] - highR);
            const float topL = L[i] - high;
            const float topR = R[i] - highR;
            wetL[(size_t) i] = L[i] + blend * 0.7f * std::tanh (topL * 4.0f);
            wetR[(size_t) i] = R[i] + blend * 0.7f * std::tanh (topR * 4.0f);
        }
    }

    void renderRotary (const float* L, const float* R, int n)
    {
        const double depth = 0.25 + spec.p[0] / 99.0 * 0.75;
        const bool fast = spec.p[1] >= 50;
        const double hornTarget = fast ? 6.8 : 0.8;
        const double drumTarget = fast ? 5.9 : 0.65;
        if (rotorHorn < 0.0)
        {
            rotorHorn = hornTarget;
            rotorDrum = drumTarget;
        }
        const double hornRamp = 1.0 - std::exp (-1.0 / (0.8 * sampleRate));
        const double drumRamp = 1.0 - std::exp (-1.0 / (3.0 * sampleRate));
        const float split = (float) (1.0 - std::exp (-2.0 * 3.141592653589793 * 800.0 / sampleRate));
        const double hornSwing = 0.00035 * sampleRate * depth;
        const double drumSwing = 0.0006 * sampleRate * depth;
        for (int i = 0; i < n; ++i)
        {
            rotorHorn += (hornTarget - rotorHorn) * hornRamp;
            rotorDrum += (drumTarget - rotorDrum) * drumRamp;
            phase += rotorHorn / sampleRate;
            phase2 += rotorDrum / sampleRate;
            if (phase >= 1.0) phase -= 1.0;
            if (phase2 >= 1.0) phase2 -= 1.0;
            const float in = 0.5f * (L[i] + R[i]);
            xLow += split * (in - xLow);
            writeDelay (buf, w, in - xLow);
            writeDelay (bufR, wR, xLow);
            const float hs = (float) std::sin (phase * 6.283185307179586);
            const float hc = (float) std::cos (phase * 6.283185307179586);
            const float ds = (float) std::sin (phase2 * 6.283185307179586);
            const float hornL = readFrac (buf, w, 40.0 + hornSwing * (1.0 + hs)) * (1.0f - 0.35f * (float) depth * hc);
            const float hornR = readFrac (buf, w, 40.0 + hornSwing * (1.0 - hs)) * (1.0f + 0.35f * (float) depth * hc);
            const float drum = readFrac (bufR, wR, 60.0 + drumSwing * (1.0 + ds)) * (1.0f - 0.2f * (float) depth * ds);
            wetL[(size_t) i] = hornL * 0.9f + drum;
            wetR[(size_t) i] = hornR * 0.9f + drum;
        }
    }

    // Delay into a second effect, in one slot. P1/P2 are the delay time and
    // feedback; P3/P4 are the partner's first two parameters.
    void renderCombo (const float* L, const float* R, int n, int type)
    {
        if (inner == nullptr || n > (int) scratchL.size())
        {
            for (int i = 0; i < n; ++i) { wetL[(size_t) i] = 0.0f; wetR[(size_t) i] = 0.0f; }
            return;
        }
        const Fx saved = spec;
        spec.p[0] = saved.p[0];
        spec.p[1] = std::min (99, saved.p[0] + 6);
        spec.p[2] = saved.p[1];
        spec.p[3] = 30;
        renderDelay (L, R, n, false);
        spec = saved;

        for (int i = 0; i < n; ++i)
        {
            scratchL[(size_t) i] = L[i] + wetL[(size_t) i];
            scratchR[(size_t) i] = R[i] + wetR[(size_t) i];
        }
        Fx partner;
        partner.balance = 98;
        for (int k = 0; k < 8; ++k)
            partner.p[k] = 50;
        partner.p[0] = saved.p[2];
        partner.p[1] = saved.p[3];
        static const int partnerType[8] = { 1, 4, 7, 10, 12, 14, 16, 18 };
        partner.type = partnerType[clampInt (type - 26, 0, 7)];
        if (partner.type == 1 || partner.type == 4)
        {
            partner.p[1] = 10;
            partner.p[2] = 40;
            partner.p[3] = saved.p[3];
        }
        inner->set (partner);
        inner->renderWetOnly (scratchL.data(), scratchR.data(), n);
        const float* pl = inner->wetLeft();
        const float* pr = inner->wetRight();
        const bool modulated = partner.type >= 12;
        for (int i = 0; i < n; ++i)
        {
            if (modulated)
            {
                wetL[(size_t) i] = pl[i];
                wetR[(size_t) i] = pr[i];
            }
            else
            {
                wetL[(size_t) i] = wetL[(size_t) i] + pl[i];
                wetR[(size_t) i] = wetR[(size_t) i] + pr[i];
            }
        }
    }

    static float eqDb (int v) { return (clampInt (v, 0, 99) - 50) / 50.0f * 12.0f; }

    Fx spec {};
    double sampleRate = 48000.0;
    std::vector<float> buf, bufR, wetL, wetR, scratchL, scratchR;
    std::vector<float> comb[kCombs], combR[kCombs];
    std::vector<float> allpass[kAllpasses], allpassR[kAllpasses];
    int w = 0, wR = 0;
    int cw[kCombs] = {}, cwR[kCombs] = {}, aw[kAllpasses] = {}, awR[kAllpasses] = {};
    float combLp[kCombs] = {}, combLpR[kCombs] = {};
    float ap[4] = {}, apR[4] = {}, apOut = 0.0f, apOutR = 0.0f;
    float low = 0, lowR = 0, high = 0, highR = 0, xLow = 0, xLowR = 0;
    double phase = 0, phase2 = 0;
    double rotorHorn = -1.0, rotorDrum = -1.0;
    std::unique_ptr<FxUnit> inner;
};

class EffectRack
{
public:
    void prepare (double sr, int maxBlock)
    {
        block = std::max (maxBlock, 1);
        a.prepare (sr, block);
        b.prepare (sr, block);
        wetAL.assign ((size_t) block, 0.0f);
        wetAR.assign ((size_t) block, 0.0f);
        wetBL.assign ((size_t) block, 0.0f);
        wetBR.assign ((size_t) block, 0.0f);
    }

    void reset() { a.reset(); b.reset(); }

    void set (Fx fx1, Fx fx2, int placementIn)
    {
        fx2.type = resolvedPartner (fx1.type, fx2.type);
        if (fx1.type != a.specNow().type)
            a.reset();
        if (fx2.type != b.specNow().type)
            b.reset();
        a.set (fx1);
        b.set (fx2);
        placement = placementIn;
        resolved2 = fx2.type;
    }

    int secondType() const { return resolved2; }

    void process (float* L, float* R, int n)
    {
        if (n > block || n <= 0)
            return;
        if (placement == (int) Placement::Parallel)
        {
            a.renderWetInto (L, R, wetAL.data(), wetAR.data(), n);
            b.renderWetInto (L, R, wetBL.data(), wetBR.data(), n);
            for (int i = 0; i < n; ++i)
            {
                L[i] = L[i] + wetAL[(size_t) i] + wetBL[(size_t) i];
                R[i] = R[i] + wetAR[(size_t) i] + wetBR[(size_t) i];
            }
        }
        else
        {
            a.process (L, R, n);
            b.process (L, R, n);
        }
    }

    static double tailSeconds (int type1, int type2)
    {
        if (effectHasLongTail (type1) || effectHasLongTail (type2))
            return 8.0;
        return 0.1;
    }

private:
    FxUnit a, b;
    int placement = 0;
    int resolved2 = 0;
    int block = 0;
    std::vector<float> wetAL, wetAR, wetBL, wetBR;
};

} // namespace m1
