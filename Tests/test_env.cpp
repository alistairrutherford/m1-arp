#include "Envelope.h"
#include "Filter.h"
#include "check.h"
#include <cmath>

int main()
{
    std::cout << "Envelope\n";
    const double sr = 48000.0;
    check (std::abs (m1::envelopeSeconds (0) - 0.002) < 1.0e-12, "time 0 is 2 ms");
    check (std::abs (m1::envelopeSeconds (99) - 5.0) < 1.0e-9, "time 99 is 5 s");
    const int attack0 = (int) std::llround (m1::envelopeSeconds (0) * sr);
    const int attack99 = (int) std::llround (m1::envelopeSeconds (99) * sr);
    check (attack0 == 96, "time 0 is 96 samples at 48 kHz");
    check (attack99 == 240000, "time 99 is 240000 samples at 48 kHz");

    m1::PitchEg peg;
    peg.start = 0;
    peg.at = 0;
    peg.al = 99;
    peg.dt = 0;
    peg.rt = 0;
    peg.rl = -99;
    m1::PitchEnvelope pitch;
    pitch.noteOn (peg, 0.5f, sr);
    check (pitch.stageIndex() == 0, "pitch starts in attack");
    int enteredDecay = -1;
    int enteredSustain = -1;
    for (int i = 0; i < 400; ++i)
    {
        pitch.process();
        if (pitch.stageIndex() == 1 && enteredDecay < 0)
            enteredDecay = i;
        if (pitch.stageIndex() == 2 && enteredSustain < 0)
            enteredSustain = i;
    }
    check (enteredDecay == 95, "pitch attack ends on the 96th sample");
    check (enteredSustain == 191, "pitch decay ends on the following 96 samples");
    check (std::abs (pitch.level()) < 1.0f, "pitch sustain holds zero, with no slope level");
    pitch.noteOff();
    check (pitch.stageIndex() == 3, "pitch release has no extra breakpoint");
    for (int i = 0; i < 96; ++i)
        pitch.process();
    check (pitch.stageIndex() == 4, "pitch release finishes in 96 samples");
    check (std::abs (pitch.level() - (-1200.0f)) < 1.0f, "pitch release level is -1200 cents");

    m1::Eg4 vdf;
    vdf.at = 0;
    vdf.al = -99;
    vdf.dt = 0;
    vdf.bp = 0;
    m1::SegmentEnvelope filterEg;
    filterEg.noteOn (vdf, sr, true, nullptr);
    for (int i = 0; i < 96; ++i)
        filterEg.process();
    check (std::abs (filterEg.level() - (-1.0f)) < 0.02f, "VDF attack level -99 is -1");

    m1::Eg4 amp;
    amp.at = 0;
    amp.al = 99;
    amp.dt = 0;
    amp.bp = 0;
    m1::SegmentEnvelope vda;
    vda.noteOn (amp, sr, false, nullptr);
    for (int i = 0; i < 96; ++i)
        vda.process();
    check (std::abs (vda.level() - 1.0f) < 0.02f, "VDA level 99 is unity");

    m1::Eg4 quiet;
    quiet.at = 0;
    quiet.al = 0;
    m1::SegmentEnvelope vda0;
    vda0.noteOn (quiet, sr, false, nullptr);
    for (int i = 0; i < 96; ++i)
        vda0.process();
    check (std::abs (vda0.level()) < 0.02f, "VDA level 0 stays silent");

    m1::Eg4 clamped;
    clamped.at = 0;
    clamped.al = -40;
    m1::SegmentEnvelope vdaNeg;
    vdaNeg.noteOn (clamped, sr, false, nullptr);
    for (int i = 0; i < 96; ++i)
        vdaNeg.process();
    check (vdaNeg.level() >= 0.0f, "VDA levels stay inside 0-99");

    check (std::abs (m1::Vdf::egOctaves (1.0f, 99) - 4.0) < 1.0e-9, "VDF intensity 99 is +4 octaves");
    check (std::abs (m1::Vdf::egOctaves (-1.0f, 99) - (-4.0)) < 1.0e-9, "VDF intensity 99 is -4 octaves");
    check (std::abs (m1::Vdf::egOctaves (1.0f, 0)) < 1.0e-9, "VDF intensity 0 scales the level to silence");

    return gFailures == 0 ? 0 : 1;
}
