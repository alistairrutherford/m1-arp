#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace m1
{

inline constexpr int kCalibration = 1;
inline constexpr int kOscSlots = 16;
inline constexpr int kMaxPrograms = 64;
inline constexpr int kFactoryPrograms = 24;
inline constexpr int kFactoryCombis = 8;
inline constexpr int kTimbres = 8;
inline constexpr int kKits = 4;
inline constexpr int kDrumKeys = 30;
inline constexpr int kDrumKeyBase = 35;
inline constexpr int kDrumSamples = 44;
inline constexpr int kMultisounds = 100;
inline constexpr int kArpSteps = 16;
inline constexpr int kNameLen = 24;
inline constexpr double kVdfEgOctaves = 4.0;
inline constexpr double kPitchMgCents = 50.0;
inline constexpr double kVdfMgOctaves = 2.0;
// Output gain of one oscillator at VDA level 99 and full expression. The
// multisamples peak at -1 dBFS, so a single full-velocity note peaks around
// -7 dBFS and chords stay under the output soft clipper.
inline constexpr float kVoiceGain = 0.45f;
inline constexpr float kDrumGain = 0.7f;

enum class OscMode { Single = 0, Double = 1, Drums = 2 };
enum class Assign { Poly = 0, Mono = 1 };
enum class CombiType { Single = 0, Layer = 1, Split = 2, Velocity = 3, Multi = 4 };
enum class ArpMode
{
    Up = 0, Down, UpDown, DownUp, Random, AsPlayed, Chord, Pattern
};
enum class Placement { Serial = 0, Parallel = 1 };

inline int clampInt (int v, int lo, int hi)
{
    return std::max (lo, std::min (hi, v));
}

inline double envelopeSeconds (int t)
{
    const double x = clampInt (t, 0, 99) / 99.0;
    return 0.002 * std::exp (x * std::log (5.0 / 0.002));
}

inline double osc2DelaySeconds (int d)
{
    if (d <= 0)
        return 0.0;
    const double x = (clampInt (d, 1, 99) - 1) / 98.0;
    return 0.002 * std::exp (x * std::log (0.5 / 0.002));
}

inline double cutoffHz (int cutoff)
{
    const double x = clampInt (cutoff, 0, 99) / 99.0;
    return 100.0 * std::exp (x * std::log (16000.0 / 100.0));
}

inline double mgHz (int frequency)
{
    const double x = clampInt (frequency, 0, 99) / 99.0;
    return 0.1 * std::exp (x * std::log (20.0 / 0.1));
}

inline int applyOffset (int value, int offset, int lo, int hi)
{
    return clampInt (value + offset, lo, hi);
}

struct Step
{
    int on = 1;
    int octave = 0;
    int velocity = 100;
};

struct ArpSettings
{
    int on = 0;
    int mode = (int) ArpMode::Up;
    int rate = 5;
    int octaves = 1;
    int gate = 50;
    int swing = 50;
    int hold = 0;
    Step steps[kArpSteps] {};

    static double rateQuarter (int rateIndex)
    {
        static const double table[10] = {
            4.0, 2.0, 4.0 / 3.0, 1.0, 2.0 / 3.0,
            0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125
        };
        return table[clampInt (rateIndex, 0, 9)];
    }
};

struct Fx
{
    int type = 0;
    int balance = 0;
    int p[8] = { 40, 20, 30, 40, 0, 0, 0, 50 };

    static float dryGain (int balance)
    {
        const float t = clampInt (balance, 0, 98) / 98.0f;
        return 1.0f - t;
    }

    static float wetGain (int balance)
    {
        return 1.0f - dryGain (balance);
    }
};

struct Eg4
{
    int at = 0, al = 99, dt = 40, bp = 70, st = 30, sl = 80, rt = 40, rl = 0;
};

struct PitchEg
{
    int start = 0, at = 0, al = 0, dt = 20, rt = 20, rl = 0;
    int velLevel = 0, velTime = 0;
};

struct VelPage
{
    int egInt = 0;
    int egTime = 0;
    int atPol = 0, dtPol = 0, stPol = 0, rtPol = 0;
};

struct KbdPage
{
    int centre = 60;
    int amount = 0;
    int egTime = 0;
    int atPol = 0, dtPol = 0, stPol = 0, rtPol = 0;
};

struct Mg
{
    int wave = 0;
    int frequency = 40;
    int delay = 0;
    int intensity = 0;
    int oscSelect = 0;
    int keySync = 0;
};

struct Aftertouch
{
    int pitch = 0;
    int pitchMg = 0;
    int vdf = 0;
    int vdfMg = 0;
    int vda = 0;
};

struct Joystick
{
    int bend = 2;
    int vdfSweep = 0;
    int pitchMg = 0;
    int pitchMgFreq = 0;
    int vdfMg = 0;
    int vdfMgFreq = 0;
};

struct Oscillator
{
    int multisound = 0;
    int level = 99;
    int octave = 0;
};

struct Program
{
    char name[kNameLen] = "Init";
    int mode = (int) OscMode::Single;
    int assign = (int) Assign::Poly;
    int hold = 0;
    int kit = 0;
    Oscillator osc1 {};
    Oscillator osc2 {};
    int interval = 0;
    int detune = 0;
    int delayStart = 0;
    PitchEg pitch1 {};
    PitchEg pitch2 {};
    int cutoff1 = 80;
    int egInt1 = 40;
    int cutoff2 = 80;
    int egInt2 = 40;
    Eg4 vdf1 {};
    Eg4 vdf2 {};
    Eg4 vda1 {};
    Eg4 vda2 {};
    VelPage vdfVel1 {};
    VelPage vdfVel2 {};
    VelPage vdaVel1 {};
    VelPage vdaVel2 {};
    KbdPage vdfKbd1 {};
    KbdPage vdfKbd2 {};
    KbdPage vdaKbd1 {};
    KbdPage vdaKbd2 {};
    Mg pitchMg {};
    Mg vdfMg {};
    Aftertouch after {};
    Joystick joy {};
    Fx fx1 {};
    Fx fx2 {};
    int placement = (int) Placement::Serial;
    int out3 = -1;
    int out4 = -1;
    ArpSettings arp {};

    int slotsPerNote() const
    {
        return mode == (int) OscMode::Double ? 2 : 1;
    }
};

struct Timbre
{
    int program = 0;
    int level = 99;
    int pan = 0;
    int damper = 1;
    int channel = 1;
    int keyLo = 0;
    int keyHi = 127;
    int velLo = 1;
    int velHi = 127;
    int transpose = 0;
    int detune = 0;
    int out3 = -1;
    int out4 = -1;
    ArpSettings arp {};
};

struct Combination
{
    char name[kNameLen] = "Single";
    int type = (int) CombiType::Single;
    int splitKey = 60;
    int velPoint = 80;
    Timbre timbre[kTimbres] {};
    ArpSettings arp {};
    Fx fx1 {};
    Fx fx2 {};
    int placement = (int) Placement::Serial;

    static Combination asSingle (int programIndex)
    {
        Combination c;
        c.type = (int) CombiType::Single;
        c.timbre[0].program = programIndex;
        c.timbre[0].level = 99;
        c.timbre[0].keyLo = 0;
        c.timbre[0].keyHi = 127;
        c.timbre[0].velLo = 1;
        c.timbre[0].velHi = 127;
        c.timbre[0].channel = 1;
        c.timbre[0].damper = 1;
        return c;
    }
};

struct DrumKey
{
    int sample = 0;
    int tune = 0;
    int level = 99;
    int decay = 70;
    int pan = 0;
    int group = 0;
    int note = 0;
};

struct DrumKit
{
    char name[kNameLen] = "Kit";
    DrumKey key[kDrumKeys] {};
};

struct PlayOffsets
{
    int oscBalance = 0;
    int cutoff = 0;
    int level = 0;
    int kbd = 0;
    int velocity = 0;
    int attack = 0;
    int release = 0;
    int effect = 0;
};

struct Session
{
    int calibration = kCalibration;
    int numPrograms = 0;
    int currentProgram = 0;
    int currentCombi = 0;
    int combiMode = 0;
    Program programs[kMaxPrograms] {};
    int numCombis = 0;
    Combination combis[32] {};
    DrumKit kits[kKits] {};
};

inline void copyName (char* dst, const char* src)
{
    std::memset (dst, 0, kNameLen);
    if (src == nullptr)
        return;
    std::strncpy (dst, src, kNameLen - 1);
}

inline const char* multisoundRole (int index)
{
    static const char* names[kMultisounds] = {
        "Piano", "E Piano 1", "E Piano 2", "Clav", "Harpsichord",
        "Organ", "Organ 2", "Magic Organ", "Guitar", "Guitar 2",
        "E Guitar", "Sitar", "Sitar 2", "A Bass", "Pick Bass",
        "E Bass", "Fretless", "Synth Bass", "Synth Bass 2", "Vibes",
        "Bell", "Tubular", "Bell Ring", "Kalimba", "Kalimba NT",
        "Syn Mallet", "Flute", "Pan Flute", "Bottles", "Voices",
        "Choir", "Strings", "Brass", "Brass 2", "Tenor Sax",
        "Mute Trumpet", "Trumpet", "Tuba", "Double Reed", "Koto",
        "Bamboo", "Rhythm", "Lore", "Lore NT", "Flexatone",
        "Wind Bells", "Pole", "Pole NT", "Block", "Block NT",
        "Finger Snap", "Pop", "Drop", "Drop NT", "Breath",
        "Breath NT", "Pluck", "Pluck NT", "Vibe Hit", "Vibe Hit NT",
        "Hammer", "Metal Hit", "Metal Hit NT", "Pick", "Distortion",
        "Dist NT", "Bass Thumb", "Bass Thumb NT", "Bass Thumb 2", "Wire",
        "Pan Wave", "Ping Wave", "Fv Wave", "Mv Wave", "Voice Wave",
        "Voice Wave NT", "Voice Wave 2", "DWGS EP", "DWGS EP 2", "DWGS EP 3",
        "DWGS Piano", "DWGS Clav", "DWGS Vibe", "DWGS Bass", "DWGS Bass 2",
        "DWGS Bell", "DWGS Organ", "DWGS Organ 2", "DWGS Voice", "Square",
        "Digital", "Saw", "Digital 2", "Pulse 25", "Pulse 10",
        "Digital 3", "Digital 4", "Digital 5", "Triangle", "Sine"
    };
    if (index < 0 || index >= kMultisounds)
        return "Empty";
    return names[index];
}

inline const char* effectName (int type)
{
    static const char* names[] = {
        "No Effect", "Hall", "Ensemble Hall", "Concert Hall", "Room", "Large Room",
        "Live Stage", "Early Ref 1", "Early Ref 2", "Early Ref 3", "Stereo Delay",
        "Cross Delay", "Stereo Chorus 1", "Stereo Chorus 2", "Stereo Flanger", "Cross Flanger",
        "Phaser 1", "Phaser 2", "Stereo Tremolo 1", "Stereo Tremolo 2", "Equalizer",
        "Overdrive", "Distortion", "Exciter", "Symphonic Ens", "Rotary Speaker",
        "Delay/Hall", "Delay/Room", "Delay/Early", "Delay/Delay", "Delay/Chorus",
        "Delay/Flanger", "Delay/Phaser", "Delay/Tremolo"
    };
    if (type < 0 || type > 33)
        return "No Effect";
    return names[type];
}

// Parameter labels for P1..P8 of an effect type, as the engine reads them.
// Null means the slot is unused for that type.
inline const char* effectParamName (int type, int index)
{
    static const char* none[8] = {};
    static const char* reverb[8] = { "Time", "Pre-delay", "Early", "Damping" };
    static const char* early[8] = { "Size", "Pre-delay" };
    static const char* delay[8] = { "Time L", "Time R", "Feedback", "Damping" };
    static const char* chorus[8] = { "Depth", "Speed", "Delay", "Triangle" };
    static const char* flanger[8] = { "Depth", "Speed", "Manual", "Feedback", "Triangle" };
    static const char* phaser[8] = { "Manual", "Speed", "Depth", "Feedback", "Triangle" };
    static const char* tremolo[8] = { "Depth", "Speed", "Triangle", "Shape" };
    static const char* eq[8] = { "Low Gain", "Low Freq", nullptr, nullptr, "High Gain", "High Freq" };
    static const char* drive[8] = { "Drive", "Level" };
    static const char* exciter[8] = { "Blend", "Emphasis" };
    static const char* ensemble[8] = { "Depth" };
    static const char* rotary[8] = { "Depth", "Fast" };
    static const char* comboVerb[8] = { "Delay", "Feedback", "Time", "Damping" };
    static const char* comboEarly[8] = { "Delay", "Feedback", "Size", "Pre-delay" };
    static const char* comboDelay[8] = { "Delay 1", "Feedback", "Delay 2", "Delay 2 R" };
    static const char* comboMod[8] = { "Delay", "Feedback", "Depth", "Speed" };
    const char** table = none;
    if (type >= 1 && type <= 6) table = reverb;
    else if (type >= 7 && type <= 9) table = early;
    else if (type == 10 || type == 11) table = delay;
    else if (type == 12 || type == 13) table = chorus;
    else if (type == 14 || type == 15) table = flanger;
    else if (type == 16 || type == 17) table = phaser;
    else if (type == 18 || type == 19) table = tremolo;
    else if (type == 20) table = eq;
    else if (type == 21 || type == 22) table = drive;
    else if (type == 23) table = exciter;
    else if (type == 24) table = ensemble;
    else if (type == 25) table = rotary;
    else if (type == 26 || type == 27) table = comboVerb;
    else if (type == 28) table = comboEarly;
    else if (type == 29) table = comboDelay;
    else if (type >= 30 && type <= 33) table = comboMod;
    return (index >= 0 && index < 8) ? table[index] : nullptr;
}

inline bool effectIsModulated (int type)
{
    if (type >= 12 && type <= 19) return true;
    if (type == 24 || type == 25) return true;
    if (type >= 30 && type <= 33) return true;
    return false;
}

inline bool effectPairAllowed (int a, int b)
{
    const bool special = (a == 24 || a == 25 || b == 24 || b == 25);
    if (! special)
        return true;
    return ! (effectIsModulated (a) && effectIsModulated (b));
}

inline bool effectHasLongTail (int type)
{
    if (type >= 1 && type <= 11) return true;
    if (type >= 26 && type <= 33) return true;
    return false;
}

inline int resolvedPartner (int a, int b)
{
    return effectPairAllowed (a, b) ? b : 0;
}

inline double effectiveCutoffHz (int cutoff, int note, int centre, int track)
{
    const double ratio = 1.0 + clampInt (track, -99, 99) / 99.0;
    const double semis = (note - centre) * ratio;
    return cutoffHz (cutoff) * std::pow (2.0, semis / 12.0);
}

inline int timePolarityScale (int polarity, int amount, float weight)
{
    if (polarity == 0 || amount == 0)
        return 1000;
    const float a = clampInt (amount, 0, 99) / 99.0f;
    const float w = std::max (0.0f, std::min (1.0f, weight));
    float scale = 1.0f;
    if (polarity > 0)
        scale = 1.0f - a * w;
    else
        scale = 1.0f - a * (1.0f - w);
    scale = std::max (0.05f, scale);
    return (int) std::lround (scale * 1000.0f);
}

} // namespace m1
