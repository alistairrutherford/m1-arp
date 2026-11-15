#pragma once

#include "Bank.h"
#include "Envelope.h"
#include "Filter.h"

namespace m1
{

struct Controllers
{
    float bend = 0.0f;
    float pressure = 0.0f;
    float mod = 0.0f;
    float expression = 1.0f;
    bool damper = false;
};

inline int octaveSemis (int octave)
{
    if (octave >= -1 && octave <= 1)
        return octave * 12;
    return clampInt (octave, -12, 12);
}

inline float mgWave (int shape, double phase)
{
    const double p = phase - std::floor (phase);
    if (shape == 1)
        return (float) (p * 2.0 - 1.0);
    if (shape == 2)
        return (float) (1.0 - p * 2.0);
    if (shape == 3)
        return p < 0.5 ? 1.0f : -1.0f;
    if (p < 0.5)
        return (float) (p * 4.0 - 1.0);
    return (float) (3.0 - p * 4.0);
}

inline void segmentScales (const VelPage& vel, const KbdPage& kbd, float velocity, int note, float out[4])
{
    const float vw = std::max (0.0f, std::min (1.0f, velocity));
    const float kw = std::max (0.0f, std::min (1.0f, (note - kbd.centre) / 24.0f));
    const int vPol[4] = { vel.atPol, vel.dtPol, vel.stPol, vel.rtPol };
    const int kPol[4] = { kbd.atPol, kbd.dtPol, kbd.stPol, kbd.rtPol };
    for (int i = 0; i < 4; ++i)
    {
        const float vs = timePolarityScale (vPol[i], vel.egTime, vw) / 1000.0f;
        const float ks = timePolarityScale (kPol[i], kbd.egTime, kw) / 1000.0f;
        out[i] = std::max (0.05f, vs * ks);
    }
}

struct NoteRequest
{
    Program program {};
    const DrumKit* kit = nullptr;
    int note = 60;
    int vel = 100;
    int channel = 1;
    int timbre = 0;
    int transpose = 0;
    int detune = 0;
    int pan = 0;
    int level = 99;
    bool damperEnabled = true;
    double pitchPhase = 0.0;
    double vdfPhase = 0.0;
};

struct VoiceMidiEvent
{
    int sample = 0;
    int on = 0;
    int channel = 1;
    int note = 60;
    int vel = 100;
};

class Voice
{
public:
    bool active = false;
    bool doomed = false;
    bool released = false;
    bool pedalHeld = false;
    bool hold = false;
    bool drum = false;
    bool damperEnabled = true;
    int note = 60;
    int soundingNote = 60;
    int vel = 100;
    int channel = 1;
    int timbre = 0;
    int slots = 1;
    int group = 0;
    int age = 0;
    float fade = 1.0f;
    double debugInc[2] = {};

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0 ? sampleRate : 44100.0;
        cutCoef = (float) (1.0 - std::exp (-1.0 / (0.005 * sr)));
        doomLength = std::max (1, (int) std::llround (0.005 * sr));
    }

    bool start (const Bank& bank, const NoteRequest& req, int ageIn, int sample, VoiceMidiEvent* midi, int& nMidi, int midiCap)
    {
        srNote (req);
        age = ageIn;
        prog = req.program;
        kitPtr = req.kit;
        hold = req.program.hold != 0;
        damperEnabled = req.damperEnabled;
        released = false;
        pedalHeld = false;
        doomed = false;
        fade = 1.0f;
        if (req.program.mode == (int) OscMode::Drums)
        {
            if (! bindDrum (bank, req))
                return false;
        }
        else if (! bindMelodic (bank, req))
        {
            return false;
        }
        if (prog.pitchMg.keySync)
            pitchPhase = 0.0;
        else
            pitchPhase = req.pitchPhase;
        if (prog.vdfMg.keySync)
            vdfPhase = 0.0;
        else
            vdfPhase = req.vdfPhase;
        pitchDelay = delaySamples (prog.pitchMg.delay);
        vdfDelay = delaySamples (prog.vdfMg.delay);
        active = true;
        emit (midi, nMidi, midiCap, sample, true);
        return true;
    }

