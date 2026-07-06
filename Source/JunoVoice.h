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
    std::atomic<float>* vcfDrive     = nullptr;   // 0..1 filter drive
    std::atomic<float>* bendRange    = nullptr;   // pitch-bend range in semitones
    std::atomic<float>* benderVcf    = nullptr;   // bender -> VCF depth
    std::atomic<float>* velVcf       = nullptr;   // velocity -> VCF depth
    std::atomic<float>* velVca       = nullptr;   // velocity -> VCA depth
    std::atomic<float>* atVcf        = nullptr;   // aftertouch -> VCF depth

    // Global triangle LFO, filled once per block by the processor.
    std::vector<float> lfoBuffer;

    // Performance state, updated by the processor from incoming MIDI.
    std::atomic<float> modWheel   { 0.0f };       // CC1, adds LFO vibrato
    std::atomic<float> aftertouch { 0.0f };       // channel pressure, 0..1
    std::atomic<float> lastNote   { -1.0f };      // most recent note, for portamento
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

        // ~5 ms one-pole smoothing to de-zipper block-rate parameter changes.
        smoothCoef = 1.0f - std::exp (-1.0f / (0.005f * (float) sampleRate));

        // Fixed per-voice analog variance: a few cents of detune and a small
        // filter-cutoff trim, so stacked voices aren't digitally identical.
        juce::Random r ((juce::int64) (this) ^ 0x9e3779b9);
        voiceDetune     = std::pow (2.0f, (r.nextFloat() * 2.0f - 1.0f) * 6.0f / 1200.0f);   // +/-6 cents
        voiceCutoffTrim = std::pow (2.0f, (r.nextFloat() * 2.0f - 1.0f) * 0.04f);            // +/-4%
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheelPos) override
    {
        note = midiNote;
        noteVelocity = velocity;
        keyDown = true;
        primed = false;
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
        bendNorm = (float) (newValue - 8192) / 8192.0f;   // -1..+1
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
        const float levelTgt   = shared.vcaLevel->load();
        const float lfoDelayS  = shared.lfoDelay->load();
        const float modWheel   = shared.modWheel.load();
        const float portTime   = shared.portTime->load();
        const float driveTgt   = shared.vcfDrive->load();
        const float bendSemis  = shared.bendRange->load();
        const float benderVcf  = shared.benderVcf->load();
        const float velVcf     = shared.velVcf->load();
        const float velVca     = shared.velVca->load();
        const float atVcf      = shared.atVcf->load();
        const float aftertouch = shared.aftertouch.load();

        // Bend lever: pitch (scaled by range) and an optional VCF sweep.
        const float bendSemitones = bendNorm * bendSemis;
        const float benderOct     = bendNorm * benderVcf * 2.0f;

        // Velocity: unity when its depth is 0, so the 106's velocity-blind
        // behaviour is the default.
        const float velVcaGain = 1.0f - velVca * (1.0f - noteVelocity);
        const float velOct     = velVcf * (noteVelocity - 0.5f) * 4.0f;
        const float atOct      = atVcf * aftertouch * 4.0f;

        const float glideCoef  = portTime > 0.005f
                                     ? 1.0f - std::exp (-1.0f / (portTime * fs))
                                     : 1.0f;
        const float keyFollow  = std::pow (2.0f, ((float) note - 60.0f) / 12.0f * kybd);

        const int delaySamples = (int) (lfoDelayS * fs);
        const int riseSamples  = juce::jmax (1, (int) (juce::jmax (0.05f, lfoDelayS * 0.5f) * fs));

        // Prime the smoothers to the current targets on the first block of a note.
        if (! primed)
        {
            smVcfBase = cutoffBase;
            smRes     = res;
            smLevel   = levelTgt;
            smDrive   = driveTgt;
            primed    = true;
        }

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

            // Smooth block-rate parameters to avoid zipper noise on automation.
            smVcfBase += (cutoffBase - smVcfBase) * smoothCoef;
            smRes     += (res        - smRes)     * smoothCoef;
            smLevel   += (levelTgt   - smLevel)   * smoothCoef;
            smDrive   += (driveTgt   - smDrive)   * smoothCoef;

            // DCO pitch: slider vibrato (delayed LFO), mod-wheel vibrato
            // (immediate, up to +/- half a semitone), pitch bend and the
            // fixed per-voice detune.
            const float vibSemis = dcoLfoAmt * dcoLfoAmt * lfo * 2.0f
                                 + modWheel * lfoRaw * 0.5f
                                 + bendSemitones;
            const float freq = 440.0f * std::pow (2.0f, ((glideNote - 69.0f) + vibSemis) / 12.0f)
                               * rangeMul * voiceDetune;
            const float phaseInc = juce::jmin (0.45f, freq / fs);

            // PWM: manual sets width directly; LFO mode sweeps it.
            const float pw = pwmManual ? 0.5f + 0.45f * pwmAmt
                                       : 0.5f + 0.45f * pwmAmt * (lfo * 0.5f + 0.5f);

            float x = dco.process (phaseInc, sawOn, pulseOn, pw, subLvl, noiseLvl);

            // VCF with env / LFO / key-follow / bender / velocity / aftertouch
            // modulation (all in octaves).
            const float octMod = envPol * envAmt * envVal * 6.0f + lfoFiltAmt * lfo * 1.5f
                               + benderOct + velOct + atOct;
            const float cutoff = smVcfBase * keyFollow * voiceCutoffTrim * std::pow (2.0f, octMod);
            x = filter.process (x, cutoff, smRes, smDrive);

            // VCA: envelope or gate, scaled by velocity depth.
            gateValue += ((keyDown ? 1.0f : 0.0f) - gateValue) * gateCoef;
            const float amp = gateMode ? gateValue : envVal;
            x *= amp * smLevel * velVcaGain * 0.25f;

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
    float noteVelocity = 1.0f;
    bool keyDown = false;
    int samplesSinceNoteOn = 0;
    float bendNorm = 0.0f;
    float gateValue = 0.0f, gateCoef = 0.01f;

    // Per-voice analog variance.
    float voiceDetune = 1.0f, voiceCutoffTrim = 1.0f;

    // Smoothed parameter state.
    float smoothCoef = 1.0f;
    float smVcfBase = 0.0f, smRes = 0.0f, smLevel = 0.0f, smDrive = 0.0f;
    bool primed = false;
};
