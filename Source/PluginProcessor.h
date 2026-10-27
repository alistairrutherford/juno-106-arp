#pragma once

#include <JuceHeader.h>
#include "JunoVoice.h"
#include "JunoChorus.h"
#include "Arpeggiator.h"
#include "JunoSysEx.h"
#include "JunoFactoryBank.h"

//==============================================================================
class Juno106AudioProcessor : public juce::AudioProcessor
{
public:
    Juno106AudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    const juce::String getName() const override            { return JucePlugin_Name; }
    bool acceptsMidi() const override                      { return true; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 0.1; }

    int getNumPrograms() override;
    int getCurrentProgram() override                       { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    juce::String getProgramCategory (int index);   // section heading for the preset list
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // On-screen keyboard, split to avoid a feedback loop:
    // - keyboardState is display-only and fed from the post-arp MIDI stream,
    //   so it shows the notes actually sounding (the arp pattern "dancing").
    // - clickState collects notes clicked on the GUI keyboard and injects
    //   them ahead of the arp, like keys on a MIDI controller.
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardState clickState;

    // Ask the audio thread to clear the arpeggiator's held-note state on the
    // next block (e.g. after loading a preset). Thread-safe.
    void requestArpReset() noexcept { arpResetRequested.store (true); }

    // A patch as (paramID, plain value) pairs. Applying one resets every
    // parameter to its default first, so unlisted extras (arp, drive, ...)
    // come back clean.
    using ParamValues = std::vector<std::pair<juce::String, float>>;
    void applyParameterValues (const ParamValues&);
    ParamValues parameterValuesFor (const juno::Juno106Patch&);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    JunoShared shared;
    juce::Synthesiser synth;
    JunoChorus chorus;
    juno::HpfSwitch hpf;
    Arpeggiator arp;

    float lfoPhase = 0.0f;
    int currentProgram = 0;
    std::atomic<bool> arpResetRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Juno106AudioProcessor)
};