    void retrigger (const Bank& bank, const NoteRequest& req, int sample, VoiceMidiEvent* midi, int& nMidi, int midiCap)
    {
        const int previous = note;
        if (previous != req.note)
            emit (midi, nMidi, midiCap, sample, false);
        srNote (req);
        prog = req.program;
        kitPtr = req.kit;
        hold = req.program.hold != 0;
        damperEnabled = req.damperEnabled;
        released = false;
        pedalHeld = false;
        doomed = false;
        fade = 1.0f;
        const bool ok = drum ? bindDrum (bank, req) : bindMelodic (bank, req);
        if (! ok)
        {
            active = false;
            if (previous == req.note)
                emit (midi, nMidi, midiCap, sample, false);
            return;
        }
        if (prog.pitchMg.keySync)
            pitchPhase = 0.0;
        if (prog.vdfMg.keySync)
            vdfPhase = 0.0;
        pitchDelay = delaySamples (prog.pitchMg.delay);
        vdfDelay = delaySamples (prog.vdfMg.delay);
        if (previous != note)
            emit (midi, nMidi, midiCap, sample, true);
    }

    void releaseTail (int sample, VoiceMidiEvent* midi, int& nMidi, int midiCap)
    {
        if (! active || released || doomed)
            return;
        released = true;
        pedalHeld = false;
        if (drum)
            drumRelease = true;
        else
        {
            for (int i = 0; i < nOsc; ++i)
            {
                osc[i].pitch.noteOff();
                osc[i].vdfEg.noteOff();
                osc[i].vdaEg.noteOff();
            }
        }
        emit (midi, nMidi, midiCap, sample, false);
    }

    bool drumDone() const
    {
        return drum && (drumAmp <= 1.0e-4f || ! osc[0].enabled);
    }

    bool exhausted() const
    {
        if (drum)
            return false;
        for (int i = 0; i < nOsc; ++i)
            if (osc[i].has && osc[i].enabled)
                return false;
        return true;
    }

    void doom (int sample, VoiceMidiEvent* midi, int& nMidi, int midiCap)
    {
        if (! active || doomed)
            return;
        doomed = true;
        fadeStep = 1.0f / (float) doomLength;
        emit (midi, nMidi, midiCap, sample, false);
    }

    void deactivate (int sample, VoiceMidiEvent* midi, int& nMidi, int midiCap)
    {
        if (active)
            emit (midi, nMidi, midiCap, sample, false);
        active = false;
        doomed = false;
    }

    void mix (const Program& live, const Controllers& c, float sharedPitch, float sharedVdf, float& L, float& R)
    {
        if (! active)
            return;
        const float pitchW = live.pitchMg.keySync ? mgWave (live.pitchMg.wave, pitchPhase) : sharedPitch;
        const float vdfW = live.vdfMg.keySync ? mgWave (live.vdfMg.wave, vdfPhase) : sharedVdf;
        advanceMg (live);
        if (drum)
            mixDrum (live, c, L, R);
        else
            mixMelodic (live, c, pitchW, vdfW, L, R);
    }

    void tickFade()
    {
        if (! active || ! doomed)
            return;
        fade -= fadeStep;
        if (fade <= 0.0f)
        {
            fade = 0.0f;
            active = false;
            doomed = false;
        }
    }

    bool quiet() const
    {
        if (! active || doomed || ! released)
            return false;
        if (drum)
            return drumAmp < 1.0e-4f;
        for (int i = 0; i < nOsc; ++i)
            if (osc[i].has && osc[i].enabled && osc[i].vdaEg.level() > 1.0e-4f && ! osc[i].vdaEg.finished())
                return false;
        return true;
    }

    float ampLevel() const
    {
        if (! active)
            return 0.0f;
        if (drum)
            return drumAmp;
        float m = 0.0f;
        int count = 0;
        for (int i = 0; i < nOsc; ++i)
            if (osc[i].has)
            {
                m += std::max (0.0f, osc[i].vdaEg.level());
                ++count;
            }
        return count > 0 ? m / (float) count : 0.0f;
    }

private:
    struct Osc
    {
        const int16_t* pcm = nullptr;
        int n = 0;
        int loopStart = 0;
        int loopEnd = 0;
        bool oneShot = true;
        bool has = false;
        bool enabled = false;
        double phase = 0.0;
        double pcmRate = 44100.0;
        int root = 60;
        int tuneCents = 0;
        int delayLeft = 0;
        float cutSm = 1000.0f;
        PitchEnvelope pitch;
        SegmentEnvelope vdfEg;
        SegmentEnvelope vdaEg;
        Vdf vdf;
    };

