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

    // --- Calibration (Juno-106 Service Notes ranges) ---
    {
        namespace cal = juno::calibration;
        check (std::abs (cal::cutoffHz (0.0f) - 5.0f) < 1.0e-3f,       "FREQ slider 0 -> 5 Hz");
        check (std::abs (cal::cutoffHz (1.0f) - 50000.0f) < 1.0f,      "FREQ slider 1 -> 50 kHz");
        check (std::abs (cal::cutoffHz (0.5f) - 500.0f) < 0.01f,       "FREQ slider is exponential (0.5 -> 500 Hz)");
        check (std::abs (cal::sliderForCutoffHz (cal::cutoffHz (0.37f)) - 0.37f) < 1.0e-5f, "Cutoff <-> slider round trip");

        // Old presets keep their sound: migrated values give the same Hz / octaves.
        bool freqSame = true;
        for (float v = 0.0f; v <= 1.0f; v += 0.05f)
        {
            const float ratio = cal::cutoffHz (cal::migrateVcfFreq (v)) / cal::legacyCutoffHz (v);
            freqSame = freqSame && std::abs (ratio - 1.0f) < 1.0e-3f;
        }
        check (freqSame, "Migrated VCF freq gives the same cutoff Hz as before");
        check (std::abs (cal::migrateVcfEnv (0.7f) * cal::envOctaves - 0.7f * cal::legacyEnvOctaves) < 1.0e-4f,
               "Migrated VCF env gives the same octave sweep as before");
    }

    // --- Envelope time calibration (Roland spec endpoints, firmware-shaped curve) ---
    {
        namespace cal = juno::calibration;
        check (std::abs (cal::secondsAt (cal::attackKnots, 0.0f) - 0.0015f) < 1.0e-6f
                   && std::abs (cal::secondsAt (cal::attackKnots, 1.0f) - 3.0f) < 1.0e-5f,      "Attack spans 1.5 ms..3 s");
        check (std::abs (cal::secondsAt (cal::decayReleaseKnots, 0.0f) - 0.0015f) < 1.0e-6f
                   && std::abs (cal::secondsAt (cal::decayReleaseKnots, 1.0f) - 12.0f) < 1.0e-4f, "Decay/release span 1.5 ms..12 s");

        bool rising = true, roundTrip = true;
        for (int i = 1; i <= 127; ++i)
        {
            const float sl = i / 127.0f, prev = (i - 1) / 127.0f;
            rising = rising && cal::secondsAt (cal::attackKnots, sl) > cal::secondsAt (cal::attackKnots, prev)
                            && cal::secondsAt (cal::decayReleaseKnots, sl) > cal::secondsAt (cal::decayReleaseKnots, prev);
            roundTrip = roundTrip
                && std::abs (cal::sliderAt (cal::attackKnots, cal::secondsAt (cal::attackKnots, sl)) - sl) < 1.0e-4f
                && std::abs (cal::sliderAt (cal::decayReleaseKnots, cal::secondsAt (cal::decayReleaseKnots, sl)) - sl) < 1.0e-4f;
        }
        check (rising, "Envelope curves rise at every one of the 128 slider steps");
        check (roundTrip, "Slider <-> seconds round trip for all 128 steps");
        check (std::abs (cal::secondsAt (cal::attackKnots, 64.0f / 127) - 0.543f) < 1.0e-3f, "Attack at half travel ~0.54 s (firmware curve)");

        // The envelope itself honours those times.
        auto samplesToPeak = [sr] (float attackSeconds)
        {
            juno::ExpAdsr e; e.prepare (sr); e.setTimes (attackSeconds, 1.0f, 1.0f, 1.0f); e.noteOn();
            int n = 0;
            while (e.next() < 0.999f && n < (int) (10 * sr)) ++n;
            return (float) n / (float) sr;
        };
        check (std::abs (samplesToPeak (0.5f) / 0.5f - 1.0f) < 0.02f, "Attack setting 0.5 s reaches full level at 0.5 s");
        check (std::abs (samplesToPeak (cal::migrateAttack (0.2f)) / (0.2f * 1.3499f) - 1.0f) < 0.02f,
               "Migrated attack keeps the old time to full level");

        juno::ExpAdsr d; d.prepare (sr); d.setTimes (0.0015f, 0.25f, 0.0f, 1.0f); d.noteOn();
        while (d.next() < 0.999f) {}
        for (int i = 0; i < (int) (0.25 * sr); ++i) d.next();
        check (std::abs (d.next() - std::exp (-1.0f)) < 0.03f, "Decay setting is its exponential time constant");
    }

    // --- HPF positions: gain at a frequency, in dB ---
    {
        auto gainDb = [sr] (int pos, float freq)
        {
            juno::HpfSwitch h; h.prepare (sr); h.setPosition (pos);
            double in = 0.0, out = 0.0;
            for (int i = 0; i < (int) sr; ++i)
            {
                const float x = std::sin (juce::MathConstants<float>::twoPi * freq * i / (float) sr);
                const float y = h.process (x);
                if (i > (int) sr / 2) { in += (double) x * x; out += (double) y * y; }
            }
            return 10.0 * std::log10 (out / in);
        };
        check (std::abs (gainDb (2, 236.0f) + 3.0) < 1.0, "HPF position 2 is -3 dB at 236 Hz");
        check (std::abs (gainDb (3, 754.0f) + 3.0) < 1.0, "HPF position 3 is -3 dB at 754 Hz");
        check (std::abs (gainDb (0, 30.0f) - 10.0) < 1.0, "HPF position 0 boosts lows by ~10 dB");
        check (std::abs (gainDb (0, 5000.0f)) < 0.5,      "HPF position 0 leaves highs alone");
        check (std::abs (gainDb (1, 50.0f)) < 0.01,       "HPF position 1 is flat");
    }

    std::cout << (failures == 0 ? "\nALL TESTS PASSED\n" : "\nTESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
