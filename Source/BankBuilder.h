#pragma once

// Builds the built-in multisample bank. Included at the end of Bank.h.
//
// Every multisound is synthesised here, so no third-party PCM is involved.
// Each one is described by a Recipe: a list of partials (frequency ratio,
// steady amplitude, attack-peak amplitude, decay time), plus optional detuned
// copies, a noise transient and a looped breath-noise band.
//
// A zone is rendered at one root per octave, band-limited for that root, as
//
//     [ attack region  A samples ][ loop region L samples ]
//
// The loop region is an inverse FFT, so every component sits on an exact FFT
// bin and the loop is seamless by construction. The attack region is the same
// loop signal plus the "excess" of each partial (peak − steady) decaying to
// exactly zero at the loop start, so the join is continuous. Decaying excess
// partials use exact (unquantised) frequencies, which gives natural beating
// and inharmonicity during the attack.

#include <algorithm>
#include <complex>
#include <functional>
#include <random>

namespace m1
{
namespace synth
{

constexpr double kRate = 44100.0;
constexpr double kTwoPi = 6.283185307179586;
constexpr double kMaxHz = 16000.0;

struct Partial
{
    double ratio = 1.0;   // frequency relative to the zone fundamental
    double steady = 0.0;  // amplitude held through the loop
    double peak = 0.0;    // amplitude at the attack
    double tau = 0.3;     // seconds for the excess (peak - steady) to decay
};

using SpectrumFn = std::function<void (double f0, std::vector<Partial>&)>;

struct Recipe
{
    SpectrumFn spectrum;
    int lo = 0, hi = 127;           // key range of the multisound
    double attackSec = 0.04;        // attack region length at C4
    double attackPitchScale = 0.0;  // attack length *= 2^(-octavesFromC4 * scale)
    double onsetMs = 1.5;           // fade-in at the very start
    int copies = 1;                 // detuned copies of every partial
    double spreadCents = 0.0;       // total detune spread across the copies
    int loopBits = 12;              // loop length = 1 << loopBits samples
    double noise = 0.0;             // noise transient, relative to the tonal peak
    double noiseMs = 15.0;
    double noiseLo = 300.0, noiseHi = 6000.0;
    double breath = 0.0;            // looped noise band, relative to the tonal RMS
    double breathLo = 400.0, breathHi = 4000.0;
    bool oneShot = false;           // no loop: the sound decays to silence
};

// ---------------------------------------------------------------- utilities

inline void fft (std::vector<std::complex<double>>& a, bool inverse)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = kTwoPi / (double) len * (inverse ? 1.0 : -1.0);
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k];
                const auto v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

inline double lp (double f, double fc, double order = 1.0)
{
    return 1.0 / std::sqrt (1.0 + std::pow (f / fc, 2.0 * order));
}

inline double hp (double f, double fc, double order = 1.0)
{
    return 1.0 / std::sqrt (1.0 + std::pow (fc / std::max (1.0, f), 2.0 * order));
}

// A smooth bump centred on `centre` Hz, `width` octaves wide.
inline double bump (double f, double centre, double width)
{
    const double d = std::log2 (std::max (1.0, f) / centre) / width;
    return std::exp (-d * d);
}

struct Formant { double hz, gain, width; };

inline double formants (double f, std::initializer_list<Formant> list, double floor = 0.04)
{
    double s = floor;
    for (const auto& fm : list)
        s += fm.gain * bump (f, fm.hz, fm.width);
    return s;
}

struct Noise
{
    uint32_t s = 1;
    double next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (s / 4294967295.0) * 2.0 - 1.0;
    }
};

// ------------------------------------------------------------ zone renderer

