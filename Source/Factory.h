#pragma once

#include "Model.h"

namespace m1
{

// Envelope times (calibration v1, 0-99 -> 2 ms .. 5 s, exponential):
//   20 ~ 10 ms   30 ~ 21 ms   40 ~ 47 ms   50 ~ 0.1 s   60 ~ 0.23 s
//   65 ~ 0.34 s  70 ~ 0.5 s   75 ~ 0.75 s  80 ~ 1.1 s   85 ~ 1.6 s
//   90 ~ 2.4 s   95 ~ 3.5 s   99 ~ 5 s

inline void nameProgram (Program& p, const char* name) { copyName (p.name, name); }

inline Eg4 eg (int at, int al, int dt, int bp, int st, int sl, int rt, int rl = 0)
{
    Eg4 e;
    e.at = at; e.al = al; e.dt = dt; e.bp = bp; e.st = st; e.sl = sl; e.rt = rt; e.rl = rl;
    return e;
}

inline Eg4 organEg (int rt = 25) { return eg (0, 99, 0, 99, 0, 99, rt); }

inline void setFx (Fx& fx, int type, int balance, std::initializer_list<int> params = {})
{
    fx = {};
    fx.type = type;
    fx.balance = balance;
    for (int i = 0; i < 8; ++i)
        fx.p[i] = 50;
    int i = 0;
    for (int v : params)
        if (i < 8)
            fx.p[i++] = v;
}

inline void noFx (Fx& fx) { setFx (fx, 0, 0); }

// A single-oscillator program: VDF opens on the attack and settles, the VDA
// holds, the keyboard tracks the cutoff fully, moderate velocity response.
inline Program makeTone (const char* name, int multi, int cutoff = 80)
{
    Program p;
    nameProgram (p, name);
    p.osc1.multisound = multi;
    p.osc1.level = 99;
    p.osc1.octave = 0;
    p.osc2 = p.osc1;
    p.cutoff1 = cutoff;
    p.cutoff2 = cutoff;
    p.egInt1 = 30;
    p.egInt2 = 30;
    p.vdf1 = eg (0, 50, 60, 25, 70, 15, 50, 0);
    p.vdf2 = p.vdf1;
    p.vda1 = eg (0, 99, 40, 99, 40, 99, 50, 0);
    p.vda2 = p.vda1;
    p.vdfVel1.egInt = 40;
    p.vdfVel2.egInt = 40;
    p.vdaVel1.egInt = 40;
    p.vdaVel2.egInt = 40;
    p.joy.bend = 2;
    p.joy.pitchMg = 50;
    p.pitchMg.frequency = 75;
    p.pitchMg.intensity = 0;
    p.pitchMg.oscSelect = 3;
    p.pitchMg.keySync = 1;
    p.vdfMg.frequency = 40;
    p.vdfMg.oscSelect = 3;
    noFx (p.fx1);
    noFx (p.fx2);
    p.placement = (int) Placement::Serial;
    return p;
}

inline void makeDouble (Program& p, int multi2, int level2, int detune, int interval = 0)
{
    p.mode = (int) OscMode::Double;
    p.osc2.multisound = multi2;
    p.osc2.level = level2;
    p.detune = detune;
    p.interval = interval;
    p.vdf2 = p.vdf1;
    p.vda2 = p.vda1;
    p.cutoff2 = p.cutoff1;
    p.egInt2 = p.egInt1;
    p.vdfVel2 = p.vdfVel1;
    p.vdaVel2 = p.vdaVel1;
    p.vdfKbd2 = p.vdfKbd1;
    p.vdaKbd2 = p.vdaKbd1;
}

inline void vibrato (Program& p, int depth, int delay = 75)
{
    p.pitchMg.wave = 0;
    p.pitchMg.frequency = 75;
    p.pitchMg.intensity = depth;
    p.pitchMg.delay = delay;
    p.pitchMg.oscSelect = 3;
    p.pitchMg.keySync = 1;
}

// Decay times shorten as the note rises, like an acoustic instrument.
inline void keyedDecay (Program& p, int amount)
{
    p.vdaKbd1.centre = 48;
    p.vdaKbd1.egTime = amount;
    p.vdaKbd1.dtPol = 1;
    p.vdaKbd1.stPol = 1;
    p.vdaKbd2 = p.vdaKbd1;
}

// General MIDI style layout so drum clips land on the expected sounds.
inline void fillKit (DrumKit& kit, const char* name, int variant)
{
    copyName (kit.name, name);
    struct K { int note, sample, tune, level, pan, group; };
    static const K layout[kDrumKeys] = {
        { 35, 9, 0, 95, 0, 0 },   { 36, 0, 0, 99, 0, 0 },   { 37, 24, 0, 80, 0, 0 },  { 38, 1, 0, 95, 0, 0 },
        { 39, 5, 0, 90, 6, 0 },   { 40, 10, 0, 90, 0, 0 },  { 41, 35, 0, 92, -18, 0 },{ 42, 2, 0, 78, 12, 1 },
        { 43, 4, 0, 92, -12, 0 }, { 44, 13, 0, 72, 12, 1 }, { 45, 7, -2, 92, -4, 0 }, { 46, 3, 0, 78, 12, 1 },
        { 47, 7, 0, 92, 4, 0 },   { 48, 8, 0, 92, 10, 0 },  { 49, 12, 0, 80, -16, 0 },{ 50, 8, 4, 90, 16, 0 },
        { 51, 6, 0, 78, 18, 0 },  { 52, 37, 0, 78, -20, 0 },{ 53, 17, 0, 78, 18, 0 }, { 54, 15, 0, 75, -10, 0 },
        { 55, 23, 0, 78, -14, 0 },{ 56, 14, 0, 80, 8, 0 },  { 57, 12, 3, 80, 16, 0 }, { 58, 30, 0, 75, -8, 0 },
        { 59, 6, -3, 78, 20, 0 }, { 60, 20, 0, 88, 10, 0 }, { 61, 21, 0, 88, 6, 0 },  { 62, 18, 3, 88, -6, 0 },
        { 63, 18, 0, 88, -10, 0 },{ 64, 19, 0, 88, -14, 0 }
    };
    for (int i = 0; i < kDrumKeys; ++i)
    {
        auto& k = kit.key[i];
        k.note = layout[i].note;
        k.sample = layout[i].sample;
        k.tune = layout[i].tune;
        k.level = layout[i].level;
        k.pan = layout[i].pan;
        k.group = layout[i].group;
        k.decay = 99;
    }
    if (variant == 1)          // Room: a little looser
        for (auto& k : kit.key) k.tune -= 1;
    else if (variant == 2)     // Tight: short kick and snare, closed decays
    {
        kit.key[1].sample = 9;
        kit.key[3].sample = 10;
        for (auto& k : kit.key) k.decay = 80;
        kit.key[11].decay = 70;
    }
    else if (variant == 3)     // Electro
    {
        kit.key[1].sample = 33;
        kit.key[0].sample = 33; kit.key[0].tune = -4;
        kit.key[3].sample = 42;
        kit.key[5].sample = 34;
        kit.key[7].sample = 38;
        kit.key[9].sample = 38; kit.key[9].decay = 60;
        kit.key[11].sample = 38; kit.key[11].decay = 85;
        kit.key[4].sample = 36;
        kit.key[21].sample = 39;
    }
}

inline void setTimbre (Timbre& t, int program, int channel, int level = 99)
{
    t = {};
    t.program = program;
    t.level = level;
    t.channel = channel;
    t.keyLo = 0;
    t.keyHi = 127;
    t.velLo = 1;
    t.velHi = 127;
    t.damper = 1;
    t.pan = 0;
}

inline Session makeFactory()
{
    Session s;
    s.calibration = kCalibration;
    s.numPrograms = kFactoryPrograms;
    s.numCombis = kFactoryCombis;
    s.currentProgram = 0;
    s.currentCombi = 0;
    s.combiMode = 0;
    auto& P = s.programs;

    // 0  Concert piano: bright strike that darkens, hall.
    P[0] = makeTone ("Concert Grand", 0, 58);
    P[0].egInt1 = 45;
    P[0].vdf1 = eg (0, 70, 70, 35, 88, 0, 50);
    P[0].vda1 = eg (0, 99, 72, 72, 93, 0, 48);
    P[0].vdfVel1.egInt = 60;
    P[0].vdaVel1.egInt = 50;
    keyedDecay (P[0], 45);
    setFx (P[0].fx1, 23, 30, { 30, 40 });
    setFx (P[0].fx2, 1, 32, { 58, 15, 35, 45 });

    // 1  Tine electric piano with chorus.
    P[1] = makeTone ("Tine Piano", 1, 66);
    P[1].egInt1 = 40;
    P[1].vdf1 = eg (0, 60, 65, 20, 85, 0, 50);
    P[1].vda1 = eg (0, 99, 78, 72, 93, 0, 52);
    P[1].vdfVel1.egInt = 75;
    P[1].vdaVel1.egInt = 55;
    keyedDecay (P[1], 35);
    setFx (P[1].fx1, 12, 45, { 45, 30, 30, 0 });
    setFx (P[1].fx2, 1, 22, { 50, 10, 30, 50 });

    // 2  Drawbar organ, slow rotary into a room.
    P[2] = makeTone ("Club Organ", 6, 92);
    P[2].egInt1 = 0;
    P[2].vdf1 = eg (0, 0, 0, 0, 0, 0, 0);
    P[2].vda1 = organEg (22);
    P[2].vdaVel1.egInt = 0;
    P[2].osc1.level = 99;
    P[2].vdfVel1.egInt = 0;
    setFx (P[2].fx1, 25, 98, { 70, 0 });
    setFx (P[2].fx2, 4, 18, { 40, 5, 40, 40 });

    // 3  Picked bass, mono.
    P[3] = makeTone ("Pick Bass", 14, 50);
    P[3].assign = (int) Assign::Mono;
    P[3].egInt1 = 50;
    P[3].vdf1 = eg (0, 70, 55, 15, 72, 0, 40);
    P[3].vda1 = eg (0, 99, 75, 75, 90, 40, 35);
    P[3].vdfVel1.egInt = 55;
    P[3].vdaVel1.egInt = 45;

    // 4  Choir: aah and ooh layered, slow attack.
    P[4] = makeTone ("Cathedral Choir", 30, 74);
    P[4].egInt1 = 15;
    P[4].vdf1 = eg (55, 40, 70, 10, 70, 0, 70);
    P[4].vda1 = eg (66, 99, 60, 99, 60, 99, 76);
    P[4].vdaVel1.egInt = 25;
    P[4].osc1.level = 85;
    makeDouble (P[4], 29, 70, 8);
    vibrato (P[4], 10, 80);
    setFx (P[4].fx1, 2, 42, { 80, 25, 30, 40 });

    // 5  String section.
    P[5] = makeTone ("String Section", 31, 70);
    P[5].egInt1 = 25;
    P[5].vdf1 = eg (55, 50, 70, 20, 70, 10, 70);
    P[5].vda1 = eg (58, 99, 60, 99, 60, 99, 74);
    P[5].vdaVel1.egInt = 30;
    P[5].osc1.level = 85;
    vibrato (P[5], 12, 80);
    setFx (P[5].fx1, 24, 40, { 50 });
    setFx (P[5].fx2, 3, 34, { 70, 20, 30, 40 });

    // 6  Wide pad: strings and an airy wave.
    P[6] = makeTone ("Nebula Pad", 31, 64);
    P[6].egInt1 = 35;
    P[6].vdf1 = eg (75, 60, 85, 30, 85, 20, 80);
    P[6].vda1 = eg (72, 99, 60, 99, 60, 99, 84);
    P[6].vdaVel1.egInt = 20;
    P[6].osc1.level = 72;
    makeDouble (P[6], 42, 62, 9, 12);
    P[6].vdfMg.intensity = 12;
    P[6].vdfMg.frequency = 25;
    setFx (P[6].fx1, 13, 45, { 50, 25, 40, 0 });
    setFx (P[6].fx2, 3, 45, { 85, 30, 25, 45 });

    // 7  Mono saw lead with delay.
    P[7] = makeTone ("Saw Lead", 91, 56);
    P[7].assign = (int) Assign::Mono;
    P[7].egInt1 = 50;
    P[7].vdf1 = eg (0, 70, 62, 35, 70, 25, 50);
    P[7].vda1 = eg (0, 99, 50, 90, 50, 90, 50);
    vibrato (P[7], 14, 80);
    P[7].joy.pitchMg = 70;
    setFx (P[7].fx1, 10, 28, { 45, 52, 35, 40 });
    setFx (P[7].fx2, 1, 22, { 50, 10, 30, 50 });

    // 8  Detuned pulse synth with phaser.
    P[8] = makeTone ("Pulse Brass", 93, 54);
    P[8].egInt1 = 50;
    P[8].vdf1 = eg (35, 70, 65, 35, 75, 25, 50);
    P[8].vda1 = eg (20, 99, 50, 95, 50, 95, 55);
    P[8].osc1.level = 80;
    makeDouble (P[8], 91, 70, 9);
    setFx (P[8].fx1, 16, 35, { 40, 25, 60, 60 });
    setFx (P[8].fx2, 1, 25, { 55, 10, 30, 50 });

    // 9  Soft sine with vibrato.
    P[9] = makeTone ("Soft Sine", 99, 99);
    P[9].egInt1 = 0;
    P[9].vdf1 = eg (0, 0, 0, 0, 0, 0, 0);
    P[9].osc1.level = 70;
    P[9].vda1 = eg (30, 99, 50, 99, 50, 99, 60);
    vibrato (P[9], 10, 70);
    setFx (P[9].fx1, 1, 25, { 50, 10, 30, 50 });

    // 10 Drum kit.
    P[10] = makeTone ("Studio Kit", 0, 99);
    P[10].mode = (int) OscMode::Drums;
    P[10].hold = 1;
    P[10].kit = 0;
    P[10].vdaVel1.egInt = 0;
    setFx (P[10].fx1, 4, 15, { 35, 5, 40, 40 });

    // 11 Arp: synth bass, up, 1/16.
    P[11] = makeTone ("Arp Bass", 17, 48);
    P[11].egInt1 = 55;
    P[11].vdf1 = eg (0, 75, 55, 10, 65, 0, 40);
    P[11].vda1 = eg (0, 99, 65, 70, 80, 40, 40);
    P[11].vdfVel1.egInt = 50;
    P[11].arp.on = 1;
    P[11].arp.mode = (int) ArpMode::Up;
    P[11].arp.rate = 7;
    P[11].arp.octaves = 2;
    P[11].arp.gate = 55;
    P[11].arp.swing = 50;
    setFx (P[11].fx1, 11, 22, { 36, 46, 35, 40 });

    // 12 Arp: mallets, down, 1/16.
    P[12] = makeTone ("Arp Mallets", 25, 75);
    P[12].vda1 = eg (0, 99, 70, 60, 85, 0, 55);
    P[12].arp = P[11].arp;
    P[12].arp.mode = (int) ArpMode::Down;
    P[12].arp.gate = 70;
    setFx (P[12].fx1, 10, 30, { 45, 55, 40, 40 });
    setFx (P[12].fx2, 1, 25, { 60, 10, 30, 40 });

    // 13 Arp: chord stabs on the beat.
    P[13] = makeTone ("Chord Stabs", 2, 64);
    P[13].vdf1 = eg (0, 60, 60, 10, 75, 0, 45);
    P[13].vda1 = eg (0, 99, 70, 60, 85, 0, 45);
    P[13].arp.on = 1;
    P[13].arp.mode = (int) ArpMode::Chord;
    P[13].arp.rate = 5;
    P[13].arp.gate = 45;
    P[13].arp.swing = 58;
    setFx (P[13].fx1, 12, 40, { 40, 30, 30, 0 });
    setFx (P[13].fx2, 1, 30, { 55, 10, 30, 40 });

    // 14 Arp: step pattern on a filtered saw.
    P[14] = makeTone ("Step Pattern", 91, 46);
    P[14].egInt1 = 60;
    P[14].vdf1 = eg (0, 80, 55, 10, 65, 0, 40);
    P[14].vda1 = eg (0, 99, 60, 70, 75, 30, 40);
    P[14].vdfVel1.egInt = 70;
    P[14].arp.on = 1;
    P[14].arp.mode = (int) ArpMode::Pattern;
    P[14].arp.rate = 7;
    P[14].arp.gate = 55;
    P[14].arp.octaves = 1;
    for (int i = 0; i < kArpSteps; ++i)
    {
        P[14].arp.steps[i].on = (i % 4) != 1 ? 1 : 0;
        P[14].arp.steps[i].octave = (i % 8 == 6) ? 1 : (i == 12 ? -1 : 0);
        P[14].arp.steps[i].velocity = (i % 4 == 0) ? 120 : 80 + (i % 3) * 10;
    }
    setFx (P[14].fx1, 11, 28, { 40, 50, 40, 40 });

    // 15 Brass section with a filter swell.
    P[15] = makeTone ("Brass Section", 32, 52);
    P[15].egInt1 = 45;
    P[15].vdf1 = eg (45, 80, 65, 40, 75, 30, 50);
    P[15].vda1 = eg (35, 99, 55, 92, 60, 88, 55);
    P[15].vdfVel1.egInt = 60;
    vibrato (P[15], 8, 85);
    setFx (P[15].fx1, 1, 28, { 55, 15, 35, 45 });

    // 16 Strings with stereo delay.
    P[16] = makeTone ("Delay Strings", 31, 72);
    P[16].vda1 = eg (50, 99, 60, 99, 60, 99, 70);
    vibrato (P[16], 10, 80);
    setFx (P[16].fx1, 10, 30, { 48, 62, 40, 50 });
    setFx (P[16].fx2, 1, 30, { 65, 15, 30, 45 });

    // 17 Digital electric piano with chorus.
    P[17] = makeTone ("Glass EP", 2, 70);
    P[17].vdf1 = eg (0, 50, 65, 15, 85, 0, 50);
    P[17].vda1 = eg (0, 99, 78, 70, 93, 0, 55);
    P[17].vdaVel1.egInt = 50;
    keyedDecay (P[17], 35);
    setFx (P[17].fx1, 13, 45, { 45, 25, 40, 0 });
    setFx (P[17].fx2, 1, 25, { 55, 10, 30, 50 });

    // 18 Distorted lead through a flanger.
    P[18] = makeTone ("Flange Lead", 64, 62);
    P[18].assign = (int) Assign::Mono;
    P[18].vda1 = eg (0, 99, 50, 95, 50, 95, 50);
    vibrato (P[18], 15, 85);
    setFx (P[18].fx1, 15, 40, { 60, 20, 30, 70, 0 });
    setFx (P[18].fx2, 10, 22, { 40, 50, 30, 40 });

    // 19 Square and triangle pad with phaser.
    P[19] = makeTone ("Phase Pad", 89, 58);
    P[19].vdf1 = eg (70, 50, 80, 20, 80, 15, 75);
    P[19].vda1 = eg (68, 99, 60, 99, 60, 99, 80);
    P[19].osc1.level = 45;
    makeDouble (P[19], 98, 45, -7);
    setFx (P[19].fx1, 17, 40, { 40, 15, 70, 65 });
    setFx (P[19].fx2, 3, 35, { 75, 20, 25, 45 });

    // 20 Vibraphone with tremolo.
    P[20] = makeTone ("Tremolo Vibes", 19, 82);
    P[20].vda1 = eg (0, 99, 80, 80, 94, 0, 60);
    P[20].vdaVel1.egInt = 50;
    keyedDecay (P[20], 30);
    setFx (P[20].fx1, 19, 98, { 45, 40, 0, 0 });
    setFx (P[20].fx2, 1, 30, { 60, 10, 30, 40 });

    // 21 Overdriven synth bass.
    P[21] = makeTone ("Driven Bass", 18, 46);
    P[21].assign = (int) Assign::Mono;
    P[21].egInt1 = 50;
    P[21].vdf1 = eg (0, 70, 60, 20, 70, 10, 40);
    P[21].vda1 = eg (0, 99, 60, 90, 70, 80, 40);
    setFx (P[21].fx1, 21, 70, { 45, 70 });

    // 22 Bright piano with exciter and a big hall.
    P[22] = makeTone ("Air Piano", 0, 66);
    P[22].egInt1 = 40;
    P[22].vdf1 = eg (0, 65, 70, 35, 88, 5, 50);
    P[22].vda1 = eg (0, 99, 72, 72, 93, 0, 50);
    P[22].vdaVel1.egInt = 45;
    keyedDecay (P[22], 45);
    setFx (P[22].fx1, 23, 55, { 50, 60 });
    setFx (P[22].fx2, 3, 40, { 75, 25, 30, 35 });

    // 23 Strings and choir an octave apart, ensemble.
    P[23] = makeTone ("Ensemble", 31, 70);
    P[23].vda1 = eg (60, 99, 60, 99, 60, 99, 76);
    P[23].osc1.level = 85;
    makeDouble (P[23], 30, 65, 5, 12);
    vibrato (P[23], 8, 80);
    setFx (P[23].fx1, 24, 45, { 60 });
    setFx (P[23].fx2, 1, 35, { 70, 20, 30, 40 });

    static const char* kitNames[kKits] = { "Studio", "Room", "Tight", "Electro" };
    for (int i = 0; i < kKits; ++i)
        fillKit (s.kits[i], kitNames[i], i);

    auto& C = s.combis;
    C[0] = Combination::asSingle (0);
    copyName (C[0].name, "Grand Hall");
    C[0].fx1 = P[0].fx1;
    C[0].fx2 = P[0].fx2;
    setTimbre (C[0].timbre[0], 0, 0);

    C[1] = Combination::asSingle (0);
    copyName (C[1].name, "Piano Strings");
    C[1].type = (int) CombiType::Layer;
    setTimbre (C[1].timbre[0], 0, 0, 92);
    setTimbre (C[1].timbre[1], 5, 0, 70);
    C[1].timbre[0].pan = -12;
    C[1].timbre[1].pan = 12;
    setFx (C[1].fx1, 24, 25, { 50 });
    setFx (C[1].fx2, 3, 32, { 70, 15, 30, 40 });

    C[2] = Combination::asSingle (3);
    copyName (C[2].name, "Bass / Piano");
    C[2].type = (int) CombiType::Split;
    C[2].splitKey = 55;
    setTimbre (C[2].timbre[0], 3, 0);
    setTimbre (C[2].timbre[1], 0, 0);
    setFx (C[2].fx1, 1, 25, { 55, 10, 30, 45 });

    C[3] = Combination::asSingle (1);
    copyName (C[3].name, "Soft / Loud");
    C[3].type = (int) CombiType::Velocity;
    C[3].velPoint = 96;
    setTimbre (C[3].timbre[0], 1, 0);
    setTimbre (C[3].timbre[1], 22, 0);
    setFx (C[3].fx1, 12, 30, { 40, 30, 30, 0 });
    setFx (C[3].fx2, 1, 28, { 55, 10, 30, 45 });

    C[4] = Combination::asSingle (0);
    copyName (C[4].name, "Eight Parts");
    C[4].type = (int) CombiType::Multi;
    {
        const int parts[8] = { 0, 1, 3, 5, 6, 7, 9, 10 };
        for (int i = 0; i < 8; ++i)
            setTimbre (C[4].timbre[i], parts[i], i + 1, 90);
        C[4].timbre[7].channel = 10;
        C[4].timbre[6].channel = 7;
    }
    setFx (C[4].fx1, 1, 22, { 55, 10, 30, 45 });

    C[5] = Combination::asSingle (11);
    copyName (C[5].name, "Bass Arp");
    C[5].arp = P[11].arp;
    setTimbre (C[5].timbre[0], 11, 0);
    C[5].fx1 = P[11].fx1;

    C[6] = Combination::asSingle (3);
    copyName (C[6].name, "Multi Arp");
    C[6].type = (int) CombiType::Multi;
    C[6].arp.rate = 7;
    C[6].arp.swing = 55;
    setTimbre (C[6].timbre[0], 11, 1);
    C[6].timbre[0].arp.on = 1;
    C[6].timbre[0].arp.mode = (int) ArpMode::Up;
    C[6].timbre[0].arp.gate = 50;
    C[6].timbre[0].arp.octaves = 2;
    setTimbre (C[6].timbre[1], 12, 1, 80);
    C[6].timbre[1].transpose = 12;
    C[6].timbre[1].arp.on = 1;
    C[6].timbre[1].arp.mode = (int) ArpMode::UpDown;
    C[6].timbre[1].arp.gate = 60;
    C[6].timbre[1].arp.octaves = 1;
    for (int i = 2; i < 8; ++i)
        setTimbre (C[6].timbre[i], 9, i + 1, 0);
    setFx (C[6].fx1, 11, 25, { 40, 50, 40, 40 });
    setFx (C[6].fx2, 1, 22, { 55, 10, 30, 45 });

    C[7] = Combination::asSingle (5);
    copyName (C[7].name, "Choir Strings");
    C[7].type = (int) CombiType::Layer;
    setTimbre (C[7].timbre[0], 5, 0, 85);
    setTimbre (C[7].timbre[1], 4, 0, 75);
    C[7].timbre[0].pan = -15;
    C[7].timbre[1].pan = 15;
    setFx (C[7].fx1, 2, 40, { 80, 25, 30, 40 });

    return s;
}

} // namespace m1
