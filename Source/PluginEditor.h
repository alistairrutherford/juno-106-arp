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
    void applyPresetFile (const juce::File&);
    void refreshPresetList (int idToSelect = -1);
    juce::File defaultPresetDir() const;

    Juno106AudioProcessor& processor;
    JunoLookAndFeel lookAndFeel;

    // Keyboard whose display state (the arp output) is never written by the
    // mouse; clicks are forwarded to the processor's clickState instead, so
    // they enter the MIDI stream ahead of the arp like real keys.
    struct ArpDisplayKeyboard : public juce::MidiKeyboardComponent
    {
        ArpDisplayKeyboard (juce::MidiKeyboardState& displayState, juce::MidiKeyboardState& clicks)
            : MidiKeyboardComponent (displayState, juce::KeyboardComponentBase::horizontalKeyboard),
              clickState (clicks) {}

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int note = getNoteAndVelocityAtPosition (e.position).note;
            if (note >= 0)
            {
                clickState.noteOn (1, note, 0.8f);
                clickedNote = note;
            }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const int note = getNoteAndVelocityAtPosition (e.position).note;
            if (note != clickedNote)
            {
                if (clickedNote >= 0) clickState.noteOff (1, clickedNote, 0.0f);
                if (note >= 0)        clickState.noteOn  (1, note, 0.8f);
                clickedNote = note;
            }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (clickedNote >= 0)
            {
                clickState.noteOff (1, clickedNote, 0.0f);
                clickedNote = -1;
            }
        }

        juce::MidiKeyboardState& clickState;
        int clickedNote = -1;
    };

    juce::ComboBox presetBox;
    juce::TextButton prevButton, nextButton, saveButton, loadButton;
    ArpDisplayKeyboard keyboard;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Array<juce::File> userFiles;   // loaded this session; combo IDs 1001+
    bool userPresetActive = false;
    int lastSeenProgram = -1;

    // ComboBox item-ID scheme: factory presets use 1..N, session presets 1001+.
    static constexpr int kUserIdBase = 1001;

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
