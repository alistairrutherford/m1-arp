#pragma once

#include "Model.h"
#include <JuceHeader.h>

namespace m1
{

inline void setAttr (juce::XmlElement& e, const char* key, int value)
{
    e.setAttribute (key, value);
}

inline int attr (const juce::XmlElement& e, const char* key, int fallback)
{
    return e.hasAttribute (key) ? e.getIntAttribute (key) : fallback;
}

inline void writeEg (juce::XmlElement& parent, const char* name, const Eg4& eg)
{
    auto* e = parent.createNewChildElement (name);
    setAttr (*e, "at", eg.at); setAttr (*e, "al", eg.al);
    setAttr (*e, "dt", eg.dt); setAttr (*e, "bp", eg.bp);
    setAttr (*e, "st", eg.st); setAttr (*e, "sl", eg.sl);
    setAttr (*e, "rt", eg.rt); setAttr (*e, "rl", eg.rl);
}

inline void readEg (const juce::XmlElement& parent, const char* name, Eg4& eg)
{
    if (auto* e = parent.getChildByName (name))
    {
        eg.at = attr (*e, "at", eg.at); eg.al = attr (*e, "al", eg.al);
        eg.dt = attr (*e, "dt", eg.dt); eg.bp = attr (*e, "bp", eg.bp);
        eg.st = attr (*e, "st", eg.st); eg.sl = attr (*e, "sl", eg.sl);
        eg.rt = attr (*e, "rt", eg.rt); eg.rl = attr (*e, "rl", eg.rl);
    }
}

inline void writePitch (juce::XmlElement& parent, const char* name, const PitchEg& eg)
{
    auto* e = parent.createNewChildElement (name);
    setAttr (*e, "start", eg.start); setAttr (*e, "at", eg.at); setAttr (*e, "al", eg.al);
    setAttr (*e, "dt", eg.dt); setAttr (*e, "rt", eg.rt); setAttr (*e, "rl", eg.rl);
    setAttr (*e, "velLevel", eg.velLevel); setAttr (*e, "velTime", eg.velTime);
}

inline void readPitch (const juce::XmlElement& parent, const char* name, PitchEg& eg)
{
    if (auto* e = parent.getChildByName (name))
    {
        eg.start = attr (*e, "start", eg.start);
        eg.at = attr (*e, "at", eg.at); eg.al = attr (*e, "al", eg.al);
        eg.dt = attr (*e, "dt", eg.dt); eg.rt = attr (*e, "rt", eg.rt); eg.rl = attr (*e, "rl", eg.rl);
        eg.velLevel = attr (*e, "velLevel", eg.velLevel);
        eg.velTime = attr (*e, "velTime", eg.velTime);
    }
}

inline void writeVel (juce::XmlElement& parent, const char* name, const VelPage& v)
{
    auto* e = parent.createNewChildElement (name);
    setAttr (*e, "egInt", v.egInt); setAttr (*e, "egTime", v.egTime);
    setAttr (*e, "atPol", v.atPol); setAttr (*e, "dtPol", v.dtPol);
    setAttr (*e, "stPol", v.stPol); setAttr (*e, "rtPol", v.rtPol);
}

inline void readVel (const juce::XmlElement& parent, const char* name, VelPage& v)
{
    if (auto* e = parent.getChildByName (name))
    {
        v.egInt = attr (*e, "egInt", v.egInt); v.egTime = attr (*e, "egTime", v.egTime);
        v.atPol = attr (*e, "atPol", v.atPol); v.dtPol = attr (*e, "dtPol", v.dtPol);
        v.stPol = attr (*e, "stPol", v.stPol); v.rtPol = attr (*e, "rtPol", v.rtPol);
    }
}

inline void writeKbd (juce::XmlElement& parent, const char* name, const KbdPage& v)
{
    auto* e = parent.createNewChildElement (name);
    setAttr (*e, "centre", v.centre); setAttr (*e, "amount", v.amount); setAttr (*e, "egTime", v.egTime);
    setAttr (*e, "atPol", v.atPol); setAttr (*e, "dtPol", v.dtPol);
    setAttr (*e, "stPol", v.stPol); setAttr (*e, "rtPol", v.rtPol);
}

inline void readKbd (const juce::XmlElement& parent, const char* name, KbdPage& v)
{
    if (auto* e = parent.getChildByName (name))
    {
        v.centre = attr (*e, "centre", v.centre); v.amount = attr (*e, "amount", v.amount);
        v.egTime = attr (*e, "egTime", v.egTime);
        v.atPol = attr (*e, "atPol", v.atPol); v.dtPol = attr (*e, "dtPol", v.dtPol);
        v.stPol = attr (*e, "stPol", v.stPol); v.rtPol = attr (*e, "rtPol", v.rtPol);
    }
}

inline void writeMg (juce::XmlElement& parent, const char* name, const Mg& m)
{
    auto* e = parent.createNewChildElement (name);
    setAttr (*e, "wave", m.wave); setAttr (*e, "frequency", m.frequency);
    setAttr (*e, "delay", m.delay); setAttr (*e, "intensity", m.intensity);
    setAttr (*e, "oscSelect", m.oscSelect); setAttr (*e, "keySync", m.keySync);
}

inline void readMg (const juce::XmlElement& parent, const char* name, Mg& m)
{
    if (auto* e = parent.getChildByName (name))
    {
        m.wave = attr (*e, "wave", m.wave); m.frequency = attr (*e, "frequency", m.frequency);
        m.delay = attr (*e, "delay", m.delay); m.intensity = attr (*e, "intensity", m.intensity);
        m.oscSelect = attr (*e, "oscSelect", m.oscSelect); m.keySync = attr (*e, "keySync", m.keySync);
    }
}

inline void writeFx (juce::XmlElement& parent, const char* name, const Fx& fx)
{
    auto* e = parent.createNewChildElement (name);
    setAttr (*e, "type", fx.type); setAttr (*e, "balance", fx.balance);
    for (int i = 0; i < 8; ++i)
        setAttr (*e, ("p" + juce::String (i)).toRawUTF8(), fx.p[i]);
}

inline void readFx (const juce::XmlElement& parent, const char* name, Fx& fx)
{
    if (auto* e = parent.getChildByName (name))
    {
        fx.type = attr (*e, "type", fx.type);
        fx.balance = attr (*e, "balance", fx.balance);
        for (int i = 0; i < 8; ++i)
            fx.p[i] = attr (*e, ("p" + juce::String (i)).toRawUTF8(), fx.p[i]);
    }
}

inline void writeArp (juce::XmlElement& parent, const ArpSettings& arp)
{
    auto* e = parent.createNewChildElement ("Arp");
    setAttr (*e, "on", arp.on); setAttr (*e, "mode", arp.mode); setAttr (*e, "rate", arp.rate);
    setAttr (*e, "octaves", arp.octaves); setAttr (*e, "gate", arp.gate);
    setAttr (*e, "swing", arp.swing); setAttr (*e, "hold", arp.hold);
    for (int i = 0; i < kArpSteps; ++i)
    {
        auto* s = e->createNewChildElement ("Step");
        setAttr (*s, "on", arp.steps[i].on);
        setAttr (*s, "octave", arp.steps[i].octave);
        setAttr (*s, "velocity", arp.steps[i].velocity);
    }
}

inline void readArp (const juce::XmlElement& parent, ArpSettings& arp)
{
    if (auto* e = parent.getChildByName ("Arp"))
    {
        arp.on = attr (*e, "on", arp.on); arp.mode = attr (*e, "mode", arp.mode);
        arp.rate = attr (*e, "rate", arp.rate); arp.octaves = attr (*e, "octaves", arp.octaves);
        arp.gate = attr (*e, "gate", arp.gate); arp.swing = attr (*e, "swing", arp.swing);
        arp.hold = attr (*e, "hold", arp.hold);
        int i = 0;
        for (auto* s : e->getChildIterator())
        {
            if (! s->hasTagName ("Step") || i >= kArpSteps)
                continue;
            arp.steps[i].on = attr (*s, "on", arp.steps[i].on);
            arp.steps[i].octave = attr (*s, "octave", arp.steps[i].octave);
            arp.steps[i].velocity = attr (*s, "velocity", arp.steps[i].velocity);
            ++i;
        }
    }
}

inline void writeOsc (juce::XmlElement& parent, const char* name, const Oscillator& o)
{
    auto* e = parent.createNewChildElement (name);
    setAttr (*e, "multi", o.multisound); setAttr (*e, "level", o.level); setAttr (*e, "octave", o.octave);
}

inline void readOsc (const juce::XmlElement& parent, const char* name, Oscillator& o)
{
    if (auto* e = parent.getChildByName (name))
    {
        o.multisound = attr (*e, "multi", o.multisound);
        o.level = attr (*e, "level", o.level);
        o.octave = attr (*e, "octave", o.octave);
    }
}

inline void writeProgram (juce::XmlElement& parent, const Program& p)
{
    auto* e = parent.createNewChildElement ("Program");
    e->setAttribute ("name", p.name);
    setAttr (*e, "mode", p.mode); setAttr (*e, "assign", p.assign); setAttr (*e, "hold", p.hold);
    setAttr (*e, "kit", p.kit); setAttr (*e, "interval", p.interval); setAttr (*e, "detune", p.detune);
    setAttr (*e, "delayStart", p.delayStart);
    setAttr (*e, "cutoff1", p.cutoff1); setAttr (*e, "egInt1", p.egInt1);
    setAttr (*e, "cutoff2", p.cutoff2); setAttr (*e, "egInt2", p.egInt2);
    setAttr (*e, "placement", p.placement); setAttr (*e, "out3", p.out3); setAttr (*e, "out4", p.out4);
    writeOsc (*e, "Osc1", p.osc1); writeOsc (*e, "Osc2", p.osc2);
    writePitch (*e, "Pitch1", p.pitch1); writePitch (*e, "Pitch2", p.pitch2);
    writeEg (*e, "Vdf1", p.vdf1); writeEg (*e, "Vdf2", p.vdf2);
    writeEg (*e, "Vda1", p.vda1); writeEg (*e, "Vda2", p.vda2);
    writeVel (*e, "VdfVel1", p.vdfVel1); writeVel (*e, "VdfVel2", p.vdfVel2);
    writeVel (*e, "VdaVel1", p.vdaVel1); writeVel (*e, "VdaVel2", p.vdaVel2);
    writeKbd (*e, "VdfKbd1", p.vdfKbd1); writeKbd (*e, "VdfKbd2", p.vdfKbd2);
    writeKbd (*e, "VdaKbd1", p.vdaKbd1); writeKbd (*e, "VdaKbd2", p.vdaKbd2);
    writeMg (*e, "PitchMg", p.pitchMg); writeMg (*e, "VdfMg", p.vdfMg);
    auto* at = e->createNewChildElement ("After");
    setAttr (*at, "pitch", p.after.pitch); setAttr (*at, "pitchMg", p.after.pitchMg);
    setAttr (*at, "vdf", p.after.vdf); setAttr (*at, "vdfMg", p.after.vdfMg); setAttr (*at, "vda", p.after.vda);
    auto* joy = e->createNewChildElement ("Joy");
    setAttr (*joy, "bend", p.joy.bend); setAttr (*joy, "vdfSweep", p.joy.vdfSweep);
    setAttr (*joy, "pitchMg", p.joy.pitchMg); setAttr (*joy, "pitchMgFreq", p.joy.pitchMgFreq);
    setAttr (*joy, "vdfMg", p.joy.vdfMg); setAttr (*joy, "vdfMgFreq", p.joy.vdfMgFreq);
    writeFx (*e, "Fx1", p.fx1); writeFx (*e, "Fx2", p.fx2);
    writeArp (*e, p.arp);
}

inline void readProgram (const juce::XmlElement& e, Program& p)
{
    if (e.hasAttribute ("name"))
        copyName (p.name, e.getStringAttribute ("name").toRawUTF8());
    p.mode = attr (e, "mode", p.mode); p.assign = attr (e, "assign", p.assign);
    p.hold = attr (e, "hold", p.hold); p.kit = attr (e, "kit", p.kit);
    p.interval = attr (e, "interval", p.interval); p.detune = attr (e, "detune", p.detune);
    p.delayStart = attr (e, "delayStart", p.delayStart);
    p.cutoff1 = attr (e, "cutoff1", p.cutoff1); p.egInt1 = attr (e, "egInt1", p.egInt1);
    p.cutoff2 = attr (e, "cutoff2", p.cutoff2); p.egInt2 = attr (e, "egInt2", p.egInt2);
    p.placement = attr (e, "placement", p.placement);
    p.out3 = attr (e, "out3", p.out3); p.out4 = attr (e, "out4", p.out4);
    readOsc (e, "Osc1", p.osc1); readOsc (e, "Osc2", p.osc2);
    readPitch (e, "Pitch1", p.pitch1); readPitch (e, "Pitch2", p.pitch2);
    readEg (e, "Vdf1", p.vdf1); readEg (e, "Vdf2", p.vdf2);
    readEg (e, "Vda1", p.vda1); readEg (e, "Vda2", p.vda2);
    readVel (e, "VdfVel1", p.vdfVel1); readVel (e, "VdfVel2", p.vdfVel2);
    readVel (e, "VdaVel1", p.vdaVel1); readVel (e, "VdaVel2", p.vdaVel2);
    readKbd (e, "VdfKbd1", p.vdfKbd1); readKbd (e, "VdfKbd2", p.vdfKbd2);
    readKbd (e, "VdaKbd1", p.vdaKbd1); readKbd (e, "VdaKbd2", p.vdaKbd2);
    readMg (e, "PitchMg", p.pitchMg); readMg (e, "VdfMg", p.vdfMg);
    if (auto* at = e.getChildByName ("After"))
    {
        p.after.pitch = attr (*at, "pitch", p.after.pitch);
        p.after.pitchMg = attr (*at, "pitchMg", p.after.pitchMg);
        p.after.vdf = attr (*at, "vdf", p.after.vdf);
        p.after.vdfMg = attr (*at, "vdfMg", p.after.vdfMg);
        p.after.vda = attr (*at, "vda", p.after.vda);
    }
    if (auto* joy = e.getChildByName ("Joy"))
    {
        p.joy.bend = attr (*joy, "bend", p.joy.bend);
        p.joy.vdfSweep = attr (*joy, "vdfSweep", p.joy.vdfSweep);
        p.joy.pitchMg = attr (*joy, "pitchMg", p.joy.pitchMg);
        p.joy.pitchMgFreq = attr (*joy, "pitchMgFreq", p.joy.pitchMgFreq);
        p.joy.vdfMg = attr (*joy, "vdfMg", p.joy.vdfMg);
        p.joy.vdfMgFreq = attr (*joy, "vdfMgFreq", p.joy.vdfMgFreq);
    }
    readFx (e, "Fx1", p.fx1); readFx (e, "Fx2", p.fx2);
    readArp (e, p.arp);
}

inline void writeTimbre (juce::XmlElement& parent, const Timbre& t)
{
    auto* e = parent.createNewChildElement ("Timbre");
    setAttr (*e, "program", t.program); setAttr (*e, "level", t.level); setAttr (*e, "pan", t.pan);
    setAttr (*e, "damper", t.damper); setAttr (*e, "channel", t.channel);
    setAttr (*e, "keyLo", t.keyLo); setAttr (*e, "keyHi", t.keyHi);
    setAttr (*e, "velLo", t.velLo); setAttr (*e, "velHi", t.velHi);
    setAttr (*e, "transpose", t.transpose); setAttr (*e, "detune", t.detune);
    setAttr (*e, "out3", t.out3); setAttr (*e, "out4", t.out4);
    writeArp (*e, t.arp);
}

inline void readTimbre (const juce::XmlElement& e, Timbre& t)
{
    t.program = attr (e, "program", t.program); t.level = attr (e, "level", t.level);
    t.pan = attr (e, "pan", t.pan); t.damper = attr (e, "damper", t.damper);
    t.channel = attr (e, "channel", t.channel);
    t.keyLo = attr (e, "keyLo", t.keyLo); t.keyHi = attr (e, "keyHi", t.keyHi);
    t.velLo = attr (e, "velLo", t.velLo); t.velHi = attr (e, "velHi", t.velHi);
    t.transpose = attr (e, "transpose", t.transpose); t.detune = attr (e, "detune", t.detune);
    t.out3 = attr (e, "out3", t.out3); t.out4 = attr (e, "out4", t.out4);
    readArp (e, t.arp);
}

inline std::unique_ptr<juce::XmlElement> sessionToXml (const Session& s)
{
    auto xml = std::make_unique<juce::XmlElement> ("M1ARP");
    xml->setAttribute ("calibration", s.calibration > 0 ? s.calibration : kCalibration);
    auto* programs = xml->createNewChildElement ("Programs");
    setAttr (*programs, "count", s.numPrograms);
    setAttr (*programs, "current", s.currentProgram);
    for (int i = 0; i < s.numPrograms && i < kMaxPrograms; ++i)
        writeProgram (*programs, s.programs[i]);
    auto* combis = xml->createNewChildElement ("Combis");
    setAttr (*combis, "count", s.numCombis);
    setAttr (*combis, "current", s.currentCombi);
    setAttr (*combis, "enabled", s.combiMode);
    for (int i = 0; i < s.numCombis && i < 32; ++i)
    {
        auto* c = combis->createNewChildElement ("Combi");
        c->setAttribute ("name", s.combis[i].name);
        setAttr (*c, "type", s.combis[i].type);
        setAttr (*c, "splitKey", s.combis[i].splitKey);
        setAttr (*c, "velPoint", s.combis[i].velPoint);
        setAttr (*c, "placement", s.combis[i].placement);
        for (int t = 0; t < kTimbres; ++t)
            writeTimbre (*c, s.combis[i].timbre[t]);
        writeArp (*c, s.combis[i].arp);
        writeFx (*c, "Fx1", s.combis[i].fx1);
        writeFx (*c, "Fx2", s.combis[i].fx2);
    }
    auto* kits = xml->createNewChildElement ("Kits");
    for (int i = 0; i < kKits; ++i)
    {
        auto* k = kits->createNewChildElement ("Kit");
        k->setAttribute ("name", s.kits[i].name);
        for (int n = 0; n < kDrumKeys; ++n)
        {
            auto* key = k->createNewChildElement ("Key");
            const auto& d = s.kits[i].key[n];
            setAttr (*key, "sample", d.sample); setAttr (*key, "tune", d.tune);
            setAttr (*key, "level", d.level); setAttr (*key, "decay", d.decay);
            setAttr (*key, "pan", d.pan); setAttr (*key, "group", d.group);
            setAttr (*key, "note", d.note);
        }
    }
    return xml;
}

inline Session sessionFromXml (const juce::XmlElement& xml)
{
    Session s;
    s.calibration = xml.hasAttribute ("calibration") ? xml.getIntAttribute ("calibration") : kCalibration;
    if (s.calibration <= 0)
        s.calibration = kCalibration;
    if (auto* programs = xml.getChildByName ("Programs"))
    {
        s.currentProgram = attr (*programs, "current", 0);
        int i = 0;
        for (auto* child : programs->getChildIterator())
        {
            if (! child->hasTagName ("Program") || i >= kMaxPrograms)
                continue;
            readProgram (*child, s.programs[i]);
            ++i;
        }
        s.numPrograms = i;
    }
    if (auto* combis = xml.getChildByName ("Combis"))
    {
        s.currentCombi = attr (*combis, "current", 0);
        s.combiMode = attr (*combis, "enabled", 0);
        int i = 0;
        for (auto* child : combis->getChildIterator())
        {
            if (! child->hasTagName ("Combi") || i >= 32)
                continue;
            if (child->hasAttribute ("name"))
                copyName (s.combis[i].name, child->getStringAttribute ("name").toRawUTF8());
            s.combis[i].type = attr (*child, "type", s.combis[i].type);
            s.combis[i].splitKey = attr (*child, "splitKey", s.combis[i].splitKey);
            s.combis[i].velPoint = attr (*child, "velPoint", s.combis[i].velPoint);
            s.combis[i].placement = attr (*child, "placement", s.combis[i].placement);
            int t = 0;
            for (auto* timbre : child->getChildIterator())
            {
                if (! timbre->hasTagName ("Timbre") || t >= kTimbres)
                    continue;
                readTimbre (*timbre, s.combis[i].timbre[t]);
                ++t;
            }
            readArp (*child, s.combis[i].arp);
            readFx (*child, "Fx1", s.combis[i].fx1);
            readFx (*child, "Fx2", s.combis[i].fx2);
            ++i;
        }
        s.numCombis = i;
    }
    if (auto* kits = xml.getChildByName ("Kits"))
    {
        int i = 0;
        for (auto* child : kits->getChildIterator())
        {
            if (! child->hasTagName ("Kit") || i >= kKits)
                continue;
            if (child->hasAttribute ("name"))
                copyName (s.kits[i].name, child->getStringAttribute ("name").toRawUTF8());
            int n = 0;
            for (auto* key : child->getChildIterator())
            {
                if (! key->hasTagName ("Key") || n >= kDrumKeys)
                    continue;
                auto& d = s.kits[i].key[n];
                d.sample = attr (*key, "sample", d.sample); d.tune = attr (*key, "tune", d.tune);
                d.level = attr (*key, "level", d.level); d.decay = attr (*key, "decay", d.decay);
                d.pan = attr (*key, "pan", d.pan); d.group = attr (*key, "group", d.group);
                d.note = attr (*key, "note", d.note);
                ++n;
            }
            ++i;
        }
    }
    if (s.numPrograms > 0)
        s.currentProgram = clampInt (s.currentProgram, 0, s.numPrograms - 1);
    if (s.numCombis > 0)
        s.currentCombi = clampInt (s.currentCombi, 0, s.numCombis - 1);
    return s;
}

} // namespace m1
