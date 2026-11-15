#pragma once

#include "Arp.h"
#include "Effects.h"
#include "Voice.h"

namespace m1
{

struct Transport
{
    double bpm = 120.0;
    double ppq = 0.0;
    bool playing = false;
};

struct Performance
{
    PlayOffsets play {};
    int cutoffAdd = 0;
    int releaseAdd = 0;
    int arpOn = -1;
    int arpRate = -1;
    int arpGate = -1;
    int fx1Add = 0;
    int fx2Add = 0;
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate > 0 ? sampleRate : 44100.0;
        block = std::max (maxBlock, 8192);
        arp.prepare (sr);
        for (auto& a : timbreArp)
            a.prepare (sr);
        pool.prepare (sr);
        rack.prepare (sr, block);
        arpMidi.ensureSize (8192);
        for (auto& b : partMidi)
            b.ensureSize (4096);
        reset();
    }

    void reset()
    {
        arp.reset();
        for (auto& a : timbreArp)
            a.reset();
        pool.silence();
        rack.reset();
        ctrl = {};
        pool.damper = false;
    }

    void setBank (const Bank* b) { pool.setBank (b); }
    void setSeed (uint32_t s)
    {
        arp.setSeed (s);
        for (auto& a : timbreArp)
            a.setSeed (s);
    }

    void process (float* L, float* R, int n, juce::MidiBuffer& midi,
                  const Session& session, Transport tr, Performance perf)
    {
        pool.nMidi = 0;
        ord = 0;
        if (L == nullptr || R == nullptr || n <= 0)
            return;
        for (int i = 0; i < n; ++i)
            L[i] = R[i] = 0.0f;
        if (poolBankMissing (session))
        {
            midi.clear();
            return;
        }
        pool.setKits (session.kits);

        Program sounding[kTimbres];
        Timbre timbres[kTimbres];
        const Program* live[kTimbres] = {};
        int nTimbres = 1;
        bool multi = false;
        ArpSettings clock {};
        Fx fx1 {}, fx2 {};
        int placement = 0;
        resolve (session, perf, sounding, timbres, nTimbres, multi, clock, fx1, fx2, placement);
        for (int i = 0; i < nTimbres; ++i)
            live[i] = &sounding[i];

        rack.set (fx1, fx2, placement);
        lastFx1 = fx1.type;
        lastFx2 = rack.secondType();

        nEvents = 0;
        if (! multi)
            runSingleArp (midi, n, tr, clock, nTimbres, timbres);
        else
            runMultiArp (midi, n, tr, clock, timbres);

        std::sort (events, events + nEvents, [] (const Ev& a, const Ev& b)
        {
            if (a.sample != b.sample)
                return a.sample < b.sample;
            return a.ord < b.ord;
        });

        int ev = 0;
        for (int i = 0; i < n; ++i)
        {
            while (ev < nEvents && events[ev].sample <= i)
                apply (events[ev++], sounding, timbres, i);
            float pitchW[kTimbres] = {};
            float vdfW[kTimbres] = {};
            for (int t = 0; t < nTimbres; ++t)
            {
                const Program& p = sounding[t];
                const int pitchF = clampInt (p.pitchMg.frequency + (int) std::lround (ctrl.mod * p.joy.pitchMgFreq), 0, 99);
                const int vdfF = clampInt (p.vdfMg.frequency + (int) std::lround (ctrl.mod * p.joy.vdfMgFreq), 0, 99);
                pitchPhase[t] += mgHz (pitchF) / sr;
                vdfPhase[t] += mgHz (vdfF) / sr;
                if (pitchPhase[t] >= 1.0)
                    pitchPhase[t] -= std::floor (pitchPhase[t]);
                if (vdfPhase[t] >= 1.0)
                    vdfPhase[t] -= std::floor (vdfPhase[t]);
                pitchW[t] = mgWave (p.pitchMg.wave, pitchPhase[t]);
                vdfW[t] = mgWave (p.vdfMg.wave, vdfPhase[t]);
            }
            pool.renderSample (live, ctrl, pitchW, vdfW, L[i], R[i], i);
        }

        int off = 0;
        while (off < n)
        {
            const int m = std::min (block, n - off);
            rack.process (L + off, R + off, m);
            off += m;
        }

        midi.clear();
        for (int i = 0; i < pool.nMidi; ++i)
        {
            const auto& e = pool.midi[i];
            if (e.on)
                midi.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.vel), e.sample);
            else
                midi.addEvent (juce::MidiMessage::noteOff (e.channel, e.note), e.sample);
        }
    }

    int slotsUsed() const { return pool.slotsUsed(); }
    int activeCount() const { return pool.activeCount(); }
    bool notePlaying (int note) const { return pool.notePlaying (note); }
    double debugInc (int note, int osc) const { return pool.debugInc (note, osc); }
    double tailSeconds() const { return EffectRack::tailSeconds (lastFx1, lastFx2); }
    int resolvedSecondEffect() const { return lastFx2; }

