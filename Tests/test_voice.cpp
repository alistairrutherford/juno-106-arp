// Isolates the JunoVoice DSP: plain notes must sound without NaN/Inf, and
// filter-only patches (no oscillators, full resonance) must ring like the 106.
#include <JuceHeader.h>
#include "JunoVoice.h"
#include <iostream>
#include <cmath>

namespace
{
    int failures = 0;

    void check (bool cond, const char* what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    struct Patch
    {
        float sawOn = 1, vcfFreq = 0.7f, vcfRes = 0, vcfKybd = 0.5f;
        float decay = 0.3f, sustain = 0.8f;
    };

    struct Result { double rms = 0; double earlyRms = 0; bool bad = false; int crossings = 0; float peak = 0; };

    // Plays middle C for ~0.43 s. earlyRms covers 20-60 ms after note-on.
    Result render (const Patch& patch, int note = 60)
    {
        const double sr = 48000.0;
        const int block = 512;

        JunoShared shared;
        shared.lfoBuffer.assign (block, 0.0f);

        std::atomic<float> lfoRate {5}, lfoDelay {0}, dcoRange {1}, dcoLfo {0}, dcoPwm {0},
            pwmSource {0}, sawOn {patch.sawOn}, pulseOn {0}, subLevel {0}, noiseLevel {0},
            vcfFreq {patch.vcfFreq}, vcfRes {patch.vcfRes}, vcfEnvPol {0}, vcfEnv {0}, vcfLfo {0},
            vcfKybd {patch.vcfKybd}, envAttack {0.005f}, envDecay {patch.decay}, envSustain {patch.sustain},
            envRelease {0.2f}, vcaMode {0}, vcaLevel {0.8f}, portTime {0}, vcfDrive {0}, bendRange {2},
            benderVcf {0}, velVcf {0}, velVca {0}, atVcf {0};

        shared.lfoRate=&lfoRate; shared.lfoDelay=&lfoDelay; shared.dcoRange=&dcoRange;
        shared.dcoLfo=&dcoLfo; shared.dcoPwm=&dcoPwm; shared.pwmSource=&pwmSource;
        shared.sawOn=&sawOn; shared.pulseOn=&pulseOn; shared.subLevel=&subLevel;
        shared.noiseLevel=&noiseLevel; shared.vcfFreq=&vcfFreq; shared.vcfRes=&vcfRes;
        shared.vcfEnvPol=&vcfEnvPol; shared.vcfEnv=&vcfEnv; shared.vcfLfo=&vcfLfo;
        shared.vcfKybd=&vcfKybd; shared.envAttack=&envAttack; shared.envDecay=&envDecay;
        shared.envSustain=&envSustain; shared.envRelease=&envRelease; shared.vcaMode=&vcaMode;
        shared.vcaLevel=&vcaLevel; shared.portTime=&portTime; shared.vcfDrive=&vcfDrive;
        shared.bendRange=&bendRange; shared.benderVcf=&benderVcf; shared.velVcf=&velVcf;
        shared.velVca=&velVca; shared.atVcf=&atVcf;

        juce::Synthesiser synth;
        synth.setCurrentPlaybackSampleRate (sr);
        for (int i = 0; i < 6; ++i)
        {
            auto* v = new JunoVoice (shared);
            v->prepare (sr);
            synth.addVoice (v);
        }
        synth.addSound (new JunoSound());

        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer noteOn;
        noteOn.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);

        Result r;
        double acc = 0.0, early = 0.0; int n = 0, ne = 0;
        float prev = 0.0f;
        const int earlyStart = (int) (0.020 * sr), earlyEnd = (int) (0.060 * sr);
        for (int b = 0; b < 40; ++b)
        {
            buf.clear();
            juce::MidiBuffer m = (b == 0) ? noteOn : juce::MidiBuffer();
            synth.renderNextBlock (buf, m, 0, block);
            for (int i = 0; i < block; ++i)
            {
                const float s = buf.getSample (0, i);
                if (! std::isfinite (s)) r.bad = true;
                r.peak = juce::jmax (r.peak, std::abs (s));
                acc += (double) s * s; ++n;
                const int t = b * block + i;
                if (t >= earlyStart && t < earlyEnd) { early += (double) s * s; ++ne; }
                if (t >= (int) (0.2 * sr) && prev < 0.0f && s >= 0.0f) ++r.crossings;   // settled cycles
                prev = s;
            }
        }
        r.rms = std::sqrt (acc / n);
        r.earlyRms = std::sqrt (early / juce::jmax (1, ne));
        return r;
    }
}

int main()
{
    std::cout << "Voice test\n";

    const auto plain = render ({});
    check (! plain.bad, "Plain saw note: no NaN/Inf");
    check (plain.rms > 1.0e-3, "Plain saw note produces sound");

    // A 106 filter-only patch: no oscillator, resonance up, full key tracking.
    // Real hardware rings from circuit noise; the plugin must too.
    const auto ring = render ({ 0.0f, 0.5f, 1.0f, 1.0f });
    check (! ring.bad, "Self-oscillating filter patch: no NaN/Inf");
    check (ring.rms > 1.0e-2, "Self-oscillating filter patch produces sound");
    check (ring.earlyRms > 1.0e-2, "Self-oscillation starts within 20-60 ms of note-on");

    // Melodic filter patches (Toy Rhodes, Tomita) need the ring to track the
    // keyboard: an octave up should double the frequency.
    const auto ringUp = render ({ 0.0f, 0.5f, 1.0f, 1.0f }, 72);
    const double ratio = ring.crossings > 0 ? (double) ringUp.crossings / ring.crossings : 0.0;
    std::cout << "        (C4 " << ring.crossings << " cycles, C5 " << ringUp.crossings << ", ratio " << ratio << ")\n";
    check (ratio > 1.9 && ratio < 2.1, "Self-oscillation pitch tracks the keyboard (octave = 2x)");

    // Percussive filter-only patch (Resonance Funk: 20 ms decay, sustain 0):
    // the 106's filter is already ringing at note-on, so this still sounds.
    Patch funk; funk.sawOn = 0; funk.vcfFreq = 0.2f; funk.vcfRes = 1.0f; funk.vcfKybd = 0.7f;
    funk.decay = 0.02f; funk.sustain = 0.0f;
    const auto blip = render (funk);
    std::cout << "        (percussive patch peak " << blip.peak << ")\n";
    check (blip.peak > 2.0e-2f, "Percussive self-oscillating patch is audible (filter rings from note-on)");

    // A normal (non-oscillating) patch must get no note-on click from that.
    Patch noOsc; noOsc.sawOn = 0; noOsc.vcfRes = 0.5f;
    check (render (noOsc).peak < 1.0e-3f, "No note-on kick for patches that don't self-oscillate");

    // Without resonance the tiny excitation must stay inaudible.
    const auto quiet = render ({ 0.0f, 0.5f, 0.0f, 1.0f });
    check (quiet.rms < 1.0e-4, "No-oscillator patch without resonance stays silent (< -80 dBFS)");

    std::cout << (failures == 0 ? "\nALL TESTS PASSED\n" : "\nTESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
