#pragma once

#include "Model.h"

namespace m1
{

class Vdf
{
public:
    static constexpr bool kHasResonance = false;

    void reset()
    {
        y1 = y2 = 0.0f;
        g = 1.0f;
    }

    void setCutoffHz (double hz, double sr)
    {
        const double nyquist = sr * 0.45;
        const double fc = std::max (20.0, std::min (hz, nyquist));
        g = (float) (1.0 - std::exp (-2.0 * 3.14159265358979323846 * fc / sr));
        lastHz = fc;
    }

    double currentHz() const { return lastHz; }

    static double egOctaves (float egUnit, int intensity)
    {
        return (double) egUnit * (clampInt (intensity, 0, 99) / 99.0) * kVdfEgOctaves;
    }

    float process (float x)
    {
        y1 += g * (x - y1);
        y2 += g * (y1 - y2);
        return y2;
    }

private:
    float y1 = 0.0f, y2 = 0.0f, g = 1.0f;
    double lastHz = 100.0;
};

} // namespace m1