inline Zone renderZone (const Recipe& r, int rootNote, int lo, int hi, uint32_t seed)
{
    Zone z;
    z.pcmRate = kRate;
    z.lo = lo;
    z.hi = hi;

    const double fTarget = 440.0 * std::pow (2.0, (rootNote - 69) / 12.0);
    const int L = r.oneShot ? 4096 : (1 << r.loopBits);
    const double binHz = kRate / L;
    // Fundamental on an exact bin. The tune field corrects the small error.
    const int K = std::max (1, (int) std::lround (fTarget / binHz));
    const double f0 = r.oneShot ? fTarget : K * binHz;
    const double exactMidi = 69.0 + 12.0 * std::log2 (f0 / 440.0);
    z.root = rootNote;
    z.tuneCents = (int) std::lround ((rootNote - exactMidi) * 100.0);

    std::vector<Partial> parts;
    r.spectrum (f0, parts);

    std::mt19937 rng (seed);
    std::uniform_real_distribution<double> uni (0.0, kTwoPi);

    struct Comp { double hz, steady, excess, tau, phase; };
    std::vector<Comp> comps;
    const int copies = std::max (1, r.copies);
    const double copyGain = 1.0 / std::sqrt ((double) copies);
    for (const auto& p : parts)
        for (int c = 0; c < copies; ++c)
        {
            const double cents = copies > 1 ? r.spreadCents * ((double) c / (copies - 1) - 0.5) : 0.0;
            const double hz = p.ratio * f0 * std::pow (2.0, cents / 1200.0);
            if (hz <= 15.0 || hz > kMaxHz)
                continue;
            comps.push_back ({ hz, p.steady * copyGain, (p.peak - p.steady) * copyGain,
                               std::max (0.002, p.tau), uni (rng) });
        }

    const double octFromC4 = (rootNote - 60) / 12.0;
    const double attackSec = r.attackSec * std::pow (2.0, -octFromC4 * r.attackPitchScale);
    const int A = std::max ((int) (r.onsetMs * 0.001 * kRate * 2.0) + 8,
                            (int) std::lround (std::min (2.0, attackSec) * kRate));

    std::vector<double> out ((size_t) (A + (r.oneShot ? 0 : L)), 0.0);

    // Loop signal, periodic in L.
    std::vector<double> loop ((size_t) L, 0.0);
    double tonalRms = 0.0;
    if (! r.oneShot)
    {
        std::vector<std::complex<double>> X ((size_t) L);
        for (const auto& c : comps)
        {
            if (std::abs (c.steady) < 1.0e-7)
                continue;
            const int b = (int) std::lround (c.hz / binHz);
            if (b <= 0 || b >= L / 2)
                continue;
            X[(size_t) b] += 0.5 * c.steady * std::polar (1.0, c.phase);
            tonalRms += 0.5 * c.steady * c.steady;
        }
        tonalRms = std::sqrt (tonalRms);
        if (r.breath > 0.0)
        {
            const int b0 = std::max (1, (int) (r.breathLo / binHz));
            const int b1 = std::min (L / 2 - 1, (int) (r.breathHi / binHz));
            if (b1 > b0)
            {
                const double centre = std::sqrt (r.breathLo * r.breathHi);
                const double width = std::max (0.3, std::log2 (r.breathHi / r.breathLo) * 0.5);
                double energy = 0.0;
                std::vector<double> mag ((size_t) (b1 - b0 + 1));
                for (int b = b0; b <= b1; ++b)
                {
                    mag[(size_t) (b - b0)] = bump (b * binHz, centre, width);
                    energy += 0.5 * mag[(size_t) (b - b0)] * mag[(size_t) (b - b0)];
                }
                const double g = r.breath * std::max (tonalRms, 0.05) / std::sqrt (std::max (1.0e-12, energy));
                for (int b = b0; b <= b1; ++b)
                    X[(size_t) b] += 0.5 * g * mag[(size_t) (b - b0)] * std::polar (1.0, uni (rng));
            }
        }
        for (int b = 1; b < L / 2; ++b)
            X[(size_t) (L - b)] = std::conj (X[(size_t) b]);
        fft (X, true);
        for (int i = 0; i < L; ++i)
            loop[(size_t) i] = 2.0 * X[(size_t) i].real();
        for (int i = 0; i < A + L; ++i)
            out[(size_t) i] = loop[(size_t) (i % L)];
    }

    // Attack excess: decays to exactly zero at sample A. Only the strongest
    // partials are rendered, and each one stops once it is inaudible.
    double peakSum = 0.0;
    std::vector<const Comp*> excess;
    for (const auto& c : comps)
        if (std::abs (c.excess) > 1.0e-6)
        {
            peakSum += std::abs (c.excess + c.steady);
            excess.push_back (&c);
        }
    std::sort (excess.begin(), excess.end(), [] (const Comp* a, const Comp* b)
               { return std::abs (a->excess) > std::abs (b->excess); });
    if (excess.size() > 48)
        excess.resize (48);
    const double floorLevel = 2.0e-5 * std::max (1.0e-3, peakSum);
    for (const Comp* cp : excess)
    {
        const Comp& c = *cp;
        const double d = std::exp (-1.0 / (c.tau * kRate));
        const double endLevel = std::pow (d, (double) A);
        const double norm = c.excess / std::max (1.0e-9, 1.0 - endLevel);
        const double w = kTwoPi * c.hz / kRate;
        const double cr = std::cos (w), ci = std::sin (w);
        double env = 1.0, re = 0.0, im = 0.0;
        for (int i = 0; i < A; ++i)
        {
            if ((i & 1023) == 0)
            {
                if (std::abs ((env - endLevel) * norm) < floorLevel)
                    break;
                re = std::cos (c.phase + w * i);
                im = std::sin (c.phase + w * i);
            }
            out[(size_t) i] += (env - endLevel) * norm * re;
            env *= d;
            const double nr = re * cr - im * ci;
            im = re * ci + im * cr;
            re = nr;
        }
    }

    // Noise transient (pick, hammer, chiff, key click).
    if (r.noise > 0.0)
    {
        Noise n;
        n.s = seed * 2654435761u + 1u;
        const double gLo = 1.0 - std::exp (-kTwoPi * r.noiseLo / kRate);
        const double gHi = 1.0 - std::exp (-kTwoPi * std::min (18000.0, r.noiseHi) / kRate);
        double low = 0.0, hi1 = 0.0, hi2 = 0.0;
        const double d = std::exp (-1.0 / (r.noiseMs * 0.001 * kRate));
        const double endLevel = std::pow (d, (double) A);
        double env = 1.0;
        const double level = r.noise * std::max (0.3, std::max (peakSum, tonalRms * 1.4));
        for (int i = 0; i < A; ++i)
        {
            const double x = n.next();
            low += gLo * (x - low);
            const double band = x - low;
            hi1 += gHi * (band - hi1);
            hi2 += gHi * (hi1 - hi2);
            out[(size_t) i] += level * 2.0 * hi2 * (env - endLevel) / (1.0 - endLevel);
            env *= d;
        }
    }

    // Onset fade.
    const int onset = std::max (1, (int) (r.onsetMs * 0.001 * kRate));
    for (int i = 0; i < onset && i < (int) out.size(); ++i)
        out[(size_t) i] *= 0.5 - 0.5 * std::cos (3.141592653589793 * i / onset);

    double peak = 0.0;
    for (double v : out)
        peak = std::max (peak, std::abs (v));
    const double scale = peak > 1.0e-9 ? 0.89 / peak : 0.0;
    z.pcm.resize (out.size());
    for (size_t i = 0; i < out.size(); ++i)
        z.pcm[i] = (int16_t) std::lrint (std::max (-1.0, std::min (1.0, out[i] * scale)) * 32767.0);
    z.loopStart = r.oneShot ? 0 : A;
    z.loopEnd = r.oneShot ? 0 : A + L;
    return z;
}

inline void renderMultisound (Multisample& m, const Recipe& r, int index)
{
    m.zones.clear();
    // One root per octave, each zone covering root-6 .. root+5.
    for (int root = 24; root <= 108; root += 12)
    {
        int zlo = root - 6, zhi = root + 5;
        if (root == 24) zlo = 0;
        if (root == 108) zhi = 127;
        zlo = std::max (zlo, r.lo);
        zhi = std::min (zhi, r.hi);
        if (zlo > zhi)
            continue;
        m.zones.push_back (renderZone (r, root, zlo, zhi, (uint32_t) (index * 977 + root * 31 + 7)));
    }
}

// ------------------------------------------------------- spectrum helpers

using Amp = std::function<double (int h, double hz)>;

// Harmonic partials with a held spectrum.
inline SpectrumFn held (Amp steady, Amp attack = nullptr, double tau = 0.08)
{
    return [steady, attack, tau] (double f0, std::vector<Partial>& out)
    {
        for (int h = 1; h <= 500 && h * f0 <= kMaxHz; ++h)
        {
            const double hz = h * f0;
            const double s = steady (h, hz);
            const double a = attack ? attack (h, hz) : s;
            if (std::abs (s) < 1.0e-5 && std::abs (a) < 1.0e-5)
                continue;
            out.push_back ({ (double) h, s, a, tau });
        }
    };
}

// Harmonic (optionally stretched) partials that strike and decay toward a
// quieter, darker held spectrum.
inline SpectrumFn struck (Amp peak, Amp steady, std::function<double (int, double)> tau, double stretch = 0.0)
{
    return [peak, steady, tau, stretch] (double f0, std::vector<Partial>& out)
    {
        for (int h = 1; h <= 120; ++h)
        {
            const double ratio = h * std::sqrt (1.0 + stretch * h * h);
            const double hz = ratio * f0;
            if (hz > kMaxHz)
                break;
            const double p = peak (h, hz);
            const double s = steady (h, hz);
            if (p < 1.0e-5 && s < 1.0e-5)
                continue;
            out.push_back ({ ratio, s, p, tau (h, hz) });
        }
    };
}

