#include "Engine.h"
#include "check.h"
#include <cmath>
#include <fstream>
#include <vector>

namespace
{
struct Rig
{
    m1::Bank bank = m1::Bank::builtin();
    m1::Engine engine;
    m1::Session session;
    juce::MidiBuffer midi;

    Rig()
    {
        engine.prepare (48000.0, 8192);
        engine.setBank (&bank);
        session.numPrograms = 2;
        session.programs[0].osc1.multisound = 99;
        session.programs[0].osc1.level = 99;
        session.programs[0].cutoff1 = 99;
        session.programs[0].cutoff2 = 99;
        session.programs[0].egInt1 = 0;
        session.programs[0].egInt2 = 0;
        session.programs[0].vdf1.al = 0;
        session.programs[0].vdf1.bp = 0;
        session.programs[0].vdf1.sl = 0;
        session.programs[0].vda1.al = 99;
        session.programs[0].vda1.bp = 99;
        session.programs[0].vda1.sl = 99;
        session.programs[0].vda1.at = 0;
        session.programs[0].vda1.rt = 10;
        session.programs[1] = session.programs[0];
        session.currentProgram = 0;
    }

    void note (int n, int vel = 100, int ch = 1)
    {
        midi.addEvent (juce::MidiMessage::noteOn (ch, n, (juce::uint8) vel), 0);
    }

    float render (int samples)
    {
        juce::AudioBuffer<float> buf (2, samples);
        buf.clear();
        engine.process (buf.getWritePointer (0), buf.getWritePointer (1), samples, midi, session, {}, {});
        midi.clear();
        float peak = 0.0f;
        for (int i = 0; i < samples; ++i)
            peak = std::max (peak, std::abs (buf.getSample (0, i)));
        return peak;
    }
};
}

int main()
{
    std::cout << "Voice\n";
    const double plain = m1::phaseIncrement (44100.0, 44100.0, 60, 60, 0, 0.0);
    const double detuned = m1::phaseIncrement (44100.0, 44100.0, 60, 60, 0, -0.5);
    check (std::abs (detuned / plain - std::pow (2.0, -0.5 / 12.0)) < 1.0e-9, "detune -50 cents is -50 cents");

    {
        Rig rig;
        for (int n = 40; n < 56; ++n)
            rig.note (n);
        rig.note (56);
        const float peak = rig.render (2048);
        check (peak > 0.01f, "sixteen notes produce sound");
        check (rig.engine.slotsUsed() == 16, "a single keeps 16 slots");
        check (! rig.engine.notePlaying (40), "the 17th note steals the oldest");
        check (rig.engine.notePlaying (56), "the 17th note is playing");
        check (rig.engine.notePlaying (55), "a newer note is kept");
    }

    {
        Rig rig;
        rig.session.programs[0].mode = (int) m1::OscMode::Double;
        rig.session.programs[0].osc2.multisound = 99;
        rig.session.programs[0].osc2.level = 99;
        for (int n = 40; n < 48; ++n)
            rig.note (n);
        rig.note (70);
        rig.render (2048);
        check (rig.engine.slotsUsed() == 16, "double mode plays 8 notes");
        check (! rig.engine.notePlaying (40), "the 9th double note steals");
        check (rig.engine.notePlaying (70), "the 9th double note plays");
    }

    {
        Rig rig;
        rig.session.programs[0].mode = (int) m1::OscMode::Double;
        rig.session.programs[0].osc1.level = 0;
        rig.session.programs[0].osc2.multisound = 99;
        rig.session.programs[0].osc2.level = 99;
        rig.session.programs[0].delayStart = 99;
        rig.note (60);
        const float early = rig.render (20000);
        const float late = rig.render (8000);
        check (early < 1.0e-4f, "OSC2 stays silent through the delay");
        check (late > 0.01f, "OSC2 sounds after the delay");
    }

    {
        Rig rig;
        rig.session.programs[0].mode = (int) m1::OscMode::Double;
        rig.session.programs[0].osc2.multisound = 99;
        rig.session.programs[0].osc2.level = 99;
        rig.session.programs[0].detune = -50;
        rig.note (60);
        rig.render (128);
        const m1::Zone* zone = rig.bank.multi[99].find (60);
        const double expect = m1::phaseIncrement (zone->pcmRate, 48000.0, 60, zone->root, zone->tuneCents, -0.5);
        check (std::abs (rig.engine.debugInc (60, 1) - expect) < 1.0e-6, "voice detune matches the phase increment");
    }

    {
        Rig rig;
        rig.session.programs[0].osc1.multisound = 0;
        rig.note (20);
        const float peak = rig.render (1024);
        check (peak < 1.0e-5f, "a note outside the piano zone is silent");
        check (rig.engine.slotsUsed() == 0, "a silent zone does not take a slot");
    }

    {
        const char* path = "/tmp/m1arp-test.bank";
        {
            std::ofstream out (path, std::ios::binary);
            auto u32 = [&] (uint32_t v)
            {
                unsigned char b[4] = { (unsigned char) v, (unsigned char) (v >> 8),
                                       (unsigned char) (v >> 16), (unsigned char) (v >> 24) };
                out.write ((char*) b, 4);
            };
            auto u16 = [&] (uint16_t v)
            {
                unsigned char b[2] = { (unsigned char) v, (unsigned char) (v >> 8) };
                out.write ((char*) b, 2);
            };
            out.write ("M1BK", 4);
            u32 (1);
            u32 (1);
            u16 (91);
            u16 (1);
            u16 (60);
            u16 (0);
            u16 (127);
            u16 (0);
            u32 (0);
            u32 (8);
            u32 (8);
            for (int i = 0; i < 8; ++i)
                u16 (1000);
            u32 (0);
        }
        m1::Bank loaded = m1::Bank::builtin();
        check (loaded.loadFile (path), "M1BK bank loads");
        const m1::Zone* zone = loaded.multi[91].find (60);
        check (zone != nullptr && zone->pcm.size() == 8 && zone->pcm[0] == 1000, "packed zone replaces the builtin saw");
        std::remove (path);
    }

    return gFailures == 0 ? 0 : 1;
}
