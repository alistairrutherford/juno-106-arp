#pragma once

#include <JuceHeader.h>
#include "JunoDsp.h"

//==============================================================================
// Raw parameter pointers plus the per-block global LFO buffer, shared by
// the processor and all six voices.
struct JunoShared
{
    std::atomic<float>* lfoRate      = nullptr;
    std::atomic<float>* lfoDelay     = nullptr;
    std::atomic<float>* dcoRange     = nullptr;   // 0=16', 1=8', 2=4'
    std::atomic<float>* dcoLfo       = nullptr;
    std::atomic<float>* dcoPwm       = nullptr;
    std::atomic<float>* pwmSource    = nullptr;   // 0=LFO, 1=MAN
    std::atomic<float>* sawOn        = nullptr;
    std::atomic<float>* pulseOn      = nullptr;
    std::atomic<float>* subLevel     = nullptr;
    std::atomic<float>* noiseLevel   = nullptr;
    std::atomic<float>* vcfFreq      = nullptr;
    std::atomic<float>* vcfRes       = nullptr;
    std::atomic<float>* vcfEnvPol    = nullptr;   // 0=positive, 1=negative
    std::atomic<float>* vcfEnv       = nullptr;
    std::atomic<float>* vcfLfo       = nullptr;
    std::atomic<float>* vcfKybd      = nullptr;
    std::atomic<float>* envAttack    = nullptr;
    std::atomic<float>* envDecay     = nullptr;
    std::atomic<float>* envSustain   = nullptr;
    std::atomic<float>* envRelease   = nullptr;
    std::atomic<float>* vcaMode      = nullptr;   // 0=ENV, 1=GATE
    std::atomic<float>* vcaLevel     = nullptr;
    std::atomic<float>* portTime     = nullptr;   // seconds, ~0 = off

    // Global triangle LFO, filled once per block by the processor.
    std::vector<float> lfoBuffer;

    // Performance state, updated by the processor from incoming MIDI.
    std::atomic<float> modWheel { 0.0f };         // CC1, adds LFO vibrato
    std::atomic<float> lastNote { -1.0f };        // most recent note, for portamento
};

//==============================================================================
struct JunoSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

