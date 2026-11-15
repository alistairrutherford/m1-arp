#include "Effects.h"
#include "check.h"
#include <cmath>
#include <vector>

namespace
{
int peakIndex (const std::vector<float>& x, int from)
{
    int at = from;
    float best = 0.0f;
    for (int i = from; i < (int) x.size(); ++i)
        if (std::abs (x[(size_t) i]) > best)
        {
            best = std::abs (x[(size_t) i]);
            at = i;
        }
    return at;
}
}

int main()
{
    std::cout << "Effects\n";
    check (m1::Fx::dryGain (0) == 1.0f && m1::Fx::wetGain (0) == 0.0f, "balance 0 is dry");
    check (m1::Fx::wetGain (98) == 1.0f && m1::Fx::dryGain (98) == 0.0f, "balance 98 is wet");

    const double sr = 48000.0;
    m1::EffectRack rack;
    rack.prepare (sr, 8192);

    std::vector<float> impulse (4000, 0.0f), right (4000, 0.0f);
    impulse[0] = 0.8f;
    right[0] = 0.8f;
    auto dryL = impulse;
    auto dryR = right;
    m1::Fx hall;
    hall.type = 1;
    hall.balance = 0;
    rack.set (hall, {}, (int) m1::Placement::Serial);
    rack.process (impulse.data(), right.data(), (int) impulse.size());
    bool same = true;
    for (size_t i = 0; i < impulse.size(); ++i)
        if (impulse[i] != dryL[i] || right[i] != dryR[i])
            same = false;
    check (same, "dry:effect 99:1 equals the dry input");

    const int d1 = m1::delaySamplesFromKnob (0, sr);
    const int d2 = m1::delaySamplesFromKnob (5, sr);
    m1::Fx delayA;
    delayA.type = 10;
    delayA.balance = 98;
    delayA.p[0] = 0;
    delayA.p[1] = 0;
    delayA.p[2] = 0;
    delayA.p[3] = 0;
    m1::Fx delayB = delayA;
    delayB.p[0] = 5;
    delayB.p[1] = 5;

    std::vector<float> sL (4000, 0.0f), sR (4000, 0.0f);
    sL[0] = 1.0f;
    sR[0] = 1.0f;
    rack.reset();
    rack.set (delayA, delayB, (int) m1::Placement::Serial);
    rack.process (sL.data(), sR.data(), (int) sL.size());
    check (std::abs (sL[0]) < 1.0e-4f, "serial full-wet delays remove the dry impulse");
    const int serialPeak = peakIndex (sL, 1);
    check (std::abs (serialPeak - (d1 + d2)) <= 1, "serial delays peak at d1 + d2");

    std::vector<float> pL (4000, 0.0f), pR (4000, 0.0f);
    pL[0] = 1.0f;
    pR[0] = 1.0f;
    rack.reset();
    rack.set (delayA, delayB, (int) m1::Placement::Parallel);
    rack.process (pL.data(), pR.data(), (int) pL.size());
    check (std::abs (pL[0] - 1.0f) < 1.0e-3f, "parallel keeps the dry impulse");
    check (std::abs (pL[(size_t) d1]) > 0.5f, "parallel peaks on the first delay");
    check (std::abs (pL[(size_t) d2]) > 0.5f, "parallel peaks on the second delay");

    m1::Fx ensemble;
    ensemble.type = 24;
    m1::Fx chorus;
    chorus.type = 12;
    rack.set (ensemble, chorus, (int) m1::Placement::Serial);
    check (rack.secondType() == 0, "effect 24 refuses a chorus partner");
    check (! m1::effectPairAllowed (24, 12), "pairing rule rejects ensemble with chorus");
    check (m1::effectPairAllowed (1, 10), "hall may pair with delay");

    check (m1::EffectRack::tailSeconds (1, 0) == 8.0, "reverb reports an 8 second tail");
    check (m1::EffectRack::tailSeconds (10, 0) == 8.0, "delay reports an 8 second tail");
    check (m1::EffectRack::tailSeconds (20, 12) == 0.1, "EQ and chorus report a short tail");
    return gFailures == 0 ? 0 : 1;
}
