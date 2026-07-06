#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace juno
{

//==============================================================================
// PolyBLEP band-limiting residual. t = phase [0,1), dt = phase increment.
inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }

    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }

    return 0.0f;
}

//==============================================================================
// The Juno DCO: saw + pulse (PWM) + square sub-oscillator one octave down,
// all derived from a single master phase, plus white noise.
struct Dco
{
    void reset() noexcept
    {
        phase = 0.0f;
        subPhase = 0.0f;
    }

    // pw in [0.5, 0.95]
    float process (float phaseInc, bool sawOn, bool pulseOn, float pw,
                   float subLevel, float noiseLevel) noexcept
    {
        float out = 0.0f;

        if (sawOn)
            out += (2.0f * phase - 1.0f) - polyBlep (phase, phaseInc);

        if (pulseOn)
        {
            float p = (phase < pw ? 1.0f : -1.0f);
            p += polyBlep (phase, phaseInc);
            float t2 = phase - pw;
            if (t2 < 0.0f) t2 += 1.0f;
            p -= polyBlep (t2, phaseInc);
            out += p;
        }

        if (subLevel > 0.0001f)
        {
            const float subInc = phaseInc * 0.5f;
            float s = (subPhase < 0.5f ? 1.0f : -1.0f);
            s += polyBlep (subPhase, subInc);
            float t2 = subPhase - 0.5f;
            if (t2 < 0.0f) t2 += 1.0f;
            s -= polyBlep (t2, subInc);
            out += s * subLevel;

            subPhase += subInc;
            if (subPhase >= 1.0f) subPhase -= 1.0f;
        }
        else
        {
            subPhase += phaseInc * 0.5f;
            if (subPhase >= 1.0f) subPhase -= 1.0f;
        }

        if (noiseLevel > 0.0001f)
            out += (rng.nextFloat() * 2.0f - 1.0f) * noiseLevel;

        phase += phaseInc;
        if (phase >= 1.0f) phase -= 1.0f;

        return out;
    }

    float phase = 0.0f, subPhase = 0.0f;
    juce::Random rng;
};

//==============================================================================
// Exponential one-shot ADSR in the spirit of the Juno's ENV chip.
struct ExpAdsr
{
    void prepare (double sampleRate) noexcept { fs = (float) sampleRate; }

    void setTimes (float attackS, float decayS, float sustainLvl, float releaseS) noexcept
    {
        aCoef = coefFor (attackS);
        dCoef = coefFor (decayS);
        rCoef = coefFor (releaseS);
        sustain = sustainLvl;
    }

    void noteOn() noexcept  { stage = Stage::attack; }
    void noteOff() noexcept { if (stage != Stage::idle) stage = Stage::release; }

    void reset() noexcept
    {
        stage = Stage::idle;
        value = 0.0f;
    }

    bool isActive() const noexcept { return stage != Stage::idle; }

    float next() noexcept
    {
        switch (stage)
        {
            case Stage::attack:
                // Aim past 1.0 so the attack has an analogue-style punchy curve.
                value += (1.35f - value) * aCoef;
                if (value >= 1.0f) { value = 1.0f; stage = Stage::decay; }
                break;

            case Stage::decay:
                value += (sustain - value) * dCoef;
                break;

            case Stage::release:
                value += (0.0f - value) * rCoef;
                if (value < 1.0e-4f) { value = 0.0f; stage = Stage::idle; }
                break;

            case Stage::idle:
            default:
                break;
        }

        return value;
    }

private:
    enum class Stage { idle, attack, decay, release };

    float coefFor (float seconds) const noexcept
    {
        const float t = juce::jmax (0.0005f, seconds);
        return 1.0f - std::exp (-1.0f / (t * fs));
    }

    float fs = 44100.0f;
    float aCoef = 0.01f, dCoef = 0.001f, rCoef = 0.001f;
    float sustain = 1.0f;
    float value = 0.0f;
    Stage stage = Stage::idle;
};

//==============================================================================
// Zero-delay-feedback 4-pole lowpass (TPT), standing in for the IR3109
// OTA cascade in the Juno's 80017a voice chip.
struct LadderZdf
{
    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        reset();
    }

    void reset() noexcept { z[0] = z[1] = z[2] = z[3] = 0.0f; }

    // cutoff in Hz, res in [0, 1], drive in [0, 1].
    // Near res == 1 the feedback exceeds the k == 4 stability point so the
    // filter self-oscillates, like the 80017a chip driven hard.
    float process (float x, float cutoff, float res, float drive) noexcept
    {
        cutoff = juce::jlimit (20.0f, fs * 0.45f, cutoff);

        const float g  = std::tan (juce::MathConstants<float>::pi * cutoff / fs);
        const float G  = g / (1.0f + g);
        const float d  = 1.0f / (1.0f + g);
        const float k  = 4.6f * res;              // >4 at max => self-oscillation
        const float G2 = G * G;
        const float G4 = G2 * G2;

        // Passband gain compensation, then input drive into the saturator.
        x *= 1.0f + 0.5f * k;
        const float pre = 1.0f + 5.0f * drive;    // drive raises the level hitting tanh
        const float post = 1.0f / (1.0f + 2.0f * drive);

        const float S  = d * (G2 * G * z[0] + G2 * z[1] + G * z[2] + z[3]);
        float y4 = (G4 * x + S) / (1.0f + k * G4);
        float u  = x - k * y4;

        // Soft saturation in the feedback path. Scaling the tanh argument down
        // reduces damping so resonance can ring and self-oscillate; drive pushes
        // it into audible dirt.
        u = std::tanh (0.8f * pre * u) / (0.8f * pre);

        for (int i = 0; i < 4; ++i)
        {
            const float v = (u - z[i]) * G;
            const float y = v + z[i];
            z[i] = y + v;
            u = y;
        }

        return u * post;
    }

    float fs = 44100.0f;
    float z[4] = {};
};

//==============================================================================
// The 4-position HPF switch that sits in front of the VCF on the 106.
// Position 0 gives a low-shelf bass boost, 1 is flat, 2 and 3 cut lows.
struct HpfSwitch
{
    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        lp = 0.0f;
        setPosition (1);
    }

    void setPosition (int newPos) noexcept
    {
        pos = juce::jlimit (0, 3, newPos);
        const float freq = (pos == 0 ? 120.0f : pos == 2 ? 160.0f : 720.0f);
        coef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * freq / fs);
    }

    float process (float x) noexcept
    {
        if (pos == 1)
            return x;

        lp += coef * (x - lp);

        if (pos == 0)
            return x + 0.5f * lp;   // gentle bass boost

        return x - lp;              // 6 dB/oct highpass
    }

    float fs = 44100.0f, coef = 0.1f, lp = 0.0f;
    int pos = 1;
};

} // namespace juno
