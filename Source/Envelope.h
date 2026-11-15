#pragma once

#include "Model.h"

namespace m1
{

class PitchEnvelope
{
public:
    void noteOn (const PitchEg& eg, float velocity, double sr)
    {
        spec = eg;
        sampleRate = sr;
        vel = std::max (0.0f, std::min (1.0f, velocity));
        stage = 0;
        current = cents (eg.start);
        begin (cents (eg.al), eg.at);
    }

    void noteOff()
    {
        if (stage >= 3)
            return;
        stage = 3;
        begin (cents (spec.rl), spec.rt);
    }

    float process()
    {
        if (stage == 2 || stage == 4)
            return current;
        if (left <= 0)
        {
            current = target;
            advance();
            return current;
        }
        current += delta;
        if (--left <= 0)
        {
            current = target;
            advance();
        }
        return current;
    }

    int stageIndex() const { return stage; }
    float level() const { return current; }

private:
    float cents (int level) const
    {
        const float base = clampInt (level, -99, 99) / 99.0f * 1200.0f;
        const float sens = clampInt (spec.velLevel, -99, 99) / 99.0f;
        const float pushed = base * (1.0f + sens * (vel * 2.0f - 1.0f));
        return std::max (-1200.0f, std::min (1200.0f, pushed));
    }

    float timeScale() const
    {
        const float sens = clampInt (spec.velTime, -99, 99) / 99.0f;
        if (sens == 0.0f)
            return 1.0f;
        const float harder = vel;
        const float scale = sens > 0.0f ? 1.0f - sens * harder
                                        : 1.0f - (-sens) * (1.0f - harder);
        return std::max (0.05f, scale);
    }

    void begin (float to, int time)
    {
        target = to;
        const double sec = envelopeSeconds (time) * timeScale();
        left = std::max (1, (int) std::llround (sec * sampleRate));
        delta = (target - current) / (float) left;
    }

    void advance()
    {
        if (stage == 0)
        {
            stage = 1;
            begin (0.0f, spec.dt);
            return;
        }
        if (stage == 1)
        {
            stage = 2;
            current = 0.0f;
            return;
        }
        if (stage == 3)
        {
            stage = 4;
            current = target;
        }
    }

    PitchEg spec {};
    double sampleRate = 48000.0;
    float vel = 0.8f;
    float current = 0.0f;
    float target = 0.0f;
    float delta = 0.0f;
    int left = 0;
    int stage = 3;
};

class SegmentEnvelope
{
public:
    void noteOn (const Eg4& eg, double sr, bool bipolarIn, const float scalesIn[4])
    {
        spec = eg;
        sampleRate = sr;
        bipolar = bipolarIn;
        for (int i = 0; i < 4; ++i)
            scales[i] = std::max (0.05f, scalesIn != nullptr ? scalesIn[i] : 1.0f);
        stage = 0;
        current = 0.0f;
        begin (valueOf (eg.al), eg.at, 0);
    }

    void noteOff()
    {
        if (stage >= 4)
            return;
        stage = 4;
        begin (valueOf (spec.rl), spec.rt, 3);
    }

    float process()
    {
        if (stage == 3)
            return current;
        if (left <= 0)
        {
            current = target;
            advance();
            return current;
        }
        current += delta;
        if (--left <= 0)
        {
            current = target;
            advance();
        }
        return current;
    }

    bool finished() const { return stage == 5; }
    int stageIndex() const { return stage; }
    float level() const { return current; }

private:
    float valueOf (int level) const
    {
        if (bipolar)
            return clampInt (level, -99, 99) / 99.0f;
        return clampInt (level, 0, 99) / 99.0f;
    }

    void begin (float to, int time, int scaleIndex)
    {
        target = to;
        const double sec = envelopeSeconds (time) * scales[scaleIndex];
        left = std::max (1, (int) std::llround (sec * sampleRate));
        delta = (target - current) / (float) left;
    }

    void advance()
    {
        if (stage == 0)
        {
            stage = 1;
            begin (valueOf (spec.bp), spec.dt, 1);
            return;
        }
        if (stage == 1)
        {
            stage = 2;
            begin (valueOf (spec.sl), spec.st, 2);
            return;
        }
        if (stage == 2)
        {
            stage = 3;
            current = valueOf (spec.sl);
            return;
        }
        if (stage == 4)
        {
            stage = 5;
            current = target;
        }
    }

    Eg4 spec {};
    double sampleRate = 48000.0;
    float scales[4] = { 1, 1, 1, 1 };
    bool bipolar = false;
    float current = 0.0f;
    float target = 0.0f;
    float delta = 0.0f;
    int left = 0;
    int stage = 5;
};

} // namespace m1
