#pragma once

#include "Model.h"
#include <vector>

namespace m1
{

// Subset of a program dump. Header F0 42 3n 19 40, then Korg 7-in-8 packed
// bytes, then F7. Unpacked layout is this instrument's, not a stored Korg dump:
// 10 name bytes, then the oscillator block below.

inline constexpr int kSysexName = 10;
inline constexpr int kSysexUnpacked = 25;

inline void pack7 (const uint8_t* src, int n, std::vector<uint8_t>& dst)
{
    int i = 0;
    while (i < n)
    {
        const int take = std::min (7, n - i);
        uint8_t msb = 0;
        uint8_t tmp[7] = {};
        for (int k = 0; k < take; ++k)
        {
            tmp[k] = (uint8_t) (src[i + k] & 0x7f);
            if (src[i + k] & 0x80)
                msb = (uint8_t) (msb | (1u << k));
        }
        dst.push_back (msb);
        for (int k = 0; k < take; ++k)
            dst.push_back (tmp[k]);
        i += take;
    }
}

inline bool unpack7 (const uint8_t* src, int n, std::vector<uint8_t>& dst)
{
    int i = 0;
    while (i < n)
    {
        if (i >= n)
            break;
        const uint8_t msb = src[i++];
        const int take = std::min (7, n - i);
        if (take <= 0)
            return false;
        for (int k = 0; k < take; ++k)
        {
            uint8_t v = (uint8_t) (src[i + k] & 0x7f);
            if (msb & (1u << k))
                v = (uint8_t) (v | 0x80);
            dst.push_back (v);
        }
        i += take;
    }
    return true;
}

inline void fillUnpacked (const Program& p, uint8_t* d)
{
    std::memset (d, 0, kSysexUnpacked);
    for (int i = 0; i < kSysexName && p.name[i] != 0; ++i)
        d[i] = (uint8_t) p.name[i];
    d[10] = (uint8_t) p.mode;
    d[11] = (uint8_t) p.assign;
    d[12] = (uint8_t) p.hold;
    d[13] = (uint8_t) p.osc1.multisound;
    d[14] = (uint8_t) p.osc1.level;
    d[15] = (uint8_t) (p.osc1.octave + 1);
    d[16] = (uint8_t) p.osc2.multisound;
    d[17] = (uint8_t) p.osc2.level;
    d[18] = (uint8_t) (p.osc2.octave + 1);
    d[19] = (uint8_t) (p.interval + 12);
    d[20] = (uint8_t) (p.detune + 50);
    d[21] = (uint8_t) p.delayStart;
    d[22] = (uint8_t) p.cutoff1;
    d[23] = (uint8_t) p.cutoff2;
    d[24] = (uint8_t) p.egInt1;
}

inline void applyUnpacked (const uint8_t* d, Program& p)
{
    char name[kSysexName + 1] = {};
    for (int i = 0; i < kSysexName; ++i)
        name[i] = (char) (d[i] & 0x7f);
    copyName (p.name, name);
    p.mode = d[10];
    p.assign = d[11];
    p.hold = d[12];
    p.osc1.multisound = d[13];
    p.osc1.level = d[14];
    p.osc1.octave = (int) d[15] - 1;
    p.osc2.multisound = d[16];
    p.osc2.level = d[17];
    p.osc2.octave = (int) d[18] - 1;
    p.interval = (int) d[19] - 12;
    p.detune = (int) d[20] - 50;
    p.delayStart = d[21];
    p.cutoff1 = d[22];
    p.cutoff2 = d[23];
    p.egInt1 = d[24];
}

inline void writeProgramDump (const Program& program, std::vector<uint8_t>& dst, int channel = 0)
{
    uint8_t raw[kSysexUnpacked];
    fillUnpacked (program, raw);
    dst.clear();
    dst.push_back (0xf0);
    dst.push_back (0x42);
    dst.push_back ((uint8_t) (0x30 | (channel & 0x0f)));
    dst.push_back (0x19);
    dst.push_back (0x40);
    pack7 (raw, kSysexUnpacked, dst);
    dst.push_back (0xf7);
}

inline bool readProgramDump (const uint8_t* data, int size, Program& program)
{
    if (data == nullptr || size < 8)
        return false;
    if (data[0] != 0xf0 || data[1] != 0x42 || data[3] != 0x19 || data[4] != 0x40)
        return false;
    if ((data[2] & 0xf0) != 0x30)
        return false;
    if (data[size - 1] != 0xf7)
        return false;
    std::vector<uint8_t> raw;
    if (! unpack7 (data + 5, size - 6, raw) || (int) raw.size() < kSysexUnpacked)
        return false;
    applyUnpacked (raw.data(), program);
    return true;
}

} // namespace m1
