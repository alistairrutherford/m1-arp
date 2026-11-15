#include "Arp.h"
#include "check.h"
#include <cmath>
#include <initializer_list>
#include <vector>

namespace
{
struct Hit
{
    int sample = 0;
    int note = -1;
    bool on = false;
    int cc = -1;
    int val = 0;
};

std::vector<Hit> hits (const juce::MidiBuffer& midi)
{
    std::vector<Hit> out;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        Hit h;
        h.sample = meta.samplePosition;
        if (m.isNoteOn())
        {
            h.on = true;
            h.note = m.getNoteNumber();
        }
        else if (m.isNoteOff())
        {
            h.note = m.getNoteNumber();
        }
        else if (m.isController())
        {
            h.cc = m.getControllerNumber();
            h.val = m.getControllerValue();
        }
        else
        {
            continue;
        }
        out.push_back (h);
    }
    return out;
}

int countOn (const std::vector<Hit>& h, int note = -1)
{
    int n = 0;
    for (const auto& e : h)
        if (e.on && e.cc < 0 && (note < 0 || e.note == note))
            ++n;
    return n;
}

bool onAt (const std::vector<Hit>& h, int sample, int note)
{
    for (const auto& e : h)
        if (e.on && e.cc < 0 && e.sample == sample && e.note == note)
            return true;
    return false;
}

bool offAt (const std::vector<Hit>& h, int sample, int note)
{
    for (const auto& e : h)
        if (! e.on && e.cc < 0 && e.sample == sample && e.note == note)
            return true;
    return false;
}

bool ccAt (const std::vector<Hit>& h, int cc, int val)
{
    for (const auto& e : h)
        if (e.cc == cc && e.val == val)
            return true;
    return false;
}

double boundary (long long step, double S, double swing)
{
    const long long pair = step / 2;
    const int within = (int) (step % 2);
    const double base = (double) pair * 2.0 * S;
    if (within == 0)
        return base;
    return base + 2.0 * S * swing;
}

int sampleOf (long long step, double S, double swing, double inc)
{
    const double b = boundary (step, S, swing);
    int i = 0;
    if (b > 0.0)
        i = (int) std::floor ((b - 1.0e-9) / inc);
    if (i < 0)
        i = 0;
    while (i * inc + 1.0e-9 < b)
        ++i;
    return i;
}

m1::ArpSettings base()
{
    m1::ArpSettings s;
    s.on = 1;
    s.mode = (int) m1::ArpMode::Up;
    s.rate = 9;
    s.octaves = 1;
    s.gate = 50;
    s.swing = 50;
    s.hold = 0;
    return s;
}

void addNotes (juce::MidiBuffer& midi, std::initializer_list<int> notes)
{
    for (int n : notes)
        midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
}
}

