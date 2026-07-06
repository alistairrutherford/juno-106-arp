#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "JunoLookAndFeel.h"

//==============================================================================
class Juno106AudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit Juno106AudioProcessorEditor (Juno106AudioProcessor&);
    ~Juno106AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;   // keeps the preset box in sync with the host

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Fader
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Toggle
    {
        juce::TextButton button;
        std::unique_ptr<ButtonAttachment> attachment;
    };

    struct Section
    {
        juce::String name;
        juce::Rectangle<int> bounds;
    };

    Fader& addFader (const juce::String& paramID, const juce::String& text,
                     juce::Colour capColour, bool snapToInt = false);
    Toggle& addToggle (const juce::String& paramID, const juce::String& text);

    void loadPreset (int index);
    void savePreset();
    void loadPresetFile();

    Juno106AudioProcessor& processor;
    JunoLookAndFeel lookAndFeel;

    juce::ComboBox presetBox;
    juce::TextButton prevButton, nextButton, saveButton, loadButton;
    std::unique_ptr<juce::FileChooser> fileChooser;
    int lastSeenProgram = -1;

    std::vector<std::unique_ptr<Fader>> faders;
    std::vector<std::unique_ptr<Toggle>> toggles;
    std::vector<Section> sections;

    // Indices into faders / toggles, assigned in the constructor.
    Fader *portTime {}, *lfoRate {}, *lfoDelay {}, *dcoRange {}, *dcoLfo {}, *dcoPwm {}, *pwmSrc {},
          *subLevel {}, *noiseLevel {}, *hpfFreq {}, *vcfFreq {}, *vcfRes {}, *vcfPol {},
          *vcfEnv {}, *vcfLfo {}, *vcfKybd {}, *attack {}, *decay {}, *sustain {},
          *release {}, *vcaMode {}, *vcaLevel {},
          *arpMode {}, *arpRate {}, *arpOct {}, *arpGate {},
          *vcfDrive {}, *bendRange {}, *benderVcf {}, *velVcf {}, *velVca {}, *atVcf {};
    Toggle *sawBtn {}, *pulseBtn {}, *chorusIBtn {}, *chorusIIBtn {},
           *arpOnBtn {}, *arpHoldBtn {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Juno106AudioProcessorEditor)
};