    void srNote (const NoteRequest& req)
    {
        note = req.note;
        vel = clampInt (req.vel, 1, 127);
        channel = req.channel;
        timbre = clampInt (req.timbre, 0, kTimbres - 1);
        soundingNote = clampInt (req.note + req.transpose, 0, 127);
        timbreDetune = req.detune;
        timbreLevel = clampInt (req.level, 0, 99) / 99.0f;
        const int pan = clampInt (req.pan, -50, 50);
        const float ang = (pan + 50) / 100.0f * 1.5707963f;
        panL = std::cos (ang);
        panR = std::sin (ang);
    }

    int delaySamples (int time) const
    {
        return std::max (0, (int) std::llround (envelopeSeconds (time) * sr));
    }

    bool bindMelodic (const Bank& bank, const NoteRequest& req)
    {
        drum = false;
        nOsc = req.program.mode == (int) OscMode::Double ? 2 : 1;
        slots = nOsc;
        group = 0;
        bool any = false;
        const float velN = vel / 127.0f;
        for (int i = 0; i < 2; ++i)
        {
            osc[i] = Osc {};
            if (i >= nOsc)
                continue;
            const Oscillator& sp = i == 0 ? req.program.osc1 : req.program.osc2;
            if (sp.multisound < 0 || sp.multisound >= kMultisounds)
                continue;
            const Zone* zone = bank.multi[sp.multisound].find (soundingNote);
            if (zone == nullptr || zone->pcm.empty())
                continue;
            any = true;
            Osc& o = osc[i];
            o.has = true;
            o.enabled = sp.level > 0;
            o.pcm = zone->pcm.data();
            o.n = (int) zone->pcm.size();
            o.loopStart = std::max (0, zone->loopStart);
            o.loopEnd = zone->loopEnd;
            o.oneShot = zone->oneShot() || o.loopEnd <= o.loopStart;
            o.pcmRate = zone->pcmRate;
            o.root = zone->root;
            o.tuneCents = zone->tuneCents;
            o.phase = 0.0;
            o.delayLeft = (i == 1) ? (int) std::llround (osc2DelaySeconds (req.program.delayStart) * sr) : 0;
            const PitchEg& peg = i == 0 ? req.program.pitch1 : req.program.pitch2;
            o.pitch.noteOn (peg, velN, sr);
            const Eg4& vdfEg = i == 0 ? req.program.vdf1 : req.program.vdf2;
            const Eg4& vdaEg = i == 0 ? req.program.vda1 : req.program.vda2;
            const VelPage& vdfVel = i == 0 ? req.program.vdfVel1 : req.program.vdfVel2;
            const VelPage& vdaVel = i == 0 ? req.program.vdaVel1 : req.program.vdaVel2;
            const KbdPage& vdfKbd = i == 0 ? req.program.vdfKbd1 : req.program.vdfKbd2;
            const KbdPage& vdaKbd = i == 0 ? req.program.vdaKbd1 : req.program.vdaKbd2;
            float vdfS[4], vdaS[4];
            segmentScales (vdfVel, vdfKbd, velN, soundingNote, vdfS);
            segmentScales (vdaVel, vdaKbd, velN, soundingNote, vdaS);
            o.vdfEg.noteOn (vdfEg, sr, true, vdfS);
            o.vdaEg.noteOn (vdaEg, sr, false, vdaS);
            o.vdf.reset();
            o.cutSm = (float) cutoffHz (i == 0 ? req.program.cutoff1 : req.program.cutoff2);
            const int oct = octaveSemis (sp.octave);
            const int interval = i == 1 ? req.program.interval : 0;
            const double detune = ((i == 1 ? req.program.detune : 0) + timbreDetune) / 100.0;
            debugInc[i] = phaseIncrement (o.pcmRate, sr, soundingNote, o.root, o.tuneCents,
                                          oct + interval + detune + o.pitch.level() / 100.0);
        }
        return any;
    }

