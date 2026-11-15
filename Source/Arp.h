#pragma once

#include "Model.h"
#include <JuceHeader.h>
#include <limits>

namespace m1
{

class Arpeggiator
{
public:
    void prepare (double sr)
    {
        sampleRate = sr > 0 ? sr : 44100.0;
        scratch.ensureSize (8192);
        reset();
    }

    void setSeed (uint32_t s) { rng = s ? s : 1u; }

    void reset()
    {
        nHeld = 0;
        nDown = 0;
        nSeq = 0;
        nActive = 0;
        stepCounter = 0;
        nextStep = 0;
        freeQuarter = 0.0;
        lastQuarter = 0.0;
        latchArmed = false;
        sustain = false;
        wasHold = false;
        wasPlaying = false;
        samplesToOff = -1;
        haveLast = false;
    }

    void process (juce::MidiBuffer& midi, int numSamples, const ArpSettings& in,
                  double bpm, double ppqStart, bool playing)
    {
        ArpSettings p = in;
        p.octaves = clampInt (p.octaves, 1, 4);
        p.gate = clampInt (p.gate, 1, 100);
        p.swing = clampInt (p.swing, 50, 75);
        p.rate = clampInt (p.rate, 0, 9);
        if (bpm <= 0.0)
            bpm = 120.0;

        scratch.clear();
        juce::MidiBuffer& out = scratch;

        if (! p.on)
        {
            flush (out, 0);
            for (const auto meta : midi)
                out.addEvent (meta.getMessage(), meta.samplePosition);
            nHeld = 0;
            nDown = 0;
            nSeq = 0;
            latchArmed = false;
            midi.swapWith (out);
            return;
        }

        if (wasPlaying && ! playing)
            flush (out, 0);
        wasPlaying = playing;

        if (wasHold && ! p.hold)
        {
            int w = 0;
            for (int i = 0; i < nHeld; ++i)
                if (isDown (held[i].note))
                    held[w++] = held[i];
            if (w != nHeld)
                nHeld = w;
            latchArmed = false;
            rebuild (p);
            if (nSeq == 0)
                flush (out, 0);
        }
        wasHold = p.hold != 0;

        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            const int s = meta.samplePosition;
            if (m.isNoteOn())
            {
                noteOn (m.getNoteNumber(), m.getVelocity(), m.getChannel(), p);
            }
            else if (m.isNoteOff())
            {
                noteOff (m.getNoteNumber(), p);
            }
            else if (m.isController() && m.getControllerNumber() == 64)
            {
                const bool down = m.getControllerValue() >= 64;
                if (sustain && ! down)
                    releaseSustained (p);
                sustain = down;
                out.addEvent (m, s);
            }
            else
            {
                out.addEvent (m, s);
            }
        }

        const double S = ArpSettings::rateQuarter (p.rate);
        const double swing = p.swing / 100.0;
        const double inc = bpm / 60.0 / sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            double qn;
            if (playing)
            {
                qn = ppqStart + i * inc;
                if (haveLast && qn + 1.0e-4 < lastQuarter)
                {
                    flush (out, i);
                    nextStep = findStep (qn, S, swing);
                }
                lastQuarter = qn;
                haveLast = true;
                freeQuarter = qn + inc;
            }
            else
            {
                qn = freeQuarter;
                freeQuarter += inc;
            }

            if (samplesToOff > 0 && --samplesToOff == 0)
                releaseSounding (out, i, false);

            if (nSeq == 0 && nActive > 0)
                releaseSounding (out, i, false);

            int guard = 0;
            while (qn + 1.0e-9 >= boundary (nextStep, S, swing) && guard++ < 8)
            {
                const double nowB = boundary (nextStep, S, swing);
                const double nextB = boundary (nextStep + 1, S, swing);
                const double durQ = std::max (1.0e-4, nextB - nowB);
                const int stepSamples = std::max (1, (int) std::llround (durQ / inc));
                const int gateSamples = std::max (1, (int) std::lround (stepSamples * (p.gate / 100.0)));
                fire (out, i, p, gateSamples);
                ++nextStep;
            }
        }

        midi.swapWith (out);
    }

    int heldCount() const { return nHeld; }

