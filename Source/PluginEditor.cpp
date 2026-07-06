#include "PluginEditor.h"

namespace
{
    const juce::Colour kPanel      (0xff1a1a1d);
    const juce::Colour kHeader     (0xff0f0f11);
    const juce::Colour kBlueCap    (0xff4a90d9);
    const juce::Colour kRedCap     (0xffe04b3a);
    const juce::Colour kGreenCap   (0xff58b368);
    const juce::Colour kWhiteCap   (0xffe8e8e8);
    const juce::Colour kOrangeCap  (0xffe08030);
    const juce::Colour kOrangeLine (0xffe08030);

    constexpr int kHeaderH   = 46;
    constexpr int kSectionY  = 56;
    constexpr int kSectionH  = 244;
    constexpr int kFaderW    = 40;
    constexpr int kLabelH    = 26;
}

//==============================================================================
Juno106AudioProcessorEditor::Juno106AudioProcessorEditor (Juno106AudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    // Portamento
    portTime = &addFader ("portamento", "TIME", kWhiteCap);

    // LFO
    lfoRate  = &addFader ("lfoRate",  "RATE",  kBlueCap);
    lfoDelay = &addFader ("lfoDelay", "DELAY", kBlueCap);

    // DCO
    dcoRange   = &addFader ("dcoRange",  "RANGE", kWhiteCap, true);
    dcoLfo     = &addFader ("dcoLfo",    "LFO",   kBlueCap);
    dcoPwm     = &addFader ("dcoPwm",    "PWM",   kBlueCap);
    pwmSrc     = &addFader ("pwmSource", "SRC",   kWhiteCap, true);
    sawBtn     = &addToggle ("sawOn",    "SAW");
    pulseBtn   = &addToggle ("pulseOn",  "PULSE");
    subLevel   = &addFader ("subLevel",   "SUB",   kBlueCap);
    noiseLevel = &addFader ("noiseLevel", "NOISE", kBlueCap);

    // HPF
    hpfFreq = &addFader ("hpf", "FREQ", kWhiteCap, true);

    // VCF
    vcfFreq = &addFader ("vcfFreq", "FREQ", kRedCap);
    vcfRes  = &addFader ("vcfRes",  "RES",  kRedCap);
    vcfPol  = &addFader ("vcfEnvPol", "POL",  kWhiteCap, true);
    vcfEnv  = &addFader ("vcfEnv",  "ENV",  kRedCap);
    vcfLfo  = &addFader ("vcfLfo",  "LFO",  kRedCap);
    vcfKybd = &addFader ("vcfKybd", "KYBD", kRedCap);

    // ENV
    attack  = &addFader ("attack",  "A", kGreenCap);
    decay   = &addFader ("decay",   "D", kGreenCap);
    sustain = &addFader ("sustain", "S", kGreenCap);
    release = &addFader ("release", "R", kGreenCap);

    // VCA
    vcaMode  = &addFader ("vcaMode",  "MODE",  kWhiteCap, true);
    vcaLevel = &addFader ("vcaLevel", "LEVEL", kWhiteCap);

    // Chorus
    chorusIBtn  = &addToggle ("chorusI",  "I");
    chorusIIBtn = &addToggle ("chorusII", "II");

    // Arpeggiator (not on the original 106)
    arpOnBtn   = &addToggle ("arpOn",   "ON");
    arpHoldBtn = &addToggle ("arpHold", "HOLD");
    arpMode = &addFader ("arpMode",    "MODE", kOrangeCap, true);
    arpRate = &addFader ("arpRate",    "RATE", kOrangeCap, true);
    arpOct  = &addFader ("arpOctaves", "OCT",  kOrangeCap, true);
    arpGate = &addFader ("arpGate",    "GATE", kOrangeCap);

    // Preset browser in the header.
    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);   // item IDs are 1-based

    presetBox.setJustificationType (juce::Justification::centredLeft);
    presetBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2a2e));
    presetBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha (0.25f));
    presetBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    presetBox.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xffe08030));
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0)
            loadPreset (index);
    };
    addAndMakeVisible (presetBox);

    lastSeenProgram = processor.getCurrentProgram();

    setSize (1430, 320);

    // Poll the processor so host-driven program changes are reflected here too.
    startTimerHz (10);
}

Juno106AudioProcessorEditor::~Juno106AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Juno106AudioProcessorEditor::loadPreset (int index)
{
    processor.setCurrentProgram (index);
    lastSeenProgram = index;
}

void Juno106AudioProcessorEditor::timerCallback()
{
    const int prog = processor.getCurrentProgram();
    if (prog != lastSeenProgram)
    {
        lastSeenProgram = prog;
        presetBox.setSelectedId (prog + 1, juce::dontSendNotification);
    }
}

//==============================================================================
Juno106AudioProcessorEditor::Fader& Juno106AudioProcessorEditor::addFader (
    const juce::String& paramID, const juce::String& text, juce::Colour capColour, bool snapToInt)
{
    auto fader = std::make_unique<Fader>();
    auto& f = *fader;

    f.slider.setSliderStyle (juce::Slider::LinearVertical);
    f.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    f.slider.setColour (juce::Slider::thumbColourId, capColour);
    f.slider.setPopupDisplayEnabled (true, true, this);

    if (snapToInt)
        f.slider.setSliderSnapsToMousePosition (false);

    addAndMakeVisible (f.slider);

    f.label.setText (text, juce::dontSendNotification);
    f.label.setJustificationType (juce::Justification::centred);
    f.label.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    addAndMakeVisible (f.label);

    f.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramID, f.slider);

    faders.push_back (std::move (fader));
    return *faders.back();
}

