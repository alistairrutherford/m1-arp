#include "Filter.h"
#include "check.h"
#include <cmath>

int main()
{
    std::cout << "Filter\n";
    check (std::abs (m1::cutoffHz (0) - 100.0) < 1.0e-6, "cutoff 0 is 100 Hz");
    check (std::abs (m1::cutoffHz (99) - 16000.0) < 1.0e-4, "cutoff 99 is 16 kHz");

    const double centre = m1::effectiveCutoffHz (50, 60, 60, 0);
    const double octave = m1::effectiveCutoffHz (50, 72, 60, 0);
    check (std::abs (octave / centre - 2.0) < 1.0e-6, "tracking 0 moves cutoff one octave with the note");

    const double down = m1::effectiveCutoffHz (50, 48, 60, -99);
    check (std::abs (down - m1::cutoffHz (50)) < 1.0e-4, "tracking -99 does not move cutoff");

    static_assert (! m1::Vdf::kHasResonance, "the filter has no resonance coefficient");
    check (! m1::Vdf::kHasResonance, "resonance stays off");

    m1::Vdf vdf;
    vdf.setCutoffHz (1000.0, 48000.0);
    float y = 0.0f;
    for (int i = 0; i < 64; ++i)
        y = vdf.process (i == 0 ? 1.0f : 0.0f);
    check (std::abs (y) < 2.0f, "two one-poles stay bounded on an impulse");
    check (std::abs (vdf.currentHz() - 1000.0) < 1.0, "cutoff request is stored");
    return gFailures == 0 ? 0 : 1;
}