// Arbitrary (inharmonic) partial list: { ratio, peak, steady, tau }.
struct Mode { double ratio, peak, steady, tau; };

inline SpectrumFn modes (std::vector<Mode> list, double brightHz = kMaxHz)
{
    return [list, brightHz] (double f0, std::vector<Partial>& out)
    {
        for (const auto& m : list)
        {
            const double hz = m.ratio * f0;
            if (hz > kMaxHz)
                continue;
            const double g = lp (hz, brightHz, 1.0);
            out.push_back ({ m.ratio, m.steady * g, m.peak * g, m.tau });
        }
    };
}

inline double saw (int h) { return 1.0 / h; }
inline double square (int h) { return (h & 1) ? 1.0 / h : 0.0; }
inline double tri (int h) { return (h & 1) ? 1.0 / ((double) h * h) : 0.0; }
inline double pulse (int h, double w) { return std::abs (std::sin (3.141592653589793 * h * w)) / h; }

// ---------------------------------------------------------------- recipes

inline Recipe sustained (Amp steady, double attackSec = 0.04, double onsetMs = 2.0)
{
    Recipe r;
    r.spectrum = held (steady);
    r.attackSec = attackSec;
    r.onsetMs = onsetMs;
    return r;
}

inline Recipe ensemble (Amp steady, int copies, double cents, double onsetMs, double breath = 0.0)
{
    Recipe r;
    r.spectrum = held (steady);
    r.copies = copies;
    r.spreadCents = cents;
    r.loopBits = 15;
    r.attackSec = std::max (0.05, onsetMs * 0.002);
    r.onsetMs = onsetMs;
    r.breath = breath;
    return r;
}

inline Recipe noTransient (Recipe r)
{
    auto inner = r.spectrum;
    r.spectrum = [inner] (double f0, std::vector<Partial>& out)
    {
        inner (f0, out);
        for (auto& p : out)
            p.peak = p.steady;
    };
    r.noise = 0.0;
    r.attackSec = 0.03;
    r.oneShot = false;
    return r;
}

inline Recipe piano()
{
    Recipe r;
    r.lo = 21; r.hi = 108;
    r.spectrum = struck (
        [] (int h, double hz) { return (h % 8 == 0 ? 0.25 : 1.0) / std::pow ((double) h, 0.9) * lp (hz, 3200.0, 1.3); },
        [] (int h, double hz) { return 0.2 / std::pow ((double) h, 1.2) * lp (hz, 900.0, 1.5); },
        [] (int h, double) { return 1.6 / (1.0 + 0.45 * (h - 1)); },
        0.00018);
    r.attackSec = 1.3; r.attackPitchScale = 0.4;
    r.copies = 2; r.spreadCents = 1.4;
    r.loopBits = 13;
    r.noise = 0.05; r.noiseMs = 8.0; r.noiseLo = 200.0; r.noiseHi = 4000.0;
    r.onsetMs = 0.8;
    return r;
}

inline Recipe tineEp (double barkRatio, double bark, double body)
{
    Recipe r;
    r.lo = 21; r.hi = 108;
    r.spectrum = modes ({
        { 1.0, 1.0, 0.45, 1.4 },
        { 2.0, 0.35 * body, 0.05, 0.6 },
        { 3.0, 0.12 * body, 0.0, 0.25 },
        { barkRatio, bark, 0.0, 0.035 },
        { barkRatio * 2.03, bark * 0.35, 0.0, 0.02 },
    }, 7000.0);
    r.attackSec = 1.1; r.attackPitchScale = 0.4;
    r.loopBits = 12;
    r.noise = 0.02; r.noiseMs = 4.0; r.noiseLo = 1000.0; r.noiseHi = 8000.0;
    r.onsetMs = 0.6;
    return r;
}

inline Recipe pluck (double brightHz, double pickPos, double hold, double decaySec, double stretch = 0.0, double noise = 0.06)
{
    Recipe r;
    r.spectrum = struck (
        [brightHz, pickPos] (int h, double hz) {
            const double comb = std::abs (std::sin (3.141592653589793 * h * pickPos)) + 0.05;
            return comb / h * lp (hz, brightHz, 1.2);
        },
        [brightHz, hold] (int h, double hz) { return hold / h * lp (hz, brightHz * 0.3, 1.5); },
        [decaySec] (int h, double) { return decaySec / (1.0 + 0.3 * (h - 1)); },
        stretch);
    r.attackSec = std::min (1.6, decaySec * 1.2); r.attackPitchScale = 0.35;
    r.loopBits = 12;
    r.noise = noise; r.noiseMs = 5.0; r.noiseLo = 800.0; r.noiseHi = 9000.0;
    r.onsetMs = 0.5;
    return r;
}

inline Recipe mallet (std::vector<Mode> list, double attackSec, double brightHz = 9000.0, double noise = 0.04)
{
    Recipe r;
    r.spectrum = modes (std::move (list), brightHz);
    r.attackSec = attackSec * 0.8; r.attackPitchScale = 0.35;
    r.loopBits = 12;
    r.noise = noise; r.noiseMs = 3.0; r.noiseLo = 1000.0; r.noiseHi = 9000.0;
    r.onsetMs = 0.4;
    return r;
}

inline Recipe hit (std::vector<Mode> list, double seconds, double noise, double noiseMs, double nLo, double nHi)
{
    Recipe r;
    r.spectrum = modes (std::move (list));
    r.oneShot = true;
    r.attackSec = seconds; r.attackPitchScale = 0.3;
    r.noise = noise; r.noiseMs = noiseMs; r.noiseLo = nLo; r.noiseHi = nHi;
    r.onsetMs = 0.3;
    return r;
}

inline Recipe organ (std::vector<std::pair<double, double>> bars, double click)
{
    Recipe r;
    r.spectrum = [bars] (double, std::vector<Partial>& out)
    {
        for (const auto& b : bars)
            out.push_back ({ b.first, b.second, b.second, 0.05 });
    };
    r.loopBits = 13;
    r.attackSec = 0.03;
    r.noise = click; r.noiseMs = 4.0; r.noiseLo = 1500.0; r.noiseHi = 12000.0;
    r.onsetMs = 3.0;
    return r;
}

inline Recipe voice (std::initializer_list<Formant> f, int copies, double cents, double breath, double onsetMs)
{
    std::vector<Formant> list (f);
    Recipe r = ensemble ([list] (int h, double hz) {
        double s = 0.03;
        for (const auto& fm : list)
            s += fm.gain * bump (hz, fm.hz, fm.width);
        return s / std::pow ((double) h, 0.35) * lp (hz, 5000.0, 2.0);
    }, copies, cents, onsetMs, breath);
    r.breathLo = 600.0; r.breathHi = 5000.0;
    return r;
}