int main()
{
    std::cout << "Arpeggiator\n";
    const double sr = 48000.0;
    const double inc = 120.0 / 60.0 / sr;
    const double S = m1::ArpSettings::rateQuarter (9);
    check (std::abs (S - 0.125) < 1.0e-12, "rate index 9 is a 1/32 note");

    const int step1 = sampleOf (1, S, 0.5, inc);
    const int step2 = sampleOf (2, S, 0.5, inc);
    const int step3 = sampleOf (3, S, 0.5, inc);
    std::cout << "  grid samples " << step1 << " " << step2 << " " << step3 << "\n";

    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        juce::MidiBuffer midi;
        addNotes (midi, { 64, 60, 67 });
        arp.process (midi, step3 + 8, s, 120.0, 0.0, false);
        const auto h = hits (midi);
        check (onAt (h, 0, 60), "Up plays the lowest note first");
        check (onAt (h, step1, 64), "Up plays the middle note on the next step");
        check (onAt (h, step2, 67), "Up plays the highest note third");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.mode = (int) m1::ArpMode::Down;
        juce::MidiBuffer midi;
        addNotes (midi, { 60, 64, 67 });
        arp.process (midi, 16, s, 120.0, 0.0, false);
        check (onAt (hits (midi), 0, 67), "Down plays the highest note first");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.mode = (int) m1::ArpMode::UpDown;
        juce::MidiBuffer midi;
        addNotes (midi, { 60, 64, 67 });
        arp.process (midi, step3 + 8, s, 120.0, 0.0, false);
        const auto h = hits (midi);
        check (onAt (h, 0, 60) && onAt (h, step1, 64) && onAt (h, step2, 67) && onAt (h, step3, 64),
               "Up/Down walks up and turns back inside the chord");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.mode = (int) m1::ArpMode::AsPlayed;
        juce::MidiBuffer midi;
        addNotes (midi, { 67, 60, 64 });
        arp.process (midi, 16, s, 120.0, 0.0, false);
        check (onAt (hits (midi), 0, 67), "As Played keeps insertion order");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.octaves = 2;
        juce::MidiBuffer midi;
        addNotes (midi, { 60 });
        arp.process (midi, step2 + 8, s, 120.0, 0.0, false);
        const auto h = hits (midi);
        check (onAt (h, 0, 60) && onAt (h, step1, 72) && onAt (h, step2, 60), "two octaves wrap back to the bottom");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.mode = (int) m1::ArpMode::Chord;
        s.gate = 100;
        juce::MidiBuffer midi;
        addNotes (midi, { 60, 64, 67 });
        arp.process (midi, step1 + 8, s, 120.0, 0.0, false);
        const auto h = hits (midi);
        check (onAt (h, 0, 60) && onAt (h, 0, 64) && onAt (h, 0, 67), "Chord stacks every held note");
        check (countOn (h) > 3, "Chord retriggers at gate 100");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.gate = 100;
        juce::MidiBuffer midi;
        addNotes (midi, { 60 });
        arp.process (midi, step2 + 8, s, 120.0, 0.0, false);
        check (countOn (hits (midi), 60) == 1, "gate 100 ties a repeated note");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.gate = 50;
        juce::MidiBuffer midi;
        addNotes (midi, { 60 });
        const int frames = step1 + 8;
        arp.process (midi, frames, s, 120.0, 0.0, false);
        const double dur = boundary (1, S, 0.5) - boundary (0, S, 0.5);
        const int stepSamples = std::max (1, (int) std::llround (dur / inc));
        const int gateSamples = std::max (1, (int) std::lround (stepSamples * 0.5));
        const auto h = hits (midi);
        std::cout << "  gate off sample " << gateSamples << " step " << stepSamples << "\n";
        check (offAt (h, gateSamples, 60), "gate 50 releases halfway through the step");
        check (onAt (h, 0, 60), "the step still starts on the grid");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.swing = 75;
        s.gate = 40;
        juce::MidiBuffer midi;
        addNotes (midi, { 60 });
        const int a = sampleOf (1, S, 0.75, inc);
        const int b = sampleOf (2, S, 0.75, inc);
        arp.process (midi, b + 8, s, 120.0, 0.0, false);
        const auto h = hits (midi);
        std::cout << "  swing samples " << a << " " << b << "\n";
        check (onAt (h, 0, 60) && onAt (h, a, 60) && onAt (h, b, 60), "swing 75 delays the odd step");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.mode = (int) m1::ArpMode::Pattern;
        s.gate = 80;
        for (int i = 0; i < m1::kArpSteps; ++i)
            s.steps[i].on = (i % 4 == 1) ? 0 : 1;
        s.steps[0].octave = 1;
        s.steps[0].velocity = 70;
        juce::MidiBuffer midi;
        addNotes (midi, { 60, 64 });
        arp.process (midi, step3 + 8, s, 120.0, 0.0, false);
        const auto h = hits (midi);
        check (onAt (h, 0, 72), "a pattern octave offsets the sounding note");
        check (! onAt (h, step1, 60) && ! onAt (h, step1, 64) && ! onAt (h, step1, 72),
               "a pattern off-step is a rest");
        check (onAt (h, step2, 64), "the next on-step continues the melody");
        bool vel = false;
        for (const auto meta : midi)
            if (meta.getMessage().isNoteOn() && meta.getMessage().getVelocity() == 70)
                vel = true;
        check (vel, "a pattern step supplies its own velocity");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.hold = 1;
        s.gate = 100;
        juce::MidiBuffer midi;
        addNotes (midi, { 60 });
        arp.process (midi, 64, s, 120.0, 0.0, false);
        check (arp.heldCount() == 1, "hold keeps the pressed key");
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        arp.process (midi, 64, s, 120.0, 0.0, false);
        check (arp.heldCount() == 1, "hold ignores note-off");
        s.hold = 0;
        midi.clear();
        arp.process (midi, 64, s, 120.0, 0.0, false);
        check (arp.heldCount() == 0, "releasing hold drops keys that are up");
        check (offAt (hits (midi), 0, 60), "releasing hold flushes the sounding note");
    }
    {
        auto run = []
        {
            m1::Arpeggiator arp;
            arp.prepare (48000.0);
            arp.setSeed (1);
            arp.reset();
            auto s = base();
            s.mode = (int) m1::ArpMode::Random;
            s.gate = 40;
            juce::MidiBuffer midi;
            addNotes (midi, { 60, 64, 67, 72 });
            arp.process (midi, 16000, s, 120.0, 0.0, false);
            std::vector<int> notes;
            for (const auto meta : midi)
                if (meta.getMessage().isNoteOn())
                    notes.push_back (meta.getMessage().getNoteNumber());
            return notes;
        };
        check (run() == run(), "a seeded random pattern repeats after reset");
        check (! run().empty(), "random mode emits notes");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.gate = 100;
        juce::MidiBuffer midi;
        addNotes (midi, { 60 });
        arp.process (midi, 400, s, 120.0, 0.0, true);
        midi.clear();
        arp.process (midi, 200, s, 120.0, 0.0, true);
        check (offAt (hits (midi), 0, 60), "a backward PPQ jump flushes the sounding note");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.gate = 100;
        juce::MidiBuffer midi;
        addNotes (midi, { 60 });
        arp.process (midi, 200, s, 120.0, 1.0, true);
        s.on = 0;
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 90), 0);
        arp.process (midi, 64, s, 120.0, 1.0, true);
        const auto h = hits (midi);
        check (offAt (h, 0, 60), "arp off flushes generated notes");
        check (onAt (h, 0, 62), "arp off passes the input through");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        juce::MidiBuffer midi;
        addNotes (midi, { 60 });
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 40), 0);
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
        arp.process (midi, 32, s, 120.0, 0.0, false);
        const auto h = hits (midi);
        check (ccAt (h, 1, 40), "CC1 passes through");
        check (ccAt (h, 64, 127), "CC64 passes through");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.gate = 100;
        juce::MidiBuffer midi;
        addNotes (midi, { 60 });
        arp.process (midi, 128, s, 120.0, 0.0, false);
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        arp.process (midi, 128, s, 120.0, 0.0, false);
        check (arp.heldCount() == 0, "releasing the last key clears the held set");
        check (offAt (hits (midi), 0, 60) || offAt (hits (midi), 1, 60), "releasing the last key sends note-off");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.gate = 100;
        juce::MidiBuffer midi;
        addNotes (midi, { 60 });
        arp.process (midi, 1000, s, 120.0, 0.0, true);
        midi.clear();
        arp.process (midi, 64, s, 120.0, 0.0, false);
        check (offAt (hits (midi), 0, 60), "transport stop flushes generated notes");
    }
    {
        m1::Arpeggiator arp;
        arp.prepare (sr);
        auto s = base();
        s.gate = 40;
        juce::MidiBuffer midi;
        addNotes (midi, { 60 });
        arp.process (midi, 1000, s, 120.0, 0.0, false);
        const double q = 1000.0 * inc;
        midi.clear();
        const int frames = 8000;
        arp.process (midi, frames, s, 60.0, 0.0, false);
        const double incSlow = 60.0 / 60.0 / sr;
        int found = -1;
        for (const auto& e : hits (midi))
            if (e.on && e.note == 60)
            {
                found = e.sample;
                break;
            }
        const double remain = boundary (1, S, 0.5) - q;
        const int expect = (int) std::floor ((remain - 1.0e-9) / incSlow);
        int sample = expect < 0 ? 0 : expect;
        while (q + sample * incSlow + 1.0e-9 < boundary (1, S, 0.5))
            ++sample;
        std::cout << "  tempo next step " << found << " expect " << sample << "\n";
        check (found == sample, "a tempo change waits for the next step boundary");
    }
    return gFailures == 0 ? 0 : 1;
}