    bool bindDrum (const Bank& bank, const NoteRequest& req)
    {
        drum = true;
        nOsc = 1;
        slots = 1;
        group = 0;
        osc[0] = Osc {};
        osc[1] = Osc {};
        const DrumKey* key = findKey (req);
        if (key == nullptr)
            return false;
        const int sample = clampInt (key->sample, 0, kDrumSamples - 1);
        if (bank.drum[sample].pcm.empty())
            return false;
        group = clampInt (key->group, 0, 7);
        Osc& o = osc[0];
        o.has = true;
        o.enabled = key->level > 0;
        o.pcm = bank.drum[sample].pcm.data();
        o.n = (int) bank.drum[sample].pcm.size();
        o.oneShot = true;
        o.pcmRate = bank.drum[sample].pcmRate;
        o.root = soundingNote;
        o.tuneCents = 0;
        o.phase = 0.0;
        drumTune = (clampInt (key->tune, -50, 50) / 50.0) * 12.0;
        drumAmp = 1.0f;
        // Decay 99 plays the whole sample; lower values fade it out linearly.
        drumStep = key->decay >= 99 ? 0.0f
                                    : 1.0f / (float) std::max (1, (int) std::llround (envelopeSeconds (key->decay) * sr));
        drumRelease = false;
        drumLevel = clampInt (key->level, 0, 99) / 99.0f;
        const int pan = clampInt (req.pan + key->pan, -50, 50);
        const float ang = (pan + 50) / 100.0f * 1.5707963f;
        panL = std::cos (ang);
        panR = std::sin (ang);
        debugInc[0] = phaseIncrement (o.pcmRate, sr, soundingNote, o.root, 0, drumTune + timbreDetune / 100.0);
        debugInc[1] = 0.0;
        return o.n > 0;
    }

    const DrumKey* findKey (const NoteRequest& req) const
    {
        if (req.kit == nullptr)
            return nullptr;
        const DrumKey* fallback = nullptr;
        const int idx = req.note - kDrumKeyBase;
        for (int i = 0; i < kDrumKeys; ++i)
        {
            if (req.kit->key[i].note == req.note && req.kit->key[i].note != 0)
                return &req.kit->key[i];
            if (i == idx)
                fallback = &req.kit->key[i];
        }
        return fallback;
    }

    void advanceMg (const Program& live)
    {
        auto step = [&] (double& phase, const Mg& mg, int freqPush)
        {
            const int f = clampInt (mg.frequency + freqPush, 0, 99);
            phase += mgHz (f) / sr;
            if (phase >= 1.0)
                phase -= std::floor (phase);
        };
        const int pitchPush = (int) std::lround (/* filled by caller via controllers in mix */ 0);
        (void) pitchPush;
        step (pitchPhase, live.pitchMg, 0);
        step (vdfPhase, live.vdfMg, 0);
        if (pitchDelay > 0)
            --pitchDelay;
        if (vdfDelay > 0)
            --vdfDelay;
    }