inline Recipe brass (double brightHz, int copies, double cents)
{
    Recipe r;
    r.spectrum = held (
        [brightHz] (int h, double hz) { return 1.0 / std::pow ((double) h, 0.75) * lp (hz, brightHz, 2.0) * hp (hz, 120.0); },
        [brightHz] (int h, double hz) { return 0.6 / std::pow ((double) h, 1.1) * lp (hz, brightHz * 0.4, 2.0); },
        0.09);
    r.copies = copies; r.spreadCents = cents;
    r.loopBits = copies > 1 ? 15 : 13;
    r.attackSec = 0.25;
    r.breath = 0.03; r.breathLo = 1000.0; r.breathHi = 6000.0;
    r.onsetMs = 18.0;
    return r;
}

inline Recipe reed (std::initializer_list<Formant> f, double oddBias, double breath)
{
    std::vector<Formant> list (f);
    Recipe r;
    r.spectrum = held ([list, oddBias] (int h, double hz) {
        double s = 0.05;
        for (const auto& fm : list)
            s += fm.gain * bump (hz, fm.hz, fm.width);
        return s * ((h & 1) ? 1.0 : 1.0 - oddBias) / std::pow ((double) h, 0.5) * lp (hz, 6000.0, 2.0);
    });
    r.loopBits = 15;
    r.copies = 2; r.spreadCents = 3.0;
    r.attackSec = 0.12;
    r.breath = breath; r.breathLo = 1200.0; r.breathHi = 7000.0;
    r.onsetMs = 25.0;
    return r;
}

inline Recipe flute (double breath, double chiff)
{
    Recipe r;
    r.spectrum = held ([] (int h, double hz) {
        static const double a[] = { 0, 1.0, 0.32, 0.14, 0.06, 0.03, 0.015 };
        return (h < 7 ? a[h] : 0.0) * lp (hz, 4000.0);
    });
    r.loopBits = 15;
    r.copies = 2; r.spreadCents = 2.0;
    r.attackSec = 0.15;
    r.breath = breath; r.breathLo = 800.0; r.breathHi = 7000.0;
    r.noise = chiff; r.noiseMs = 30.0; r.noiseLo = 1000.0; r.noiseHi = 6000.0;
    r.onsetMs = 30.0;
    return r;
}

inline Recipe strings (double brightHz, int copies, double cents, double onsetMs)
{
    Recipe r = ensemble ([brightHz] (int h, double hz) {
        const double body = formants (hz, { { 450.0, 0.7, 0.5 }, { 1400.0, 0.5, 0.6 }, { 3000.0, 0.4, 0.5 } }, 0.25);
        return body / std::pow ((double) h, 0.85) * lp (hz, brightHz, 2.0) * hp (hz, 150.0);
    }, copies, cents, onsetMs, 0.02);
    r.breathLo = 2000.0; r.breathHi = 9000.0;
    return r;
}

inline Recipe staticWave (Amp a, double brightHz = kMaxHz)
{
    return sustained ([a, brightHz] (int h, double hz) { return a (h, hz) * lp (hz, brightHz, 4.0); }, 0.02, 1.0);
}