private:
    struct Note
    {
        int note = 0;
        int vel = 100;
        int ch = 1;
        int order = 0;
    };

    double boundary (long long step, double S, double swing) const
    {
        if (step < 0)
            step = 0;
        const long long pair = step / 2;
        const int within = (int) (step % 2);
        const double base = (double) pair * 2.0 * S;
        if (within == 0)
            return base;
        return base + 2.0 * S * swing;
    }

    long long findStep (double qn, double S, double swing) const
    {
        long long guess = (long long) std::floor (qn / S);
        if (guess < 0)
            guess = 0;
        for (long long s = guess; s < guess + 6; ++s)
            if (boundary (s, S, swing) + 1.0e-9 >= qn)
                return s;
        return guess;
    }

    void noteOn (int note, int vel, int ch, const ArpSettings& p)
    {
        if (latchArmed)
        {
            nHeld = 0;
            latchArmed = false;
        }
        bool found = false;
        for (int i = 0; i < nDown; ++i)
            if (down[i] == note)
                found = true;
        if (! found && nDown < 16)
            down[nDown++] = note;

        for (int i = 0; i < nHeld; ++i)
            if (held[i].note == note)
                return;
        if (nHeld >= 16)
            return;
        held[nHeld++] = { note, vel, ch, orderCounter++ };
        rebuild (p);
    }

    void noteOff (int note, const ArpSettings& p)
    {
        int w = 0;
        for (int i = 0; i < nDown; ++i)
            if (down[i] != note)
                down[w++] = down[i];
        nDown = w;

        if (p.hold || sustain)
        {
            if (nDown == 0 && p.hold)
                latchArmed = true;
            return;
        }
        removeHeld (note);
        rebuild (p);
    }

    void releaseSustained (const ArpSettings& p)
    {
        if (p.hold)
            return;
        int w = 0;
        for (int i = 0; i < nHeld; ++i)
            if (isDown (held[i].note))
                held[w++] = held[i];
        nHeld = w;
        rebuild (p);
    }

    void removeHeld (int note)
    {
        int w = 0;
        for (int i = 0; i < nHeld; ++i)
            if (held[i].note != note)
                held[w++] = held[i];
        nHeld = w;
    }

    bool isDown (int note) const
    {
        for (int i = 0; i < nDown; ++i)
            if (down[i] == note)
                return true;
        return false;
    }

    void rebuild (const ArpSettings& p)
    {
        nSeq = 0;
        if (nHeld == 0)
            return;

        Note base[16];
        int n = nHeld;
        for (int i = 0; i < n; ++i)
            base[i] = held[i];

        if (p.mode != (int) ArpMode::AsPlayed && p.mode != (int) ArpMode::Random)
        {
            std::stable_sort (base, base + n, [] (const Note& a, const Note& b)
            {
                return a.note < b.note;
            });
        }

        const int octs = clampInt (p.octaves, 1, 4);
        for (int oct = 0; oct < octs; ++oct)
            for (int i = 0; i < n && nSeq < 64; ++i)
            {
                const int nn = base[i].note + 12 * oct;
                if (nn <= 127)
                    seq[nSeq++] = { nn, base[i].vel, base[i].ch, base[i].order };
            }

        if (p.mode == (int) ArpMode::Down || p.mode == (int) ArpMode::DownUp)
            std::reverse (seq, seq + nSeq);

        if (p.mode == (int) ArpMode::UpDown || p.mode == (int) ArpMode::DownUp)
        {
            const int nUp = nSeq;
            for (int i = nUp - 2; i >= 1 && nSeq < 64; --i)
                seq[nSeq++] = seq[i];
        }
    }

    void fire (juce::MidiBuffer& out, int samplePos, const ArpSettings& p, int gateSamples)
    {
        Note chord[64];
        int nChord = 0;
        const bool pattern = p.mode == (int) ArpMode::Pattern;
        const int step = (int) (stepCounter % kArpSteps);
        const bool stepOn = ! pattern || p.steps[step].on != 0;

        if (stepOn && nSeq > 0)
        {
            if (p.mode == (int) ArpMode::Chord)
            {
                for (int i = 0; i < nSeq && nChord < 64; ++i)
                    chord[nChord++] = seq[i];
            }
            else if (p.mode == (int) ArpMode::Random)
            {
                chord[nChord++] = seq[nextRand() % (uint32_t) nSeq];
            }
            else
            {
                int idx = (int) (melodic % nSeq);
                if (pattern)
                {
                    if (p.steps[step].on)
                    {
                        chord[0] = seq[idx];
                        const int nn = chord[0].note + 12 * clampInt (p.steps[step].octave, -2, 2);
                        chord[0].note = clampInt (nn, 0, 127);
                        chord[0].vel = clampInt (p.steps[step].velocity, 1, 127);
                        nChord = 1;
                        ++melodic;
                    }
                }
                else
                {
                    chord[0] = seq[idx];
                    nChord = 1;
                    ++melodic;
                }
            }
        }
        ++stepCounter;

        const bool canTie = p.gate >= 100 && p.mode != (int) ArpMode::Chord;
        const bool tie = canTie && sameChord (chord, nChord);
        if (! tie)
        {
            releaseSounding (out, samplePos, false);
            for (int i = 0; i < nChord; ++i)
            {
                out.addEvent (juce::MidiMessage::noteOn (chord[i].ch, chord[i].note,
                                                         (juce::uint8) clampInt (chord[i].vel, 1, 127)),
                              samplePos);
                if (nActive < 32)
                    active[nActive++] = chord[i];
            }
        }
        samplesToOff = (nChord > 0 && p.gate < 100) ? gateSamples : -1;
    }

    bool sameChord (const Note* chord, int n) const
    {
        if (n == 0 || n != nActive)
            return false;
        for (int i = 0; i < n; ++i)
            if (chord[i].note != active[i].note)
                return false;
        return true;
    }

    void releaseSounding (juce::MidiBuffer& out, int samplePos, bool tied)
    {
        if (tied)
            return;
        for (int i = 0; i < nActive; ++i)
            out.addEvent (juce::MidiMessage::noteOff (active[i].ch, active[i].note), samplePos);
        nActive = 0;
        samplesToOff = -1;
    }

    void flush (juce::MidiBuffer& out, int samplePos)
    {
        releaseSounding (out, samplePos, false);
    }

    uint32_t nextRand()
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    }

    double sampleRate = 48000.0;
    Note held[16];
    int nHeld = 0;
    int down[16];
    int nDown = 0;
    Note seq[64];
    int nSeq = 0;
    Note active[32];
    int nActive = 0;
    int orderCounter = 0;
    long long stepCounter = 0;
    long long melodic = 0;
    long long nextStep = 0;
    double freeQuarter = 0.0;
    double lastQuarter = 0.0;
    bool haveLast = false;
    bool latchArmed = false;
    bool sustain = false;
    bool wasHold = false;
    bool wasPlaying = false;
    int samplesToOff = -1;
    uint32_t rng = 1u;
    juce::MidiBuffer scratch;
};

} // namespace m1