    void mixMelodic (const Program& live, const Controllers& c, float pitchW, float vdfW, float& L, float& R)
    {
        const float velN = vel / 127.0f;
        const int pitchFreqPush = (int) std::lround (c.mod * live.joy.pitchMgFreq);
        const int vdfFreqPush = (int) std::lround (c.mod * live.joy.vdfMgFreq);
        if (pitchFreqPush != 0 || vdfFreqPush != 0)
        {
            pitchPhase += (mgHz (clampInt (live.pitchMg.frequency + pitchFreqPush, 0, 99))
                           - mgHz (live.pitchMg.frequency)) / sr;
            vdfPhase += (mgHz (clampInt (live.vdfMg.frequency + vdfFreqPush, 0, 99))
                         - mgHz (live.vdfMg.frequency)) / sr;
        }
        for (int i = 0; i < nOsc; ++i)
        {
            Osc& o = osc[i];
            if (! o.has)
                continue;
            const float peg = o.pitch.process();
            const float vdfEg = o.vdfEg.process();
            const float vdaEg = o.vdaEg.process();
            if (! o.enabled)
                continue;
            if (o.delayLeft > 0)
            {
                --o.delayLeft;
                continue;
            }
            if (o.pcm == nullptr || o.n <= 1)
                continue;
            const Oscillator& sp = i == 0 ? live.osc1 : live.osc2;
            const int cutoff = i == 0 ? live.cutoff1 : live.cutoff2;
            const int egInt = i == 0 ? live.egInt1 : live.egInt2;
            const VelPage& velPage = i == 0 ? live.vdfVel1 : live.vdfVel2;
            const VelPage& vdaVel = i == 0 ? live.vdaVel1 : live.vdaVel2;
            const KbdPage& kbd = i == 0 ? live.vdfKbd1 : live.vdfKbd2;
            const KbdPage& vdaKbd = i == 0 ? live.vdaKbd1 : live.vdaKbd2;
            double semis = octaveSemis (sp.octave) + (i == 1 ? live.interval : 0);
            semis += ((i == 1 ? live.detune : 0) + timbreDetune) / 100.0;
            semis += peg / 100.0;
            semis += c.bend * clampInt (live.joy.bend, -12, 12);
            semis += pitchMgSemis (live, c, i, pitchW) / 100.0;
            const double inc = phaseIncrement (o.pcmRate, sr, soundingNote, o.root, o.tuneCents, semis);
            const int i0 = std::max (0, std::min (o.n - 1, (int) o.phase));
            int i1 = i0 + 1;
            if (! o.oneShot && o.loopEnd > o.loopStart && i1 >= o.loopEnd)
                i1 = o.loopStart;
            if (i1 >= o.n)
                i1 = i0;
            const float frac = (float) (o.phase - (int) o.phase);
            const float sample = (o.pcm[i0] * (1.0f - frac) + o.pcm[i1] * frac) / 32768.0f;
            o.phase += inc;
            if (o.oneShot)
            {
                if (o.phase >= o.n - 1)
                    o.enabled = false;
            }
            else
            {
                const int loopLen = std::max (1, o.loopEnd - o.loopStart);
                if (o.phase >= o.loopEnd)
                    o.phase = o.loopStart + std::fmod (o.phase - o.loopStart, (double) loopLen);
            }

            const float egScale = std::max (0.0f, 1.0f + (velPage.egInt / 99.0f) * (velN * 2.0f - 1.0f));
            double oct = Vdf::egOctaves (vdfEg, egInt) * egScale;
            oct += c.pressure * (live.after.vdf / 99.0) * 2.0;
            oct += c.mod * (live.joy.vdfSweep / 99.0) * 2.0;
            oct += vdfMgOct (live, c, i, vdfW);
            const double hz = effectiveCutoffHz (cutoff, soundingNote, kbd.centre, kbd.amount) * std::pow (2.0, oct);
            o.cutSm += cutCoef * ((float) hz - o.cutSm);
            o.vdf.setCutoffHz (std::max (20.0f, o.cutSm), sr);

            float amp = std::max (0.0f, vdaEg);
            const float vScale = std::max (0.0f, 1.0f + (vdaVel.egInt / 99.0f) * (velN * 2.0f - 1.0f));
            const float kw = std::max (-1.0f, std::min (1.0f, (soundingNote - vdaKbd.centre) / 24.0f));
            const float kScale = std::max (0.0f, 1.0f + (vdaKbd.amount / 99.0f) * kw);
            amp *= (clampInt (sp.level, 0, 99) / 99.0f) * vScale * kScale * timbreLevel;
            const float db = c.pressure * (live.after.vda / 99.0f) * 12.0f;
            amp *= std::pow (10.0f, db / 20.0f);
            amp = std::min (amp, std::pow (10.0f, 6.0f / 20.0f));
            // Two oscillators per note sum to about the level of one.
            amp *= std::max (0.0f, c.expression) * fade * kVoiceGain * (nOsc == 2 ? 0.7f : 1.0f);
            const float y = o.vdf.process (sample) * amp;
            L += y * panL;
            R += y * panR;
        }
    }

