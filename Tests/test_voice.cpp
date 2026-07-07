// Isolates the JunoVoice DSP to catch NaN/instability on a plain note.
#include <JuceHeader.h>
#include "JunoVoice.h"
#include <iostream>
#include <cmath>

int main()
{
    const double sr = 48000.0;
    const int block = 512;

    JunoShared shared;
    shared.lfoBuffer.assign (block, 0.0f);

    // All parameters as owned atomics (defaults that should make sound).
    std::atomic<float> lfoRate {5}, lfoDelay {0}, dcoRange {1}, dcoLfo {0}, dcoPwm {0},
        pwmSource {0}, sawOn {1}, pulseOn {0}, subLevel {0}, noiseLevel {0},
        vcfFreq {0.7f}, vcfRes {0}, vcfEnvPol {0}, vcfEnv {0}, vcfLfo {0}, vcfKybd {0.5f},
        envAttack {0.005f}, envDecay {0.3f}, envSustain {0.8f}, envRelease {0.2f},
        vcaMode {0}, vcaLevel {0.8f}, portTime {0}, vcfDrive {0}, bendRange {2},
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
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);

    double acc = 0.0; int n = 0; bool anyNan = false;
    for (int b = 0; b < 40; ++b)
    {
        buf.clear();
        juce::MidiBuffer m = (b == 0) ? midi : juce::MidiBuffer();
        synth.renderNextBlock (buf, m, 0, block);
        for (int i = 0; i < block; ++i)
        {
            const float s = buf.getSample (0, i);
            if (std::isnan (s) || std::isinf (s)) anyNan = true;
            acc += (double) s * s; ++n;
        }
    }

    const double rms = std::sqrt (acc / n);
    std::cout << "voice RMS = " << rms << "  nan/inf=" << (anyNan ? 1 : 0) << "\n";
    std::cout << (anyNan ? "FAIL: voice produced NaN/Inf\n"
                         : (rms > 1.0e-3 ? "PASS: voice produces sound\n" : "FAIL: silent\n"));
    return anyNan || rms < 1.0e-3 ? 1 : 0;
}
