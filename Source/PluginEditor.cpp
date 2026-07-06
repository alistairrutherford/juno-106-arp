#include "PluginEditor.h"

namespace
{
    const juce::Colour kPanel      (0xff1a1a1d);
    const juce::Colour kHeader     (0xff0f0f11);
    const juce::Colour kBlueCap    (0xff4a90d9);
    const juce::Colour kRedCap     (0xffe04b3a);
    const juce::Colour kGreenCap   (0xff58b368);
    const juce::Colour kWhiteCap   (0xffe8e8e8);
    const juce::Colour kOrangeLine (0xffe08030);

    constexpr int kHeaderH   = 46;
    constexpr int kSectionY  = 56;
    constexpr int kSectionH  = 244;
    constexpr int kFaderW    = 46;
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

    setSize (1336, 320);
}

Juno106AudioProcessorEditor::~Juno106AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
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

    const int btnW = 52;
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
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold | juce::Font::italic)));
    g.drawText ("JUNO-106", 16, 0, 220, kHeaderH - 4, juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("POLYPHONIC SYNTHESIZER", 240, 0, 300, kHeaderH - 4, juce::Justification::centredLeft);

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