    void mixDrum (const Program& live, const Controllers& c, float& L, float& R)
    {
        Osc& o = osc[0];
        if (! o.has || ! o.enabled || o.pcm == nullptr || o.n <= 1)
            return;
        const int i0 = std::max (0, std::min (o.n - 1, (int) o.phase));
        int i1 = std::min (o.n - 1, i0 + 1);
        const float frac = (float) (o.phase - (int) o.phase);
        const float sample = (o.pcm[i0] * (1.0f - frac) + o.pcm[i1] * frac) / 32768.0f;
        const double inc = phaseIncrement (o.pcmRate, sr, soundingNote, o.root, 0,
                                           drumTune + timbreDetune / 100.0 + c.bend * clampInt (live.joy.bend, -12, 12));
        o.phase += inc;
        if (o.phase >= o.n - 1)
            o.enabled = false;
        drumAmp = std::max (0.0f, drumAmp - drumStep);
        if (drumRelease)
            drumAmp = std::max (0.0f, drumAmp - 0.002f);
        float amp = drumAmp * drumLevel * timbreLevel * std::max (0.0f, c.expression) * fade * kDrumGain;
        L += sample * amp * panL;
        R += sample * amp * panR;
        (void) live;
    }

    float pitchMgSemis (const Program& live, const Controllers& c, int oscIndex, float wave) const
    {
        if (! mgSelects (live.pitchMg.oscSelect, oscIndex) || pitchDelay > 0)
            return 0.0f;
        float depth = (clampInt (live.pitchMg.intensity, 0, 99) / 99.0f) * (float) kPitchMgCents;
        depth *= 1.0f + c.pressure * (clampInt (live.after.pitchMg, 0, 99) / 99.0f);
        depth += c.pressure * live.after.pitch * 100.0f;
        depth *= 1.0f + c.mod * (clampInt (live.joy.pitchMg, 0, 99) / 99.0f);
        return wave * depth;
    }

    float vdfMgOct (const Program& live, const Controllers& c, int oscIndex, float wave) const
    {
        if (! mgSelects (live.vdfMg.oscSelect, oscIndex) || vdfDelay > 0)
            return 0.0f;
        float depth = (clampInt (live.vdfMg.intensity, 0, 99) / 99.0f) * (float) kVdfMgOctaves;
        depth *= 1.0f + c.pressure * (clampInt (live.after.vdfMg, 0, 99) / 99.0f);
        depth *= 1.0f + c.mod * (clampInt (live.joy.vdfMg, 0, 99) / 99.0f);
        return wave * depth;
    }

    static bool mgSelects (int select, int oscIndex)
    {
        if (select == 3)
            return true;
        if (select == 1)
            return oscIndex == 0;
        if (select == 2)
            return oscIndex == 1;
        return false;
    }

    void emit (VoiceMidiEvent* midi, int& nMidi, int midiCap, int sample, bool on)
    {
        if (midi == nullptr || nMidi >= midiCap)
            return;
        if (! on && ! noteLit)
            return;
        midi[nMidi++] = { sample, on ? 1 : 0, channel, note, vel };
        noteLit = on;
    }

    Program prog {};
    const DrumKit* kitPtr = nullptr;
    Osc osc[2];
    int nOsc = 1;
    double sr = 48000.0;
    float cutCoef = 0.01f;
    int doomLength = 240;
    float fadeStep = 0.01f;
    float panL = 0.707f;
    float panR = 0.707f;
    float timbreLevel = 1.0f;
    int timbreDetune = 0;
    double pitchPhase = 0.0;
    double vdfPhase = 0.0;
    int pitchDelay = 0;
    int vdfDelay = 0;
    float drumAmp = 0.0f;
    float drumStep = 0.001f;
    float drumLevel = 1.0f;
    double drumTune = 0.0;
    bool drumRelease = false;
    bool noteLit = false;
};

