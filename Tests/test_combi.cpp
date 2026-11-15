#include "Engine.h"
#include "check.h"
#include <cmath>

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
        for (int i = 0; i < 2; ++i)
        {
            auto& p = session.programs[i];
            p.osc1.multisound = 99;
            p.osc1.level = 99;
            p.cutoff1 = 99;
            p.egInt1 = 0;
            p.vdf1.al = 0;
            p.vdf1.bp = 0;
            p.vdf1.sl = 0;
            p.vda1.al = 99;
            p.vda1.bp = 99;
            p.vda1.sl = 99;
            p.vda1.at = 0;
        }
        session.programs[0].osc1.level = 0;
        session.currentProgram = 0;
        session.combiMode = 1;
        session.numCombis = 1;
        session.currentCombi = 0;
        auto& c = session.combis[0];
        c.fx1.type = 0;
        c.fx2.type = 0;
        for (int i = 0; i < m1::kTimbres; ++i)
        {
            c.timbre[i].level = 99;
            c.timbre[i].keyLo = 0;
            c.timbre[i].keyHi = 127;
            c.timbre[i].velLo = 1;
            c.timbre[i].velHi = 127;
            c.timbre[i].channel = 1;
            c.timbre[i].program = 0;
            c.timbre[i].damper = 1;
        }
    }

    void note (int n, int vel = 100, int ch = 1)
    {
        midi.addEvent (juce::MidiMessage::noteOn (ch, n, (juce::uint8) vel), 0);
    }

    float render (int samples = 1024)
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
    std::cout << "Combinations\n";
    {
        Rig rig;
        rig.session.combis[0].type = (int) m1::CombiType::Split;
        rig.session.combis[0].splitKey = 60;
        rig.session.combis[0].timbre[0].program = 0;
        rig.session.combis[0].timbre[1].program = 1;
        rig.session.combis[0].timbre[1].channel = 1;
        rig.note (50);
        check (rig.render() < 1.0e-4f, "below the split plays the lower program");
        rig.note (72);
        check (rig.render() > 0.01f, "at the split key the upper program plays");
    }
    {
        Rig rig;
        rig.session.combis[0].type = (int) m1::CombiType::Velocity;
        rig.session.combis[0].velPoint = 80;
        rig.session.combis[0].timbre[0].program = 0;
        rig.session.combis[0].timbre[1].program = 1;
        rig.note (60, 60);
        check (rig.render() < 1.0e-4f, "a soft note takes the lower velocity program");
        rig.note (64, 100);
        check (rig.render() > 0.01f, "a loud note takes the upper velocity program");
    }
    {
        Rig rig;
        rig.session.combis[0].type = (int) m1::CombiType::Multi;
        rig.session.programs[0].osc1.level = 99;
        rig.session.combis[0].timbre[0].program = 0;
        rig.session.combis[0].timbre[0].channel = 2;
        rig.session.combis[0].timbre[0].keyLo = 48;
        rig.session.combis[0].timbre[0].keyHi = 60;
        rig.session.programs[1].osc1.level = 0;
        rig.session.combis[0].timbre[1].program = 1;
        rig.session.combis[0].timbre[1].channel = 3;
        rig.session.combis[0].timbre[1].level = 0;
        for (int t = 2; t < m1::kTimbres; ++t)
        {
            rig.session.combis[0].timbre[t].channel = 9;
            rig.session.combis[0].timbre[t].level = 0;
        }
        rig.note (50, 100, 2);
        check (rig.render() > 0.01f && rig.engine.notePlaying (50), "multi plays a note inside the channel and key window");
        rig.note (70, 100, 2);
        rig.render();
        check (! rig.engine.notePlaying (70), "multi rejects a note above the key window");
        const int voices = rig.engine.activeCount();
        rig.note (50, 100, 3);
        rig.render();
        check (rig.engine.activeCount() == voices, "a silent other channel does not start a voice");
        check (rig.engine.notePlaying (50), "the original channel keeps its note");
    }
    {
        Rig rig;
        rig.session.programs[0].osc1.level = 99;
        rig.session.programs[0].mode = (int) m1::OscMode::Double;
        rig.session.programs[0].osc2.multisound = 99;
        rig.session.programs[0].osc2.level = 99;
        rig.session.programs[1].mode = (int) m1::OscMode::Single;
        rig.session.combis[0].type = (int) m1::CombiType::Multi;
        rig.session.combis[0].timbre[0].program = 0;
        rig.session.combis[0].timbre[0].channel = 1;
        rig.session.combis[0].timbre[1].program = 1;
        rig.session.combis[0].timbre[1].channel = 2;
        for (int t = 2; t < m1::kTimbres; ++t)
        {
            rig.session.combis[0].timbre[t].channel = 9;
            rig.session.combis[0].timbre[t].level = 0;
        }
        for (int n = 40; n < 47; ++n)
            rig.note (n, 100, 1);
        rig.render (256);
        check (rig.engine.slotsUsed() == 14, "seven double notes use 14 slots");
        rig.note (60, 100, 2);
        rig.render (256);
        check (rig.engine.slotsUsed() == 15, "a single timbre shares the remaining slot");
        check (rig.engine.notePlaying (60), "the single timbre is heard");
    }
    return gFailures == 0 ? 0 : 1;
}