inline Recipe recipeFor (int index)
{
    switch (index)
    {
        case 0:  return piano();
        case 1:  return tineEp (7.1, 0.55, 1.0);                                   // E Piano 1
        case 2:  return tineEp (13.9, 0.45, 0.6);                                  // E Piano 2
        case 3:  { auto r = pluck (5000.0, 0.12, 0.35, 0.5, 0.0, 0.08);            // Clav
                   r.spectrum = struck ([] (int h, double hz) { return pulse (h, 0.18) * lp (hz, 4500.0, 1.0) * hp (hz, 300.0); },
                                        [] (int h, double hz) { return 0.35 * pulse (h, 0.18) * lp (hz, 2500.0, 1.0); },
                                        [] (int h, double) { return 0.45 / (1.0 + 0.15 * h); });
                   return r; }
        case 4:  { auto r = pluck (9000.0, 0.08, 0.15, 0.9, 0.0, 0.1);             // Harpsichord
                   r.copies = 2; r.spreadCents = 3.0; r.loopBits = 13; return r; }
        case 5:  return organ ({ { 1, 0.8 }, { 2, 0.6 }, { 3, 0.5 }, { 4, 0.35 }, { 0.5, 0.5 } }, 0.04);
        case 6:  return organ ({ { 1, 0.7 }, { 2, 0.7 }, { 3, 0.6 }, { 4, 0.5 }, { 5, 0.3 }, { 6, 0.35 }, { 8, 0.4 } }, 0.05);
        case 7:  return organ ({ { 1, 0.9 }, { 0.5, 0.6 }, { 1.5, 0.4 }, { 2, 0.3 } }, 0.02);  // Magic organ
        case 8:  return pluck (4000.0, 0.15, 0.12, 1.1, 0.00005, 0.05);           // Guitar
        case 9:  return pluck (6500.0, 0.11, 0.15, 1.3, 0.00008, 0.08);           // Guitar 2
        case 10: { auto r = pluck (3500.0, 0.2, 0.4, 2.0, 0.0, 0.03);             // E Guitar
                   r.copies = 2; r.spreadCents = 2.0; r.loopBits = 13; return r; }
        case 11: { auto r = pluck (9000.0, 0.05, 0.5, 2.5, 0.0002, 0.05);         // Sitar
                   r.copies = 3; r.spreadCents = 8.0; r.loopBits = 14; return r; }
        case 12: return noTransient (recipeFor (11));
        case 13: { auto r = pluck (1800.0, 0.25, 0.3, 1.0, 0.0, 0.03); r.lo = 0; r.hi = 84; return r; }   // A Bass
        case 14: { auto r = pluck (3000.0, 0.1, 0.35, 1.2, 0.0, 0.12); r.hi = 84; return r; }             // Pick bass
        case 15: { auto r = pluck (1500.0, 0.3, 0.45, 1.4); r.hi = 84; return r; }                         // E Bass
        case 16: { auto r = pluck (1200.0, 0.35, 0.7, 1.4, 0.0, 0.0); r.hi = 84; return r; }               // Fretless
        case 17: return sustained ([] (int h, double hz) { return saw (h) * lp (hz, 900.0, 2.0); });      // Synth bass
        case 18: return sustained ([] (int h, double hz) { return square (h) * lp (hz, 1400.0, 2.0); });  // Synth bass 2
        case 19: return mallet ({ { 1, 1.0, 0.45, 1.5 }, { 4.0, 0.4, 0.05, 0.4 }, { 10.0, 0.25, 0.0, 0.08 } }, 1.6);   // Vibes
        case 20: return mallet ({ { 1, 1.0, 0.4, 2.0 }, { 2.76, 0.6, 0.18, 1.0 }, { 5.4, 0.4, 0.06, 0.5 }, { 8.93, 0.25, 0.0, 0.25 } }, 2.2);   // Bell
        case 21: return mallet ({ { 1, 0.6, 0.3, 2.0 }, { 2.0, 0.4, 0.2, 1.6 }, { 3.0, 0.5, 0.2, 1.2 }, { 4.16, 0.6, 0.15, 0.9 }, { 5.43, 0.4, 0.06, 0.6 } }, 2.2);   // Tubular
        case 22: return mallet ({ { 1, 1.0, 0.5, 2.0 }, { 1.5, 0.5, 0.3, 1.5 }, { 2.41, 0.4, 0.2, 1.0 }, { 3.53, 0.3, 0.1, 0.6 } }, 2.0);   // Bell ring
        case 23: return mallet ({ { 1, 1.0, 0.25, 0.6 }, { 6.2, 0.35, 0.0, 0.05 } }, 0.9);              // Kalimba
        case 24: return noTransient (recipeFor (23));
        case 25: return mallet ({ { 1, 1.0, 0.2, 0.35 }, { 3.0, 0.4, 0.04, 0.1 }, { 5.0, 0.2, 0.0, 0.05 } }, 0.6);   // Syn mallet
        case 26: return flute (0.10, 0.12);
        case 27: { auto r = flute (0.22, 0.3); r.noiseMs = 45.0; return r; }      // Pan flute
        case 28: { auto r = flute (0.3, 0.2); r.breathLo = 300.0; r.breathHi = 2500.0; return r; }   // Bottles
        case 29: return voice ({ { 300.0, 1.0, 0.25 }, { 870.0, 0.4, 0.25 }, { 2240.0, 0.25, 0.2 } }, 3, 14.0, 0.05, 60.0);   // Voices (ooh)
        case 30: return voice ({ { 730.0, 1.0, 0.25 }, { 1090.0, 0.7, 0.25 }, { 2440.0, 0.35, 0.2 } }, 3, 16.0, 0.06, 60.0);  // Choir (aah)
        case 31: return strings (7000.0, 3, 14.0, 40.0);
        case 32: return brass (2600.0, 3, 9.0);
        case 33: return brass (3800.0, 1, 0.0);
        case 34: return reed ({ { 500.0, 1.0, 0.4 }, { 1500.0, 0.6, 0.4 }, { 2700.0, 0.3, 0.3 } }, 0.2, 0.08);   // Tenor sax
        case 35: return reed ({ { 1500.0, 1.0, 0.25 }, { 3100.0, 0.7, 0.25 } }, 0.0, 0.03);    // Mute trumpet
        case 36: return brass (4500.0, 1, 0.0);                                    // Trumpet
        case 37: { auto r = brass (900.0, 1, 0.0); r.lo = 0; r.hi = 84; return r; }   // Tuba
        case 38: return reed ({ { 1100.0, 1.0, 0.3 }, { 2700.0, 0.6, 0.3 } }, 0.5, 0.04);      // Double reed
        case 39: return pluck (7000.0, 0.07, 0.15, 1.2, 0.0001, 0.1);             // Koto
        case 40: return hit ({ { 1, 1.0, 0, 0.12 }, { 2.76, 0.5, 0, 0.05 }, { 5.4, 0.3, 0, 0.03 } }, 0.4, 0.2, 6.0, 500.0, 5000.0);   // Bamboo
        case 41: return hit ({ { 1, 0.6, 0, 0.08 }, { 3.1, 0.6, 0, 0.04 } }, 0.3, 0.5, 8.0, 1500.0, 9000.0);     // Rhythm
        case 42: return ensemble ([] (int h, double hz) { return (1.0 / h) * bump (hz, 1200.0, 1.2) * (h == 1 ? 2.0 : 1.0); }, 3, 20.0, 120.0, 0.15);   // Lore
        case 43: return noTransient (recipeFor (42));
        case 44: return ensemble ([] (int h, double) { return h == 1 ? 1.0 : (h == 2 ? 0.1 : 0.0); }, 3, 40.0, 10.0);   // Flexatone
        case 45: return hit ({ { 1, 1.0, 0, 1.5 }, { 2.32, 0.6, 0, 1.0 }, { 4.25, 0.5, 0, 0.6 }, { 6.63, 0.3, 0, 0.4 } }, 2.0, 0.05, 3.0, 2000.0, 9000.0);   // Wind bells
        case 46: return mallet ({ { 1, 1.0, 0.25, 0.6 }, { 2.4, 0.5, 0.05, 0.3 }, { 4.4, 0.35, 0.0, 0.15 } }, 0.8);   // Pole
        case 47: return noTransient (recipeFor (46));
        case 48: return hit ({ { 1, 1.0, 0, 0.06 }, { 2.9, 0.4, 0, 0.03 } }, 0.25, 0.2, 3.0, 800.0, 6000.0);    // Block
        case 49: { auto r = mallet ({ { 1, 1.0, 0.3, 0.06 }, { 2.9, 0.4, 0.0, 0.03 } }, 0.2); return noTransient (r); }
        case 50: return hit ({ { 1, 0.1, 0, 0.01 } }, 0.15, 3.0, 12.0, 1500.0, 7000.0);                         // Finger snap
        case 51: return hit ({ { 1, 1.0, 0, 0.05 }, { 2, 0.3, 0, 0.02 } }, 0.15, 0.3, 2.0, 400.0, 3000.0);      // Pop
        case 52: return hit ({ { 1, 1.0, 0, 0.1 }, { 1.5, 0.3, 0, 0.05 } }, 0.3, 0.05, 2.0, 1000.0, 4000.0);    // Drop
        case 53: return noTransient (recipeFor (26));
        case 54: { Recipe r = sustained ([] (int h, double) { return h == 1 ? 0.15 : 0.0; }, 0.1, 40.0);   // Breath
                   r.loopBits = 15; r.breath = 4.0; r.breathLo = 500.0; r.breathHi = 5000.0; return r; }
        case 55: return noTransient (recipeFor (54));
        case 56: { auto r = pluck (5000.0, 0.1, 0.0, 0.5, 0.0, 0.08); r.oneShot = true; return r; }   // Pluck
        case 57: return noTransient (pluck (5000.0, 0.1, 0.3, 0.5));
        case 58: return mallet ({ { 1, 1.0, 0.0, 0.3 }, { 4.0, 0.4, 0.0, 0.1 }, { 10.0, 0.25, 0.0, 0.04 } }, 0.5);   // Vibe hit
        case 59: return noTransient (recipeFor (19));
        case 60: return hit ({ { 1, 1.0, 0, 0.2 }, { 1.7, 0.6, 0, 0.1 }, { 2.9, 0.4, 0, 0.05 } }, 0.5, 0.6, 5.0, 300.0, 6000.0);   // Hammer
        case 61: return hit ({ { 1, 1.0, 0, 0.6 }, { 1.47, 0.8, 0, 0.5 }, { 2.09, 0.6, 0, 0.4 }, { 2.56, 0.5, 0, 0.3 }, { 3.9, 0.4, 0, 0.2 } }, 1.0, 0.3, 4.0, 2000.0, 10000.0);   // Metal hit
        case 62: return mallet ({ { 1, 1.0, 0.3, 0.6 }, { 1.47, 0.8, 0.2, 0.5 }, { 2.09, 0.6, 0.1, 0.4 }, { 2.56, 0.5, 0.05, 0.3 } }, 0.6);
        case 63: return hit ({ { 1, 0.3, 0, 0.02 } }, 0.08, 2.0, 4.0, 1500.0, 9000.0);                          // Pick
        case 64: { auto r = sustained ([] (int h, double hz) { return 1.0 / std::pow ((double) h, 0.6) * lp (hz, 4500.0, 2.0) * hp (hz, 120.0); }, 0.05);
                   r.copies = 2; r.spreadCents = 10.0; r.loopBits = 15; r.noise = 0.1; r.noiseMs = 6.0; return r; }   // Distortion
        case 65: return noTransient (recipeFor (64));
        case 66: { auto r = pluck (6000.0, 0.08, 0.35, 1.0, 0.00005, 0.25); r.hi = 84; return r; }    // Bass thumb
        case 67: return noTransient (recipeFor (66));
        case 68: { auto r = pluck (4000.0, 0.15, 0.4, 0.9, 0.0, 0.2); r.hi = 84; return r; }
        case 69: return pluck (9000.0, 0.03, 0.3, 2.0, 0.0003, 0.05);             // Wire
        case 70: return staticWave ([] (int h, double) { return (h <= 8 ? 1.0 / h : 0.0) * (h % 3 == 0 ? 1.4 : 1.0); });   // Pan wave
        case 71: return staticWave ([] (int h, double hz) { return 0.3 / h + bump (hz, 2500.0, 0.3); });  // Ping wave
        case 72: return staticWave ([] (int h, double hz) { return formants (hz, { { 400, 1, 0.3 }, { 2000, 0.6, 0.3 } }) / std::sqrt ((double) h); });   // Fv wave
        case 73: return staticWave ([] (int h, double hz) { return formants (hz, { { 700, 1, 0.3 }, { 1200, 0.8, 0.3 } }) / std::sqrt ((double) h); });  // Mv wave
        case 74: return voice ({ { 600.0, 1.0, 0.3 }, { 1000.0, 0.6, 0.3 }, { 2600.0, 0.4, 0.25 } }, 1, 0.0, 0.0, 15.0);   // Voice wave
        case 75: return noTransient (recipeFor (74));
        case 76: return voice ({ { 400.0, 1.0, 0.3 }, { 1700.0, 0.5, 0.3 } }, 2, 8.0, 0.02, 20.0);
        case 77: return staticWave ([] (int h, double) { return h == 1 ? 1.0 : h == 2 ? 0.5 : h == 4 ? 0.25 : h == 7 ? 0.15 : 0.0; });  // DWGS EP
        case 78: return staticWave ([] (int h, double) { return h == 1 ? 1.0 : h == 3 ? 0.4 : h == 6 ? 0.2 : h == 11 ? 0.1 : 0.0; });
        case 79: return staticWave ([] (int h, double) { return h == 1 ? 1.0 : h == 2 ? 0.6 : h == 5 ? 0.3 : h == 9 ? 0.2 : 0.0; });
        case 80: return staticWave ([] (int h, double hz) { return 1.0 / std::pow ((double) h, 1.3) * lp (hz, 3000.0); });              // DWGS piano
        case 81: return staticWave ([] (int h, double hz) { return pulse (h, 0.15) * lp (hz, 5000.0); });                                 // DWGS clav
        case 82: return staticWave ([] (int h, double) { return h == 1 ? 1.0 : h == 4 ? 0.5 : h == 10 ? 0.2 : 0.0; });                    // DWGS vibe
        case 83: return staticWave ([] (int h, double) { return h <= 6 ? 1.0 / (h * h) * (h == 2 ? 3.0 : 1.0) : 0.0; });                 // DWGS bass
        case 84: return staticWave ([] (int h, double hz) { return saw (h) * lp (hz, 1500.0, 2.0) * (1.0 + 0.5 * (h == 3)); });
        case 85: return staticWave ([] (int h, double) { return h == 1 ? 1.0 : h == 3 ? 0.3 : h == 7 ? 0.5 : h == 13 ? 0.3 : 0.0; });    // DWGS bell
        case 86: return staticWave ([] (int h, double) { return h == 1 ? 1.0 : h == 2 ? 0.8 : h == 3 ? 0.6 : h == 4 ? 0.5 : h == 6 ? 0.4 : h == 8 ? 0.35 : 0.0; });
        case 87: return staticWave ([] (int h, double) { return h == 1 ? 1.0 : h == 2 ? 0.2 : h == 3 ? 0.7 : h == 5 ? 0.4 : h == 8 ? 0.3 : 0.0; });
        case 88: return staticWave ([] (int h, double hz) { return formants (hz, { { 650, 1, 0.3 }, { 1080, 0.7, 0.3 }, { 2650, 0.4, 0.3 } }) / std::sqrt ((double) h); });
        case 89: return staticWave ([] (int h, double) { return square (h); });
        case 90: return staticWave ([] (int h, double) { return (h & 1) ? 1.0 / std::sqrt ((double) h) : 0.3 / h; });                   // Digital
        case 91: return staticWave ([] (int h, double) { return saw (h); });
        case 92: return staticWave ([] (int h, double) { return 1.0 / std::pow ((double) h, 0.7) * (h % 4 == 0 ? 0.2 : 1.0); });         // Digital 2
        case 93: return staticWave ([] (int h, double) { return pulse (h, 0.25); });
        case 94: return staticWave ([] (int h, double) { return pulse (h, 0.10); });
        case 95: return staticWave ([] (int h, double) { return h == 1 ? 1.0 : (h >= 6 && h <= 10) ? 0.4 : 0.0; });                      // Digital 3
        case 96: return staticWave ([] (int h, double) { return (h == 1 || h == 2 || h == 4 || h == 8 || h == 16) ? 1.0 / std::sqrt ((double) h) : 0.0; });
        case 97: return staticWave ([] (int h, double hz) { return bump (hz, 1800.0, 0.6) + (h == 1 ? 0.6 : 0.0); });                    // Digital 5
        case 98: return staticWave ([] (int h, double) { return tri (h); });
        case 99: return staticWave ([] (int h, double) { return h == 1 ? 1.0 : 0.0; });
        default: break;
    }
    return staticWave ([] (int h, double) { return h == 1 ? 1.0 : 0.0; });
}

