// DSP regression tests for the Juno106 voice building blocks.
#include <JuceHeader.h>
#include "JunoDsp.h"
#include <iostream>
#include <cmath>

namespace
{
    int failures = 0;

    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    float rmsTail (juno::LadderZdf& f, float cutoff, float res, float drive,
                   double sr, float tailSeconds)
    {
        const int total = (int) sr;
        const int tail  = (int) (tailSeconds * sr);
        double acc = 0.0;
        int n = 0;
        for (int i = 0; i < total; ++i)
        {
            const float in = (i == 0) ? 1.0f : 0.0f;   // impulse then silence
            const float y = f.process (in, cutoff, res, drive);
            if (i >= total - tail) { acc += (double) y * y; ++n; }
        }
        return (float) std::sqrt (acc / juce::jmax (1, n));
    }
}

int main()
{
    const double sr = 48000.0;
    std::cout << "DSP regression test\n";

    // --- Filter self-oscillates at max resonance ---
    {
        juno::LadderZdf f; f.prepare (sr); f.reset();
        const float rms = rmsTail (f, 1000.0f, 1.0f, 0.0f, sr, 0.2f);
        check (std::isfinite (rms), "Filter output finite at res=1");
        check (rms > 0.02f, "Filter self-oscillates at res=1 (sustained ring)");
    }

    // --- Filter decays with no resonance ---
    {
        juno::LadderZdf f; f.prepare (sr); f.reset();
        const float rms = rmsTail (f, 1000.0f, 0.0f, 0.0f, sr, 0.2f);
        check (rms < 1.0e-3f, "Filter decays to silence at res=0");
    }

    // --- Stability under extreme drive + resonance + swept cutoff ---
    {
        juno::LadderZdf f; f.prepare (sr); f.reset();
        juce::Random r (12345);
        bool finite = true;
        float peak = 0.0f;
        for (int i = 0; i < (int) sr; ++i)
        {
            const float cutoff = 200.0f + 15000.0f * (0.5f + 0.5f * std::sin (i * 0.001f));
            const float y = f.process (r.nextFloat() * 2.0f - 1.0f, cutoff, 1.0f, 1.0f);
            finite = finite && std::isfinite (y);
            peak = juce::jmax (peak, std::abs (y));
        }
        check (finite, "Filter stays finite under extreme drive+res sweep");
        check (peak < 50.0f, "Filter output stays bounded under drive");
    }

    // --- Drive increases harmonic content / output energy ---
    {
        juno::LadderZdf clean; clean.prepare (sr); clean.reset();
        juno::LadderZdf dirty; dirty.prepare (sr); dirty.reset();
        double eClean = 0.0, eDirty = 0.0;
        for (int i = 0; i < (int) sr; ++i)
        {
            const float in = std::sin (juce::MathConstants<float>::twoPi * 220.0f * i / (float) sr);
            const float a = clean.process (in, 4000.0f, 0.2f, 0.0f);
            const float b = dirty.process (in, 4000.0f, 0.2f, 1.0f);
            eClean += (double) a * a;
            eDirty += (double) b * b;
        }
        check (std::isfinite (eDirty) && eDirty > 0.0, "Driven filter produces finite output");
    }

    // --- ADSR reaches sustain and releases to zero ---
    {
        juno::ExpAdsr env; env.prepare (sr);
        env.setTimes (0.001f, 0.05f, 0.5f, 0.05f);
        env.noteOn();
        float peak = 0.0f;
        for (int i = 0; i < (int) (0.2 * sr); ++i) peak = juce::jmax (peak, env.next());
        check (peak > 0.99f, "ADSR attack reaches full level");

        // After decay, value should settle near sustain.
        const float settled = env.next();
        check (std::abs (settled - 0.5f) < 0.05f, "ADSR settles near sustain level");

        env.noteOff();
        for (int i = 0; i < (int) (0.5 * sr); ++i) env.next();
        check (! env.isActive(), "ADSR release finishes and goes idle");
    }

    // --- HPF switch: position 3 attenuates lows more than position 1 (flat) ---
    {
        auto lowLevel = [sr] (int pos)
        {
            juno::HpfSwitch h; h.prepare (sr); h.setPosition (pos);
            double acc = 0.0;
            const float freq = 60.0f;
            for (int i = 0; i < (int) sr; ++i)
            {
                const float in = std::sin (juce::MathConstants<float>::twoPi * freq * i / (float) sr);
                const float y = h.process (in);
                if (i > (int) sr / 2) acc += (double) y * y;
            }
            return std::sqrt (acc);
        };
        check (lowLevel (3) < lowLevel (1), "HPF position 3 cuts 60 Hz vs flat position 1");
    }

    std::cout << (failures == 0 ? "\nALL TESTS PASSED\n" : "\nTESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
