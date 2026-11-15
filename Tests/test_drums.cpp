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
        session.numPrograms = 1;
        auto& p = session.programs[0];
        p.mode = (int) m1::OscMode::Drums;
        p.hold = 1;
        p.kit = 0;
        p.cutoff1 = 99;
        for (int i = 0; i < m1::kDrumKeys; ++i)
        {
            session.kits[0].key[i].note = 0;
            session.kits[0].key[i].level = 0;
            session.kits[0].key[i].group = 0;
        }
        session.kits[0].key[0].note = 42;
        session.kits[0].key[0].sample = 2;
        session.kits[0].key[0].level = 99;
        session.kits[0].key[0].decay = 80;
        session.kits[0].key[0].group = 1;
        session.kits[0].key[1].note = 46;
        session.kits[0].key[1].sample = 3;
        session.kits[0].key[1].level = 99;
        session.kits[0].key[1].decay = 80;
        session.kits[0].key[1].group = 1;
        session.currentProgram = 0;
    }

    void note (int n) { midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 110), 0); }
    void off (int n) { midi.addEvent (juce::MidiMessage::noteOff (1, n), 0); }

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
    std::cout << "Drums\n";
    {
        Rig rig;
        rig.note (42);
        check (rig.render (1000) > 0.01f, "closed hat sounds");
        check (rig.engine.notePlaying (42), "closed hat is held");
        rig.note (46);
        const float peak = rig.render (2000);
        check (peak > 0.01f, "open hat sounds");
        check (! rig.engine.notePlaying (42), "open hat replaces the closed hat");
        check (rig.engine.notePlaying (46), "open hat keeps the group");
    }
    {
        Rig rig;
        rig.note (42);
        rig.render (500);
        rig.off (42);
        const float held = rig.render (4000);
        check (rig.engine.notePlaying (42), "hold ignores note-off");
        check (held > 0.01f, "a held drum keeps ringing");
    }
    {
        Rig rig;
        rig.session.programs[0].hold = 0;
        rig.note (42);
        rig.render (400);
        rig.off (42);
        rig.render (8000);
        check (! rig.engine.notePlaying (42), "without hold, note-off ends the drum");
    }
    return gFailures == 0 ? 0 : 1;
}