// ------------------------------------------------------------------ drums

struct DrumSynth
{
    std::vector<double> x;
    Noise noise;

    explicit DrumSynth (double seconds, uint32_t seed)
        : x ((size_t) std::max (16, (int) (seconds * kRate)), 0.0) { noise.s = seed * 747796405u + 1u; }

    int n() const { return (int) x.size(); }

    // Sine with an exponential pitch sweep and amplitude decay.
    void tone (double fStart, double fEnd, double sweepMs, double level, double decayMs)
    {
        double ph = 0.0;
        for (int i = 0; i < n(); ++i)
        {
            const double t = i / kRate;
            const double f = fEnd + (fStart - fEnd) * std::exp (-t * 1000.0 / sweepMs);
            ph += kTwoPi * f / kRate;
            x[(size_t) i] += level * std::sin (ph) * std::exp (-t * 1000.0 / decayMs);
        }
    }

    // Band-passed noise with an exponential decay.
    void hiss (double lo, double hi, double level, double decayMs, double holdMs = 0.0)
    {
        const double gLo = 1.0 - std::exp (-kTwoPi * lo / kRate);
        const double gHi = 1.0 - std::exp (-kTwoPi * std::min (19000.0, hi) / kRate);
        double l = 0, h1 = 0, h2 = 0;
        for (int i = 0; i < n(); ++i)
        {
            const double t = i / kRate * 1000.0;
            const double v = noise.next();
            l += gLo * (v - l);
            h1 += gHi * ((v - l) - h1);
            h2 += gHi * (h1 - h2);
            const double env = t < holdMs ? 1.0 : std::exp (-(t - holdMs) / decayMs);
            x[(size_t) i] += level * 2.5 * h2 * env;
        }
    }

