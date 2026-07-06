#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>

//==============================================================================
// The Juno-106 BBD chorus: mono in, stereo out. The two wet taps are
// modulated in opposite directions, which is where the width comes from.
// Mode 1 = button I, 2 = button II, 3 = both (fast, vibrato-like).
class JunoChorus
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        delayLine.assign ((size_t) (fs * 0.06f) + 8, 0.0f);
        writePos = 0;
        phase = 0.0f;
        lpL = lpR = 0.0f;
        // BBD-ish darkness on the wet signal.
        wetLpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 8000.0f / fs);
    }

    void setMode (int newMode) noexcept { mode = juce::jlimit (0, 3, newMode); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (mode == 0 || buffer.getNumChannels() < 2)
            return;

        float rate, depth, centre;
        switch (mode)
        {
            case 1:  rate = 0.513f; depth = 0.00185f; centre = 0.00335f; break;
            case 2:  rate = 0.863f; depth = 0.00185f; centre = 0.00335f; break;
            default: rate = 9.75f;  depth = 0.00025f; centre = 0.00335f; break;
        }

        const float phaseInc = rate / fs;
        const int numSamples = buffer.getNumSamples();
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        const int size = (int) delayLine.size();

        for (int i = 0; i < numSamples; ++i)
        {
            const float dry = l[i];

            delayLine[(size_t) writePos] = dry;

            // Bipolar triangle.
            const float tri = phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase;
            phase += phaseInc;
            if (phase >= 1.0f) phase -= 1.0f;

            float wetL = read ((centre + depth * tri) * fs, size);
            float wetR = read ((centre - depth * tri) * fs, size);

            lpL += wetLpCoef * (wetL - lpL);
            lpR += wetLpCoef * (wetR - lpR);

            l[i] = 0.7f * (dry + lpL);
            r[i] = 0.7f * (dry + lpR);

            if (++writePos >= size) writePos = 0;
        }
    }

private:
    float read (float delaySamples, int size) const noexcept
    {
        float pos = (float) writePos - delaySamples;
        while (pos < 0.0f) pos += (float) size;

        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % size;
        const float frac = pos - (float) i0;
        return delayLine[(size_t) i0] + frac * (delayLine[(size_t) i1] - delayLine[(size_t) i0]);
    }

    std::vector<float> delayLine;
    int writePos = 0;
    float fs = 44100.0f, phase = 0.0f;
    float lpL = 0.0f, lpR = 0.0f, wetLpCoef = 0.5f;
    int mode = 0;
};
