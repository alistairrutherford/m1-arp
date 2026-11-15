#pragma once

#include "Model.h"
#include <vector>
#include <fstream>

namespace m1
{

inline double phaseIncrement (double pcmRate, double hostRate, int note, int root,
                              int tuneCents, double extraSemis)
{
    const double semis = (note - root) + tuneCents / 100.0 + extraSemis;
    const double rate = hostRate > 0 ? hostRate : 44100.0;
    return (pcmRate / rate) * std::pow (2.0, semis / 12.0);
}

struct Zone
{
    int root = 60;
    int lo = 0;
    int hi = 127;
    int tuneCents = 0;
    int loopStart = 0;
    int loopEnd = 0;
    double pcmRate = 44100.0;
    std::vector<int16_t> pcm;

    bool oneShot() const { return loopEnd <= loopStart; }
};

struct Multisample
{
    std::vector<Zone> zones;

    const Zone* find (int note) const
    {
        for (const auto& z : zones)
            if (note >= z.lo && note <= z.hi && ! z.pcm.empty())
                return &z;
        return nullptr;
    }
};

struct DrumSample
{
    double pcmRate = 44100.0;
    std::vector<int16_t> pcm;
};

struct Bank
{
    Multisample multi[kMultisounds];
    DrumSample drum[kDrumSamples];

    // Synthesised factory content (BankBuilder.h). Zone tuneCents is a
    // correction: a zone whose PCM is 3 cents sharp of its root stores -3.
    static Bank builtin();

    // Writes the same M1BK format loadFile reads (all non-empty slots).
    bool writeFile (const std::string& path) const
    {
        std::ofstream out (path, std::ios::binary);
        if (! out)
            return false;
        auto u32 = [&] (uint32_t v) {
            const unsigned char b[4] = { (unsigned char) v, (unsigned char) (v >> 8),
                                         (unsigned char) (v >> 16), (unsigned char) (v >> 24) };
            out.write ((const char*) b, 4);
        };
        auto u16 = [&] (int v) {
            const uint16_t x = (uint16_t) (int16_t) v;
            const unsigned char b[2] = { (unsigned char) x, (unsigned char) (x >> 8) };
            out.write ((const char*) b, 2);
        };
        out.write ("M1BK", 4);
        u32 (1);
        uint32_t mc = 0;
        for (const auto& m : multi)
            mc += m.zones.empty() ? 0u : 1u;
        u32 (mc);
        for (int i = 0; i < kMultisounds; ++i)
        {
            if (multi[i].zones.empty())
                continue;
            u16 (i);
            u16 ((int) multi[i].zones.size());
            for (const auto& z : multi[i].zones)
            {
                u16 (z.root); u16 (z.lo); u16 (z.hi); u16 (z.tuneCents);
                u32 ((uint32_t) z.loopStart); u32 ((uint32_t) z.loopEnd);
                u32 ((uint32_t) z.pcm.size());
                out.write ((const char*) z.pcm.data(), (std::streamsize) z.pcm.size() * 2);
            }
        }
        uint32_t dc = 0;
        for (const auto& d : drum)
            dc += d.pcm.empty() ? 0u : 1u;
        u32 (dc);
        for (int i = 0; i < kDrumSamples; ++i)
        {
            if (drum[i].pcm.empty())
                continue;
            u16 (i);
            u32 ((uint32_t) drum[i].pcm.size());
            out.write ((const char*) drum[i].pcm.data(), (std::streamsize) drum[i].pcm.size() * 2);
        }
        return (bool) out;
    }

    bool loadFile (const std::string& path)
    {
        std::ifstream in (path, std::ios::binary);
        if (! in)
            return false;
        char magic[4] = {};
        in.read (magic, 4);
        if (std::string (magic, 4) != "M1BK")
            return false;
        auto u32 = [&] () {
            unsigned char b[4];
            in.read ((char*) b, 4);
            return (uint32_t) b[0] | ((uint32_t) b[1] << 8) | ((uint32_t) b[2] << 16) | ((uint32_t) b[3] << 24);
        };
        auto u16 = [&] () {
            unsigned char b[2];
            in.read ((char*) b, 2);
            return (uint16_t) (b[0] | (b[1] << 8));
        };
        if (u32() != 1)
            return false;
        const uint32_t mc = u32();
        for (uint32_t i = 0; i < mc; ++i)
        {
            const int index = u16();
            const int zones = u16();
            if (index < 0 || index >= kMultisounds)
                return false;
            multi[index].zones.clear();
            for (int z = 0; z < zones; ++z)
            {
                Zone zone;
                zone.root = (int16_t) u16();
                zone.lo = (int16_t) u16();
                zone.hi = (int16_t) u16();
                zone.tuneCents = (int16_t) u16();
                zone.loopStart = (int) u32();
                zone.loopEnd = (int) u32();
                const uint32_t n = u32();
                zone.pcm.resize (n);
                in.read ((char*) zone.pcm.data(), (std::streamsize) n * 2);
                zone.pcmRate = 44100.0;
                multi[index].zones.push_back (std::move (zone));
            }
        }
        const uint32_t dc = u32();
        for (uint32_t i = 0; i < dc; ++i)
        {
            const int index = u16();
            const uint32_t n = u32();
            if (index < 0 || index >= kDrumSamples)
                return false;
            drum[index].pcm.resize (n);
            in.read ((char*) drum[index].pcm.data(), (std::streamsize) n * 2);
        }
        return (bool) in || in.eof();
    }
};

} // namespace m1

#include "BankBuilder.h"