    // Six detuned square waves, high-passed: cymbal and hat metal.
    void metal (double base, double hpHz, double level, double decayMs, double holdMs = 0.0)
    {
        static const double ratios[6] = { 1.0, 1.4823, 1.8005, 2.5463, 2.6302, 3.8970 };
        double ph[6] = {};
        const double g = 1.0 - std::exp (-kTwoPi * hpHz / kRate);
        double low1 = 0, low2 = 0;
        for (int i = 0; i < n(); ++i)
        {
            double s = 0.0;
            for (int k = 0; k < 6; ++k)
            {
                ph[k] += base * ratios[k] / kRate;
                ph[k] -= std::floor (ph[k]);
                s += ph[k] < 0.5 ? 1.0 : -1.0;
            }
            s /= 6.0;
            low1 += g * (s - low1);
            const double hp1 = s - low1;
            low2 += g * (hp1 - low2);
            const double t = i / kRate * 1000.0;
            const double env = t < holdMs ? 1.0 : std::exp (-(t - holdMs) / decayMs);
            x[(size_t) i] += level * (hp1 - low2) * env;
        }
    }

    // Short multi-burst noise for claps.
    void clap (double level)
    {
        const double g = 1.0 - std::exp (-kTwoPi * 1100.0 / kRate);
        const double g2 = 1.0 - std::exp (-kTwoPi * 2400.0 / kRate);
        double l = 0, b = 0;
        for (int i = 0; i < n(); ++i)
        {
            const double t = i / kRate * 1000.0;
            const double v = noise.next();
            l += g * (v - l);
            b += g2 * ((v - l) - b);
            double env = std::exp (-t / 110.0) * 0.6;
            for (int k = 0; k < 3; ++k)
            {
                const double tt = t - k * 9.0;
                if (tt >= 0.0 && tt < 9.0)
                    env = std::max (env, std::exp (-tt / 2.5));
            }
            x[(size_t) i] += level * 3.0 * b * env;
        }
    }

    void fadeTail (double ms = 10.0)
    {
        const int f = std::min (n(), (int) (ms * 0.001 * kRate));
        for (int i = 0; i < f; ++i)
            x[(size_t) (n() - 1 - i)] *= (double) i / f;
    }

    std::vector<int16_t> pcm (double gain = 0.89)
    {
        fadeTail();
        double peak = 0.0;
        for (double v : x)
            peak = std::max (peak, std::abs (v));
        std::vector<int16_t> out (x.size());
        const double s = peak > 0 ? gain / peak : 0.0;
        for (size_t i = 0; i < x.size(); ++i)
            out[i] = (int16_t) std::lrint (std::max (-1.0, std::min (1.0, x[i] * s)) * 32767.0);
        return out;
    }
};

inline const char* drumName (int index)
{
    static const char* names[kDrumSamples] = {
        "Kick", "Snare", "Closed Hat", "Open Hat", "Low Tom", "Clap", "Ride", "Mid Tom",
        "High Tom", "Kick 2", "Snare 2", "Rim", "Crash", "Pedal Hat", "Cowbell", "Tambourine",
        "Shaker", "Ride Bell", "High Conga", "Low Conga", "High Bongo", "Low Bongo", "Claves",
        "Splash", "Side Stick", "Timbale", "Agogo Hi", "Agogo Lo", "Cabasa", "Whistle",
        "Guiro", "Wood Block", "Triangle", "Kick 808", "Snare Brush", "Floor Tom", "Snap",
        "China", "E Hat", "Zap", "Noise Hit", "Metal", "Gated Snare", "Timpani"
    };
    return (index >= 0 && index < kDrumSamples) ? names[index] : "Empty";
}

