#pragma once

#include <JuceHeader.h>
#include "JunoVoice.h"
#include "JunoChorus.h"
#include "Arpeggiator.h"

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
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    JunoShared shared;
    juce::Synthesiser synth;
    JunoChorus chorus;
    juno::HpfSwitch hpf;
    Arpeggiator arp;

    float lfoPhase = 0.0f;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Juno106AudioProcessor)
};