Juno106AudioProcessorEditor::Toggle& Juno106AudioProcessorEditor::addToggle (
    const juce::String& paramID, const juce::String& text)
{
    auto toggle = std::make_unique<Toggle>();
    auto& t = *toggle;

    t.button.setButtonText (text);
    t.button.setClickingTogglesState (true);
    addAndMakeVisible (t.button);

    t.attachment = std::make_unique<ButtonAttachment> (processor.apvts, paramID, t.button);

    toggles.push_back (std::move (toggle));
    return *toggles.back();
}

//==============================================================================
void Juno106AudioProcessorEditor::resized()
{
    // Preset browser sits at the right end of the header strip.
    const int boxW = 220, boxH = 26;
    presetBox.setBounds (getWidth() - boxW - 16, (kHeaderH - boxH) / 2, boxW, boxH);

    sections.clear();

    int x = 14;
    const int gap = 10;

    auto placeFader = [&] (Fader* f, int& fx)
    {
        f->slider.setBounds (fx, kSectionY + 18, kFaderW, kSectionH - 18 - kLabelH);
        f->label.setBounds (fx, kSectionY + kSectionH - kLabelH, kFaderW, kLabelH);
        fx += kFaderW;
    };

    auto beginSection = [&] (const juce::String& name) { return std::make_pair (name, x); };
    auto endSection = [&] (std::pair<juce::String, int> s)
    {
        sections.push_back ({ s.first, { s.second - 6, kSectionY, x - s.second + 12, kSectionH } });
        x += gap + 12;
    };

    // Portamento
    auto s = beginSection ("PORTA");
    placeFader (portTime, x);
    endSection (s);

    // LFO
    s = beginSection ("LFO");
    placeFader (lfoRate, x);
    placeFader (lfoDelay, x);
    endSection (s);

    // DCO
    s = beginSection ("DCO");
    placeFader (dcoRange, x);
    placeFader (dcoLfo, x);
    placeFader (dcoPwm, x);
    placeFader (pwmSrc, x);

    const int btnW = 46;
    sawBtn->button.setBounds   (x + 4, kSectionY + 40, btnW, 34);
    pulseBtn->button.setBounds (x + 4, kSectionY + 90, btnW, 34);
    x += btnW + 8;

    placeFader (subLevel, x);
    placeFader (noiseLevel, x);
    endSection (s);

    // HPF
    s = beginSection ("HPF");
    placeFader (hpfFreq, x);
    endSection (s);

    // VCF
    s = beginSection ("VCF");
    placeFader (vcfFreq, x);
    placeFader (vcfRes, x);
    placeFader (vcfPol, x);
    placeFader (vcfEnv, x);
    placeFader (vcfLfo, x);
    placeFader (vcfKybd, x);
    endSection (s);

    // ENV
    s = beginSection ("ENV");
    placeFader (attack, x);
    placeFader (decay, x);
    placeFader (sustain, x);
    placeFader (release, x);
    endSection (s);

    // VCA
    s = beginSection ("VCA");
    placeFader (vcaMode, x);
    placeFader (vcaLevel, x);
    endSection (s);

    // Chorus
    s = beginSection ("CHORUS");
    chorusIBtn->button.setBounds  (x + 4, kSectionY + 40, btnW, 34);
    chorusIIBtn->button.setBounds (x + 4, kSectionY + 90, btnW, 34);
    x += btnW + 12;
    endSection (s);

    // Arpeggiator
    s = beginSection ("ARP");
    arpOnBtn->button.setBounds   (x + 4, kSectionY + 40, btnW, 34);
    arpHoldBtn->button.setBounds (x + 4, kSectionY + 90, btnW, 34);
    x += btnW + 8;
    placeFader (arpMode, x);
    placeFader (arpRate, x);
    placeFader (arpOct, x);
    placeFader (arpGate, x);
    endSection (s);
}

void Juno106AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kPanel);

    // Header strip
    g.setColour (kHeader);
    g.fillRect (0, 0, getWidth(), kHeaderH);
    g.setColour (kOrangeLine);
    g.fillRect (0, kHeaderH - 3, getWidth(), 3);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (23.0f, juce::Font::bold | juce::Font::italic)));
    g.drawText ("Roland Juno 106-ARP", 16, 0, 340, kHeaderH - 4, juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("POLYPHONIC SYNTHESIZER", 366, 0, 300, kHeaderH - 4, juce::Justification::centredLeft);

    // Caption to the left of the preset browser.
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("PRESET", presetBox.getX() - 66, 0, 60, kHeaderH - 2,
                juce::Justification::centredRight);

    // Section boxes and titles
    for (const auto& section : sections)
    {
        auto r = section.bounds;

        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (r.toFloat(), 5.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (section.name, r.getX(), r.getY() + 2, r.getWidth(), 14,
                    juce::Justification::centred);
    }
}