class VoicePool
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate > 0 ? sampleRate : 44100.0;
        for (auto& v : voices)
            v.prepare (sr);
        silence();
    }

    void silence()
    {
        for (auto& v : voices)
            if (v.active)
                v.deactivate (0, midi, nMidi, midiCap);
        nPending = 0;
    }

    void setBank (const Bank* b) { bank = b; }
    void setKits (const DrumKit* k) { kits = k; }

    int slotsUsed() const
    {
        int s = 0;
        for (const auto& v : voices)
            if (v.active)
                s += v.slots;
        return s;
    }

    int activeCount() const
    {
        int n = 0;
        for (const auto& v : voices)
            if (v.active && ! v.doomed)
                ++n;
        return n;
    }

    bool notePlaying (int note) const
    {
        for (const auto& v : voices)
            if (v.active && ! v.doomed && v.note == note)
                return true;
        return false;
    }

    double debugInc (int note, int osc) const
    {
        for (const auto& v : voices)
            if (v.active && v.note == note)
                return v.debugInc[osc < 0 ? 0 : (osc > 1 ? 1 : osc)];
        return 0.0;
    }

    void noteOn (NoteRequest req, int sample)
    {
        bindKit (req);
        if (bank == nullptr || ! audible (req))
            return;
        reap();
        for (auto& v : voices)
            if (v.active && ! v.doomed && v.note == req.note && v.timbre == req.timbre)
            {
                v.retrigger (*bank, req, sample, midi, nMidi, midiCap);
                return;
            }
        if (req.program.assign == (int) Assign::Mono && req.program.mode != (int) OscMode::Drums)
        {
            for (auto& v : voices)
                if (v.active && ! v.doomed && ! v.drum && v.timbre == req.timbre)
                {
                    v.retrigger (*bank, req, sample, midi, nMidi, midiCap);
                    return;
                }
        }
        if (req.program.mode == (int) OscMode::Drums)
            chokeGroup (req, sample);

        const int need = req.program.slotsPerNote();
        if (freeSlots() >= need && startNow (req, sample))
            return;
        enqueue (req);
        int covered = freeSlots();
        while (covered < need)
        {
            Voice* victim = pickVictim();
            if (victim == nullptr)
                break;
            covered += victim->slots;
            victim->doom (sample, midi, nMidi, midiCap);
        }
    }

    void noteOff (int note, int timbre, bool honorDamper, int sample)
    {
        for (auto& v : voices)
        {
            if (! v.active || v.doomed || v.note != note)
                continue;
            if (timbre >= 0 && v.timbre != timbre)
                continue;
            if (v.hold)
                continue;
            if (honorDamper && damper && v.damperEnabled)
            {
                v.pedalHeld = true;
                continue;
            }
            v.releaseTail (sample, midi, nMidi, midiCap);
        }
    }

    void setDamper (bool down, int sample)
    {
        if (damper && ! down)
        {
            for (auto& v : voices)
                if (v.active && v.pedalHeld && ! v.hold)
                    v.releaseTail (sample, midi, nMidi, midiCap);
        }
        damper = down;
        (void) sample;
    }

    void releaseAll (int sample)
    {
        for (auto& v : voices)
            if (v.active && ! v.released)
                v.releaseTail (sample, midi, nMidi, midiCap);
        nPending = 0;
    }

    bool audible (const NoteRequest& req) const
    {
        if (bank == nullptr)
            return false;
        if (req.program.mode == (int) OscMode::Drums)
        {
            if (req.kit == nullptr)
                return false;
            const DrumKey* key = nullptr;
            const int idx = req.note - kDrumKeyBase;
            for (int i = 0; i < kDrumKeys; ++i)
            {
                if (req.kit->key[i].note == req.note && req.kit->key[i].note != 0)
                {
                    key = &req.kit->key[i];
                    break;
                }
                if (i == idx)
                    key = &req.kit->key[i];
            }
            if (key == nullptr || key->level <= 0)
                return false;
            const int sample = clampInt (key->sample, 0, kDrumSamples - 1);
            return ! bank->drum[sample].pcm.empty();
        }
        const int sounding = clampInt (req.note + req.transpose, 0, 127);
        const int count = req.program.mode == (int) OscMode::Double ? 2 : 1;
        for (int i = 0; i < count; ++i)
        {
            const Oscillator& sp = i == 0 ? req.program.osc1 : req.program.osc2;
            if (sp.level <= 0 || sp.multisound < 0 || sp.multisound >= kMultisounds)
                continue;
            const Zone* zone = bank->multi[sp.multisound].find (sounding);
            if (zone != nullptr && ! zone->pcm.empty())
                return true;
        }
        return false;
    }

    void renderSample (const Program* live[kTimbres], const Controllers& c,
                       float sharedPitch[kTimbres], float sharedVdf[kTimbres], float& L, float& R, int sample)
    {
        for (auto& v : voices)
            if (v.active)
            {
                const Program& src = live[v.timbre] != nullptr ? *live[v.timbre] : vLiveFallback;
                v.mix (src, c, sharedPitch[v.timbre], sharedVdf[v.timbre], L, R);
            }
        for (auto& v : voices)
        {
            if (! v.active)
                continue;
            v.tickFade();
            if (v.active && v.quiet())
                v.deactivate (sample, midi, nMidi, midiCap);
            if (v.active && (v.drumDone() || v.exhausted()))
                v.deactivate (sample, midi, nMidi, midiCap);
        }
        reap();
        promote (sample);
    }

    VoiceMidiEvent midi[512];
    int nMidi = 0;
    static constexpr int midiCap = 512;

    bool damper = false;