private:
    struct Ev
    {
        int sample = 0;
        int ord = 0;
        int kind = 0;
        int ch = 1;
        int note = 0;
        int vel = 0;
        int cc = 0;
        int val = 0;
        int timbre = -1;
        int honorDamper = 1;
    };

    bool poolBankMissing (const Session& session) const
    {
        return session.numPrograms <= 0;
    }

    static Program applyPerf (Program p, const Performance& perf)
    {
        auto addRelease = [&] (int& t)
        {
            t = applyOffset (t, perf.play.release + perf.releaseAdd, 0, 99);
        };
        auto addAttack = [&] (int& t)
        {
            t = applyOffset (t, perf.play.attack, 0, 99);
        };
        p.cutoff1 = applyOffset (p.cutoff1, perf.play.cutoff + perf.cutoffAdd, 0, 99);
        p.cutoff2 = applyOffset (p.cutoff2, perf.play.cutoff + perf.cutoffAdd, 0, 99);
        addAttack (p.vda1.at); addAttack (p.vda2.at);
        addAttack (p.vdf1.at); addAttack (p.vdf2.at);
        addAttack (p.pitch1.at); addAttack (p.pitch2.at);
        addRelease (p.vda1.rt); addRelease (p.vda2.rt);
        addRelease (p.vdf1.rt); addRelease (p.vdf2.rt);
        addRelease (p.pitch1.rt); addRelease (p.pitch2.rt);
        p.osc1.level = applyOffset (p.osc1.level, perf.play.level - perf.play.oscBalance, 0, 99);
        p.osc2.level = applyOffset (p.osc2.level, perf.play.level + perf.play.oscBalance, 0, 99);
        p.vdfKbd1.amount = applyOffset (p.vdfKbd1.amount, perf.play.kbd, -99, 99);
        p.vdfKbd2.amount = applyOffset (p.vdfKbd2.amount, perf.play.kbd, -99, 99);
        p.vdaKbd1.amount = applyOffset (p.vdaKbd1.amount, perf.play.kbd, -99, 99);
        p.vdaKbd2.amount = applyOffset (p.vdaKbd2.amount, perf.play.kbd, -99, 99);
        p.vdfVel1.egInt = applyOffset (p.vdfVel1.egInt, perf.play.velocity, -99, 99);
        p.vdfVel2.egInt = applyOffset (p.vdfVel2.egInt, perf.play.velocity, -99, 99);
        p.vdaVel1.egInt = applyOffset (p.vdaVel1.egInt, perf.play.velocity, -99, 99);
        p.vdaVel2.egInt = applyOffset (p.vdaVel2.egInt, perf.play.velocity, -99, 99);
        p.fx1.balance = applyOffset (p.fx1.balance, perf.play.effect + perf.fx1Add, 0, 98);
        p.fx2.balance = applyOffset (p.fx2.balance, perf.play.effect + perf.fx2Add, 0, 98);
        return p;
    }

    void resolve (const Session& session, const Performance& perf,
                  Program sounding[kTimbres], Timbre timbres[kTimbres], int& nTimbres,
                  bool& multi, ArpSettings& clock, Fx& fx1, Fx& fx2, int& placement)
    {
        const int nProg = std::max (1, session.numPrograms);
        auto programAt = [&] (int index)
        {
            return applyPerf (session.programs[clampInt (index, 0, nProg - 1)], perf);
        };
        multi = false;
        nTimbres = 1;
        if (session.combiMode != 0 && session.numCombis > 0)
        {
            const Combination& combi = session.combis[clampInt (session.currentCombi, 0, session.numCombis - 1)];
            fx1 = combi.fx1;
            fx2 = combi.fx2;
            placement = combi.placement;
            fx1.balance = applyOffset (fx1.balance, perf.play.effect + perf.fx1Add, 0, 98);
            fx2.balance = applyOffset (fx2.balance, perf.play.effect + perf.fx2Add, 0, 98);
            clock = combi.arp;
            if (perf.arpRate >= 0)
                clock.rate = clampInt (perf.arpRate, 0, 9);
            splitKey = combi.splitKey;
            velPoint = combi.velPoint;
            combiType = combi.type;
            if (combi.type == (int) CombiType::Multi)
            {
                multi = true;
                nTimbres = kTimbres;
                for (int i = 0; i < kTimbres; ++i)
                {
                    timbres[i] = combi.timbre[i];
                    timbres[i].arp.rate = clock.rate;
                    timbres[i].arp.swing = combi.arp.swing;
                    sounding[i] = programAt (timbres[i].program);
                }
                return;
            }
            if (perf.arpOn >= 0)
                clock.on = perf.arpOn ? 1 : 0;
            if (perf.arpGate >= 0)
                clock.gate = clampInt (perf.arpGate, 1, 100);
            nTimbres = combi.type == (int) CombiType::Single ? 1 : 2;
            for (int i = 0; i < nTimbres; ++i)
            {
                timbres[i] = combi.timbre[i];
                sounding[i] = programAt (timbres[i].program);
            }
            return;
        }

        combiType = (int) CombiType::Single;
        splitKey = 60;
        velPoint = 64;
        const int index = session.numPrograms > 0 ? clampInt (session.currentProgram, 0, session.numPrograms - 1) : 0;
        sounding[0] = programAt (index);
        clock = sounding[0].arp;
        if (perf.arpOn >= 0)
            clock.on = perf.arpOn ? 1 : 0;
        if (perf.arpRate >= 0)
            clock.rate = clampInt (perf.arpRate, 0, 9);
        if (perf.arpGate >= 0)
            clock.gate = clampInt (perf.arpGate, 1, 100);
        fx1 = sounding[0].fx1;
        fx2 = sounding[0].fx2;
        placement = sounding[0].placement;
        timbres[0] = {};
        timbres[0].program = index;
        timbres[0].level = 99;
        timbres[0].channel = 0;
        timbres[0].keyLo = 0;
        timbres[0].keyHi = 127;
        timbres[0].velLo = 1;
        timbres[0].velHi = 127;
        timbres[0].damper = 1;
        nTimbres = 1;
    }

    void pushEvent (Ev e)
    {
        if (nEvents >= eventCap)
            return;
        e.ord = ord++;
        events[nEvents++] = e;
    }

    void collectBuffer (const juce::MidiBuffer& buf, int timbre, int honorDamper, bool notesOnly)
    {
        for (const auto meta : buf)
        {
            const auto m = meta.getMessage();
            Ev e;
            e.sample = meta.samplePosition;
            e.ch = m.getChannel();
            e.timbre = timbre;
            e.honorDamper = honorDamper;
            if (m.isNoteOn())
            {
                e.kind = 1;
                e.note = m.getNoteNumber();
                e.vel = m.getVelocity();
                pushEvent (e);
            }
            else if (m.isNoteOff())
            {
                e.kind = 2;
                e.note = m.getNoteNumber();
                pushEvent (e);
            }
            else if (! notesOnly && m.isController())
            {
                e.kind = 3;
                e.cc = m.getControllerNumber();
                e.val = m.getControllerValue();
                e.timbre = -1;
                pushEvent (e);
            }
            else if (! notesOnly && m.isPitchWheel())
            {
                e.kind = 4;
                e.val = m.getPitchWheelValue();
                e.timbre = -1;
                pushEvent (e);
            }
            else if (! notesOnly && (m.isChannelPressure() || m.isAftertouch()))
            {
                e.kind = 5;
                e.val = m.isChannelPressure() ? m.getChannelPressureValue() : m.getAfterTouchValue();
                e.timbre = -1;
                pushEvent (e);
            }
        }
    }

    void runSingleArp (const juce::MidiBuffer& input, int n, Transport tr, const ArpSettings& clock,
                       int nTimbres, const Timbre* timbres)
    {
        arpMidi.clear();
        for (const auto meta : input)
            arpMidi.addEvent (meta.getMessage(), meta.samplePosition);
        arp.process (arpMidi, n, clock, tr.bpm, tr.ppq, tr.playing);
        const int honor = clock.on ? 0 : 1;
        collectBuffer (arpMidi, -1, honor, false);
        (void) nTimbres;
        (void) timbres;
    }

    void runMultiArp (const juce::MidiBuffer& input, int n, Transport tr, const ArpSettings& clock,
                      const Timbre* timbres)
    {
        for (const auto meta : input)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn() || m.isNoteOff())
                continue;
            Ev e;
            e.sample = meta.samplePosition;
            e.ch = m.getChannel();
            e.timbre = -1;
            if (m.isController())
            {
                e.kind = 3;
                e.cc = m.getControllerNumber();
                e.val = m.getControllerValue();
                pushEvent (e);
            }
            else if (m.isPitchWheel())
            {
                e.kind = 4;
                e.val = m.getPitchWheelValue();
                pushEvent (e);
            }
            else if (m.isChannelPressure() || m.isAftertouch())
            {
                e.kind = 5;
                e.val = m.isChannelPressure() ? m.getChannelPressureValue() : m.getAfterTouchValue();
                pushEvent (e);
            }
        }

        for (int t = 0; t < kTimbres; ++t)
        {
            partMidi[t].clear();
            for (const auto meta : input)
            {
                const auto m = meta.getMessage();
                if (m.isController() && m.getControllerNumber() == 64)
                {
                    partMidi[t].addEvent (m, meta.samplePosition);
                    continue;
                }
                if (! m.isNoteOn() && ! m.isNoteOff())
                    continue;
                if (! channelOk (timbres[t], m.getChannel()) || ! keyOk (timbres[t], m.getNoteNumber()))
                    continue;
                if (m.isNoteOn() && ! velOk (timbres[t], m.getVelocity()))
                    continue;
                partMidi[t].addEvent (m, meta.samplePosition);
            }
            ArpSettings settings = timbres[t].arp;
            settings.rate = clock.rate;
            settings.swing = clock.swing;
            const bool enabled = settings.on != 0;
            if (enabled)
                timbreArp[t].process (partMidi[t], n, settings, tr.bpm, tr.ppq, tr.playing);
            collectBuffer (partMidi[t], t, enabled ? 0 : 1, true);
        }
    }

    static bool channelOk (const Timbre& t, int ch)
    {
        if (t.channel <= 0)
            return true;
        return t.channel == ch;
    }

    static bool keyOk (const Timbre& t, int note)
    {
        return note >= t.keyLo && note <= t.keyHi;
    }

    static bool velOk (const Timbre& t, int vel)
    {
        return vel >= t.velLo && vel <= t.velHi;
    }

    int route (bool noteOn, int note, int vel, int ch, int timbreHint, int* out) const
    {
        if (timbreHint >= 0)
        {
            out[0] = timbreHint;
            return 1;
        }
        int n = 0;
        auto consider = [&] (int index, bool checkVel)
        {
            if (index < 0)
                return;
            if (! channelOk (routeTimbres[index], ch) || ! keyOk (routeTimbres[index], note))
                return;
            if (checkVel && noteOn && ! velOk (routeTimbres[index], vel))
                return;
            out[n++] = index;
        };
        if (combiType == (int) CombiType::Layer)
        {
            consider (0, true);
            consider (1, true);
        }
        else if (combiType == (int) CombiType::Split)
        {
            consider (note < splitKey ? 0 : 1, true);
        }
        else if (combiType == (int) CombiType::Velocity)
        {
            if (! noteOn)
            {
                consider (0, false);
                consider (1, false);
            }
            else
                consider (vel < velPoint ? 0 : 1, true);
        }
        else
        {
            consider (0, true);
        }
        return n;
    }

    void apply (const Ev& e, const Program sounding[kTimbres], const Timbre timbres[kTimbres], int sample)
    {
        for (int i = 0; i < kTimbres; ++i)
            routeTimbres[i] = timbres[i];
        if (e.kind == 4)
        {
            ctrl.bend = (e.val - 8192) / 8192.0f;
            return;
        }
        if (e.kind == 5)
        {
            ctrl.pressure = clampInt (e.val, 0, 127) / 127.0f;
            return;
        }
        if (e.kind == 3)
        {
            if (e.cc == 1)
                ctrl.mod = e.val / 127.0f;
            else if (e.cc == 11)
                ctrl.expression = e.val / 127.0f;
            else if (e.cc == 64)
            {
                pool.setDamper (e.val >= 64, sample);
            }
            else if (e.cc == 120)
                pool.silence();
            else if (e.cc == 123)
                pool.releaseAll (sample);
            return;
        }
        int targets[kTimbres];
        const int n = route (e.kind == 1, e.note, e.vel, e.ch, e.timbre, targets);
        if (e.kind == 2)
        {
            if (e.timbre >= 0)
                pool.noteOff (e.note, e.timbre, e.honorDamper != 0, sample);
            else
            {
                for (int i = 0; i < n; ++i)
                    pool.noteOff (e.note, targets[i], e.honorDamper != 0, sample);
            }
            return;
        }
        if (e.kind != 1)
            return;
        for (int i = 0; i < n; ++i)
        {
            const int t = targets[i];
            NoteRequest req;
            req.program = sounding[t];
            req.note = e.note;
            req.vel = e.vel;
            req.channel = e.ch;
            req.timbre = t;
            req.transpose = timbres[t].transpose;
            req.detune = timbres[t].detune;
            req.pan = timbres[t].pan;
            req.level = timbres[t].level;
            req.damperEnabled = timbres[t].damper != 0;
            req.pitchPhase = pitchPhase[t];
            req.vdfPhase = vdfPhase[t];
            pool.noteOn (req, sample);
        }
    }

    VoicePool pool;
    Arpeggiator arp;
    Arpeggiator timbreArp[kTimbres];
    EffectRack rack;
    juce::MidiBuffer arpMidi;
    juce::MidiBuffer partMidi[kTimbres];
    Controllers ctrl {};
    double sr = 48000.0;
    int block = 8192;
    int lastFx1 = 0;
    int lastFx2 = 0;
    int combiType = 0;
    int splitKey = 60;
    int velPoint = 64;
    double pitchPhase[kTimbres] = {};
    double vdfPhase[kTimbres] = {};
    Ev events[1024];
    static constexpr int eventCap = 1024;
    int nEvents = 0;
    int ord = 0;
    Timbre routeTimbres[kTimbres] {};
};

} // namespace m1