inline std::vector<int16_t> drumSample (int index)
{
    const uint32_t seed = (uint32_t) (index * 7919 + 11);
    auto make = [seed] (double sec) { return DrumSynth (sec, seed); };
    switch (index)
    {
        case 0:  { auto d = make (0.55); d.tone (160, 48, 30, 1.0, 260); d.hiss (2000, 9000, 0.12, 4); return d.pcm(); }
        case 1:  { auto d = make (0.4); d.tone (230, 180, 20, 0.55, 90); d.tone (340, 330, 20, 0.25, 50); d.hiss (900, 9000, 0.7, 120); return d.pcm(); }
        case 2:  { auto d = make (0.18); d.metal (380, 7000, 1.0, 35); d.hiss (8000, 16000, 0.25, 30); return d.pcm (0.7); }
        case 3:  { auto d = make (0.8); d.metal (380, 7000, 1.0, 280); d.hiss (7000, 16000, 0.3, 250); return d.pcm (0.7); }
        case 4:  { auto d = make (0.7); d.tone (150, 95, 60, 1.0, 320); d.hiss (300, 3000, 0.15, 30); return d.pcm(); }
        case 5:  { auto d = make (0.45); d.clap (1.0); return d.pcm (0.8); }
        case 6:  { auto d = make (2.2); d.metal (510, 3000, 0.6, 900); d.hiss (5000, 14000, 0.2, 900); d.tone (2800, 2800, 1, 0.08, 600); return d.pcm (0.6); }
        case 7:  { auto d = make (0.6); d.tone (200, 130, 50, 1.0, 280); d.hiss (300, 3000, 0.15, 30); return d.pcm(); }
        case 8:  { auto d = make (0.5); d.tone (270, 180, 40, 1.0, 240); d.hiss (400, 4000, 0.15, 30); return d.pcm(); }
        case 9:  { auto d = make (0.35); d.tone (220, 55, 18, 1.0, 140); d.hiss (2500, 10000, 0.3, 3); return d.pcm(); }
        case 10: { auto d = make (0.3); d.tone (320, 260, 15, 0.45, 60); d.hiss (1500, 12000, 0.8, 70); return d.pcm(); }
        case 11: { auto d = make (0.12); d.tone (1700, 1600, 5, 0.6, 12); d.hiss (2000, 9000, 0.6, 10); return d.pcm(); }
        case 12: { auto d = make (2.8); d.metal (430, 2500, 0.7, 1100); d.hiss (3000, 16000, 0.6, 1000); return d.pcm (0.6); }
        case 13: { auto d = make (0.15); d.metal (380, 7000, 0.8, 25); d.hiss (6000, 14000, 0.2, 20); return d.pcm (0.55); }
        case 14: { auto d = make (0.4); d.tone (540, 540, 1, 0.6, 90); d.tone (800, 800, 1, 0.5, 70); return d.pcm (0.7); }
        case 15: { auto d = make (0.35); d.metal (900, 6000, 0.5, 90); d.hiss (6000, 15000, 0.6, 80, 10); return d.pcm (0.65); }
        case 16: { auto d = make (0.18); d.hiss (5000, 15000, 1.0, 40, 30); return d.pcm (0.55); }
        case 17: { auto d = make (1.6); d.tone (2400, 2400, 1, 0.5, 700); d.tone (3600, 3600, 1, 0.3, 500); d.metal (600, 3000, 0.3, 600); return d.pcm (0.6); }
        case 18: { auto d = make (0.35); d.tone (380, 330, 10, 1.0, 110); d.hiss (800, 5000, 0.2, 6); return d.pcm(); }
        case 19: { auto d = make (0.45); d.tone (260, 220, 10, 1.0, 160); d.hiss (600, 4000, 0.2, 6); return d.pcm(); }
        case 20: { auto d = make (0.25); d.tone (520, 470, 8, 1.0, 70); d.hiss (1000, 6000, 0.25, 4); return d.pcm(); }
        case 21: { auto d = make (0.3); d.tone (380, 340, 8, 1.0, 90); d.hiss (800, 5000, 0.25, 4); return d.pcm(); }
        case 22: { auto d = make (0.2); d.tone (2500, 2500, 1, 1.0, 40); return d.pcm (0.7); }
        case 23: { auto d = make (1.4); d.metal (520, 4000, 0.7, 450); d.hiss (4000, 16000, 0.6, 400); return d.pcm (0.6); }
        case 24: { auto d = make (0.15); d.tone (1200, 900, 5, 0.6, 18); d.hiss (1500, 7000, 0.5, 14); return d.pcm (0.8); }
        case 25: { auto d = make (0.6); d.tone (520, 480, 8, 0.8, 200); d.metal (900, 3000, 0.3, 200); return d.pcm(); }
        case 26: { auto d = make (0.5); d.tone (930, 930, 1, 0.8, 160); d.tone (2350, 2350, 1, 0.3, 90); return d.pcm (0.7); }
        case 27: { auto d = make (0.5); d.tone (660, 660, 1, 0.8, 170); d.tone (1660, 1660, 1, 0.3, 90); return d.pcm (0.7); }
        case 28: { auto d = make (0.2); d.hiss (4000, 14000, 1.0, 50, 15); return d.pcm (0.55); }
        case 29: { auto d = make (0.5); d.tone (2200, 2300, 50, 0.8, 1000); d.hiss (2000, 4000, 0.1, 400); return d.pcm (0.6); }
        case 30: { auto d = make (0.35); for (int k = 0; k < 12; ++k) { DrumSynth b (0.03, seed + k); b.hiss (2000, 8000, 1.0, 6); for (int i = 0; i < b.n() && k * 1100 + i < d.n(); ++i) d.x[(size_t) (k * 1100 + i)] += b.x[(size_t) i]; } return d.pcm (0.6); }
        case 31: { auto d = make (0.2); d.tone (1000, 950, 3, 1.0, 45); d.tone (2700, 2700, 1, 0.3, 20); return d.pcm (0.8); }
        case 32: { auto d = make (1.8); d.tone (4200, 4200, 1, 0.6, 900); d.tone (6100, 6100, 1, 0.3, 600); d.tone (11300, 11300, 1, 0.15, 400); return d.pcm (0.5); }
        case 33: { auto d = make (1.4); d.tone (110, 48, 50, 1.0, 650); return d.pcm(); }
        case 34: { auto d = make (0.5); d.hiss (1500, 10000, 1.0, 160, 20); d.tone (220, 200, 20, 0.15, 60); return d.pcm (0.6); }
        case 35: { auto d = make (0.8); d.tone (110, 75, 70, 1.0, 380); d.hiss (200, 2500, 0.15, 30); return d.pcm(); }
        case 36: { auto d = make (0.18); d.hiss (1800, 6000, 1.0, 25); d.tone (2200, 1900, 3, 0.2, 15); return d.pcm (0.7); }
        case 37: { auto d = make (1.8); d.metal (300, 2000, 0.8, 700); d.hiss (2000, 12000, 0.6, 600); return d.pcm (0.6); }
        case 38: { auto d = make (0.12); d.hiss (8000, 18000, 1.0, 20); return d.pcm (0.55); }
        case 39: { auto d = make (0.25); d.tone (2000, 120, 25, 1.0, 120); return d.pcm (0.8); }
        case 40: { auto d = make (0.4); d.hiss (200, 12000, 1.0, 120); return d.pcm (0.7); }
        case 41: { auto d = make (1.0); d.metal (820, 1500, 1.0, 350); return d.pcm (0.6); }
        case 42: { auto d = make (0.3); d.tone (210, 180, 20, 0.4, 400); d.hiss (900, 9000, 0.8, 600); for (int i = 0; i < d.n(); ++i) if (i > (int) (0.22 * kRate)) d.x[(size_t) i] *= std::exp (-(i - 0.22 * kRate) / 200.0); return d.pcm(); }
        case 43: { auto d = make (1.6); d.tone (95, 87, 40, 1.0, 900); d.tone (150, 146, 40, 0.4, 600); d.hiss (100, 1500, 0.15, 40); return d.pcm(); }
        default: break;
    }
    return {};
}

} // namespace synth

inline Bank Bank::builtin()
{
    Bank b;
    for (int i = 0; i < kMultisounds; ++i)
        synth::renderMultisound (b.multi[i], synth::recipeFor (i), i);
    for (int d = 0; d < kDrumSamples; ++d)
    {
        b.drum[d].pcmRate = synth::kRate;
        b.drum[d].pcm = synth::drumSample (d);
    }
    return b;
}

} // namespace m1