private:
    int freeSlots() const { return kOscSlots - slotsUsed(); }

    void reap()
    {
        for (auto& v : voices)
            if (v.active && ! v.doomed && v.quiet())
                v.active = false;
    }

    Voice* pickVictim()
    {
        Voice* finished = nullptr;
        Voice* releasing = nullptr;
        float lowest = 1.0e9f;
        Voice* oldest = nullptr;
        for (auto& v : voices)
        {
            if (! v.active || v.doomed)
                continue;
            if (v.released && v.quiet())
                return &v;
            if (v.released && v.ampLevel() < lowest)
            {
                lowest = v.ampLevel();
                releasing = &v;
            }
            if (oldest == nullptr || v.age < oldest->age)
                oldest = &v;
            if (v.released && v.ampLevel() <= 1.0e-4f && finished == nullptr)
                finished = &v;
        }
        if (finished != nullptr)
            return finished;
        if (releasing != nullptr)
            return releasing;
        return oldest;
    }

    bool startNow (const NoteRequest& req, int sample)
    {
        for (auto& v : voices)
            if (! v.active)
            {
                if (! v.start (*bank, req, ageCounter++, sample, midi, nMidi, midiCap))
                    return false;
                return true;
            }
        return false;
    }

    void enqueue (const NoteRequest& req)
    {
        if (nPending >= pendingCap)
            return;
        pending[nPending++] = req;
    }

    void promote (int sample)
    {
        while (nPending > 0 && freeSlots() >= pending[0].program.slotsPerNote())
        {
            NoteRequest req = pending[0];
            bindKit (req);
            for (int i = 1; i < nPending; ++i)
                pending[i - 1] = pending[i];
            --nPending;
            if (freeSlots() < req.program.slotsPerNote())
                break;
            if (! startNow (req, sample))
                continue;
        }
    }

    void chokeGroup (const NoteRequest& req, int sample)
    {
        if (req.kit == nullptr)
            return;
        int group = 0;
        const int idx = req.note - kDrumKeyBase;
        for (int i = 0; i < kDrumKeys; ++i)
        {
            if (req.kit->key[i].note == req.note && req.kit->key[i].note != 0)
            {
                group = req.kit->key[i].group;
                break;
            }
            if (i == idx)
                group = req.kit->key[i].group;
        }
        if (group <= 0)
            return;
        for (auto& v : voices)
            if (v.active && ! v.doomed && v.drum && v.group == group && v.timbre == req.timbre)
                v.doom (sample, midi, nMidi, midiCap);
    }

    Voice voices[kOscSlots];
    NoteRequest pending[16];
    static constexpr int pendingCap = 16;
    int nPending = 0;
    int ageCounter = 1;
    void bindKit (NoteRequest& req) const
    {
        if (req.program.mode == (int) OscMode::Drums && kits != nullptr)
            req.kit = &kits[clampInt (req.program.kit, 0, kKits - 1)];
    }

    const Bank* bank = nullptr;
    const DrumKit* kits = nullptr;
    double sr = 48000.0;
    Program vLiveFallback {};
};

} // namespace m1