//==============================================================================
class JunoVoice : public juce::SynthesiserVoice
{
public:
    explicit JunoVoice (JunoShared& sharedState) : shared (sharedState) {}

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<JunoSound*> (s) != nullptr;
    }

    void prepare (double sampleRate)
    {
        env.prepare (sampleRate);
        filter.prepare (sampleRate);
        dco.reset();
        gateValue = 0.0f;
        gateCoef = 1.0f - std::exp (-1.0f / (0.003f * (float) sampleRate));
    }

    void startNote (int midiNote, float, juce::SynthesiserSound*, int pitchWheelPos) override
    {
        note = midiNote;
        keyDown = true;
        samplesSinceNoteOn = 0;
        pitchWheelMoved (pitchWheelPos);

        // Portamento: glide in from the previously played note.
        const float last = shared.lastNote.load();
        glideNote = (shared.portTime->load() > 0.005f && last >= 0.0f) ? last : (float) midiNote;
        shared.lastNote.store ((float) midiNote);

        dco.reset();
        filter.reset();
        env.reset();
        env.noteOn();
        gateValue = 0.0f;
    }

    void stopNote (float, bool allowTailOff) override
    {
        keyDown = false;
        env.noteOff();

        if (! allowTailOff)
        {
            clearCurrentNote();
            env.reset();
            gateValue = 0.0f;
        }
    }

    void pitchWheelMoved (int newValue) override
    {
        bendSemitones = 2.0f * (float) (newValue - 8192) / 8192.0f;
    }

    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        env.setTimes (shared.envAttack->load(), shared.envDecay->load(),
                      shared.envSustain->load(), shared.envRelease->load());

        const float fs         = (float) getSampleRate();
        const int   range      = (int) shared.dcoRange->load();
        const float rangeMul   = (range == 0 ? 0.5f : range == 2 ? 2.0f : 1.0f);
        const float dcoLfoAmt  = shared.dcoLfo->load();
        const float pwmAmt     = shared.dcoPwm->load();
        const bool  pwmManual  = shared.pwmSource->load() > 0.5f;
        const bool  sawOn      = shared.sawOn->load() > 0.5f;
        const bool  pulseOn    = shared.pulseOn->load() > 0.5f;
        const float subLvl     = shared.subLevel->load();
        const float noiseLvl   = shared.noiseLevel->load();
        const float cutoffBase = 30.0f * std::pow (2.0f, shared.vcfFreq->load() * 9.3f);
        const float res        = shared.vcfRes->load();
        const float envPol     = shared.vcfEnvPol->load() > 0.5f ? -1.0f : 1.0f;
        const float envAmt     = shared.vcfEnv->load();
        const float lfoFiltAmt = shared.vcfLfo->load();
        const float kybd       = shared.vcfKybd->load();
        const bool  gateMode   = shared.vcaMode->load() > 0.5f;
        const float level      = shared.vcaLevel->load();
        const float lfoDelayS  = shared.lfoDelay->load();
        const float modWheel   = shared.modWheel.load();
        const float portTime   = shared.portTime->load();

        const float glideCoef  = portTime > 0.005f
                                     ? 1.0f - std::exp (-1.0f / (portTime * fs))
                                     : 1.0f;
        const float keyFollow  = std::pow (2.0f, ((float) note - 60.0f) / 12.0f * kybd);

        const int delaySamples = (int) (lfoDelayS * fs);
        const int riseSamples  = juce::jmax (1, (int) (juce::jmax (0.05f, lfoDelayS * 0.5f) * fs));

        auto* left  = output.getWritePointer (0, startSample);
        auto* right = output.getNumChannels() > 1 ? output.getWritePointer (1, startSample) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            const float envVal = env.next();

            // Per-voice LFO delay fade applied to the global LFO.
            float delayScale = 1.0f;
            if (samplesSinceNoteOn < delaySamples)
                delayScale = 0.0f;
            else
                delayScale = juce::jmin (1.0f, (float) (samplesSinceNoteOn - delaySamples) / (float) riseSamples);
            ++samplesSinceNoteOn;

            const float lfoRaw = shared.lfoBuffer[(size_t) (startSample + i)];
            const float lfo = lfoRaw * delayScale;

            // Portamento glide toward the held note.
            glideNote += ((float) note - glideNote) * glideCoef;

            // DCO pitch: slider vibrato (delayed LFO), mod-wheel vibrato
            // (immediate, up to +/- half a semitone) and pitch bend.
            const float vibSemis = dcoLfoAmt * dcoLfoAmt * lfo * 2.0f
                                 + modWheel * lfoRaw * 0.5f
                                 + bendSemitones;
            const float freq = 440.0f * std::pow (2.0f, ((glideNote - 69.0f) + vibSemis) / 12.0f) * rangeMul;
            const float phaseInc = juce::jmin (0.45f, freq / fs);

            // PWM: manual sets width directly; LFO mode sweeps it.
            const float pw = pwmManual ? 0.5f + 0.45f * pwmAmt
                                       : 0.5f + 0.45f * pwmAmt * (lfo * 0.5f + 0.5f);

            float x = dco.process (phaseInc, sawOn, pulseOn, pw, subLvl, noiseLvl);

            // VCF with env / LFO / key-follow modulation (in octaves).
            const float octMod = envPol * envAmt * envVal * 6.0f + lfoFiltAmt * lfo * 1.5f;
            const float cutoff = cutoffBase * keyFollow * std::pow (2.0f, octMod);
            x = filter.process (x, cutoff, res);

            // VCA: envelope or gate.
            gateValue += ((keyDown ? 1.0f : 0.0f) - gateValue) * gateCoef;
            const float amp = gateMode ? gateValue : envVal;
            x *= amp * level * 0.25f;

            left[i] += x;
            if (right != nullptr)
                right[i] += x;
        }

        const bool finished = gateMode ? (! keyDown && gateValue < 1.0e-4f)
                                       : (! env.isActive());
        if (finished)
        {
            clearCurrentNote();
            env.reset();
        }
    }

private:
    JunoShared& shared;
    juno::Dco dco;
    juno::ExpAdsr env;
    juno::LadderZdf filter;

    int note = 60;
    float glideNote = 60.0f;
    bool keyDown = false;
    int samplesSinceNoteOn = 0;
    float bendSemitones = 0.0f;
    float gateValue = 0.0f, gateCoef = 0.01f;
};
