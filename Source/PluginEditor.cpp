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
    constexpr int kRow1H     = 210;
    constexpr int kRow2Y     = kSectionY + kRow1H + 12;   // 278
    constexpr int kRow2H     = 150;
    constexpr int kFaderW    = 40;
    constexpr int kLabelH    = 26;
}

//==============================================================================
Juno106AudioProcessorEditor::Juno106AudioProcessorEditor (Juno106AudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      keyboard (p.keyboardState, p.clickState)
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

    // Second-row additions (modern extensions, not on the original 106)
    vcfDrive  = &addFader ("vcfDrive",  "DRIVE", kRedCap);
    bendRange = &addFader ("bendRange", "RANGE", kWhiteCap, true);
    benderVcf = &addFader ("benderVcf", "VCF",   kBlueCap);
    velVcf    = &addFader ("velVcf",    "VCF",   kGreenCap);
    velVca    = &addFader ("velVca",    "VCA",   kGreenCap);
    atVcf     = &addFader ("atVcf",     "VCF",   kBlueCap);

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

    // On-screen keyboard: shows the notes actually sounding (post-arp),
    // clickable to play. Full 88-key piano range.
    keyboard.setAvailableRange (21, 108);           // A0..C8
    keyboard.setScrollButtonsVisible (false);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffd8d8d8));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff1a1a1d));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colours::black.withAlpha (0.6f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, kOrangeLine.withAlpha (0.35f));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, kOrangeLine.withAlpha (0.8f));
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colour (0xff333333));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (keyboard);

    // Preset browser in the header: factory presets, then user presets.
    presetBox.setJustificationType (juce::Justification::centredLeft);
    presetBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2a2e));
    presetBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha (0.25f));
    presetBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    presetBox.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xffe08030));
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0)
            return;

        if (id < kUserIdBase)                 // factory preset
        {
            userPresetActive = false;
            loadPreset (id - 1);
        }
        else                                  // preset loaded/saved this session
        {
            const int j = id - kUserIdBase;
            if (juce::isPositiveAndBelow (j, (int) sessionPresets.size()))
            {
                userPresetActive = true;
                processor.applyParameterValues (sessionPresets[(size_t) j].values);
            }
        }
    };
    addAndMakeVisible (presetBox);

    auto setupHeaderButton = [this] (juce::TextButton& b, const juce::String& text)
    {
        b.setButtonText (text);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a2e));
        addAndMakeVisible (b);
    };
    setupHeaderButton (prevButton, "<");
    setupHeaderButton (nextButton, ">");
    setupHeaderButton (saveButton, "SAVE");
    setupHeaderButton (loadButton, "LOAD");

    // Prev/next step through every item in the list (factory + loaded).
    prevButton.onClick = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx > 0) presetBox.setSelectedItemIndex (idx - 1);   // notifies -> onChange loads
    };
    nextButton.onClick = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx < presetBox.getNumItems() - 1) presetBox.setSelectedItemIndex (idx + 1);
    };
    saveButton.onClick = [this] { savePreset(); };
    loadButton.onClick = [this] { loadPresetFile(); };

    // On load the instrument shows only the factory presets.
    refreshPresetList (processor.getCurrentProgram() + 1);
    lastSeenProgram = processor.getCurrentProgram();

    setSize (1430, 452);

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
    presetBox.setSelectedId (index + 1, juce::dontSendNotification);
}

juce::File Juno106AudioProcessorEditor::defaultPresetDir() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Juno106-ARP Presets");
}

void Juno106AudioProcessorEditor::refreshPresetList (int idToSelect)
{
    presetBox.clear (juce::dontSendNotification);

    // Factory presets: IDs 1..N, under a heading per category. Fresh
    // instances start with only these.
    juce::String category;
    for (int i = 0; i < processor.getNumPrograms(); ++i)
    {
        if (processor.getProgramCategory (i) != category)
        {
            category = processor.getProgramCategory (i);
            presetBox.addSectionHeading (category);
        }
        presetBox.addItem (processor.getProgramName (i), i + 1);
    }

    // Presets loaded/saved this session (IDs kUserIdBase..), grouped under
    // a heading per group in the order the groups first appeared.
    juce::StringArray groups;
    for (const auto& p : sessionPresets)
        groups.addIfNotAlreadyThere (p.group);

    for (const auto& group : groups)
    {
        presetBox.addSectionHeading (group);
        for (size_t j = 0; j < sessionPresets.size(); ++j)
            if (sessionPresets[j].group == group)
                presetBox.addItem (sessionPresets[j].name, kUserIdBase + (int) j);
    }

    if (idToSelect > 0)
        presetBox.setSelectedId (idToSelect, juce::dontSendNotification);
}

std::vector<Juno106AudioProcessorEditor::SessionPreset>
Juno106AudioProcessorEditor::readPresetFile (const juce::File& file)
{
    std::vector<SessionPreset> entries;

    if (file.hasFileExtension ("syx"))
    {
        juce::MemoryBlock data;
        if (! file.loadFileAsData (data))
            return entries;

        for (const auto& patch : juno::parseJuno106SysEx (data.getData(), data.getSize()))
            entries.push_back ({ file.getFileNameWithoutExtension(), patch.name(), file,
                                 processor.parameterValuesFor (patch) });
        return entries;
    }

    if (auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName (processor.apvts.state.getType()))
        {
            Juno106AudioProcessor::migrateStateXml (*xml);   // saved before the filter recalibration
            Juno106AudioProcessor::ParamValues values;
            for (auto* child : xml->getChildIterator())
                if (child->hasAttribute ("id") && child->hasAttribute ("value"))
                    values.emplace_back (child->getStringAttribute ("id"),
                                         (float) child->getDoubleAttribute ("value"));

            entries.push_back ({ "User", file.getFileNameWithoutExtension(), file, std::move (values) });
        }

    return entries;
}

bool Juno106AudioProcessorEditor::addSessionPresets (const juce::File& source, std::vector<SessionPreset> entries)
{
    if (entries.empty())
        return false;

    sessionPresets.erase (std::remove_if (sessionPresets.begin(), sessionPresets.end(),
                                          [&] (const SessionPreset& p) { return p.source == source; }),
                          sessionPresets.end());

    const int first = (int) sessionPresets.size();
    for (auto& e : entries)
        sessionPresets.push_back (std::move (e));

    userPresetActive = true;
    processor.applyParameterValues (sessionPresets[(size_t) first].values);
    refreshPresetList (kUserIdBase + first);
    return true;
}

void Juno106AudioProcessorEditor::savePreset()
{
    auto dir = defaultPresetDir();
    dir.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> ("Save preset", dir, "*.juno");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto file = fc.getResult();
                                  if (file == juce::File())
                                      return;

                                  file = file.withFileExtension ("juno");
                                  if (auto xml = processor.createStateXml())
                                      xml->writeTo (file);

                                  // Add the saved preset to the list and select it.
                                  addSessionPresets (file, readPresetFile (file));
                              });
}

void Juno106AudioProcessorEditor::loadPresetFile()
{
    auto dir = defaultPresetDir();

    fileChooser = std::make_unique<juce::FileChooser> ("Load preset or Juno-106 SysEx bank", dir, "*.juno;*.syx");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto file = fc.getResult();
                                  if (! file.existsAsFile())
                                      return;

                                  if (! addSessionPresets (file, readPresetFile (file)))
                                      juce::AlertWindow::showMessageBoxAsync (
                                          juce::MessageBoxIconType::WarningIcon, "Load preset",
                                          "No Juno 106-ARP preset or Juno-106 SysEx patches were found in\n"
                                              + file.getFileName());
                              });
}

void Juno106AudioProcessorEditor::timerCallback()
{
    // Don't override a user preset the player has selected; only track
    // host-driven factory program changes.
    if (userPresetActive)
        return;

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

    // Popup shows the parameter's real text: choice labels ("1/16", "16'") and
    // formatted values with units, instead of the raw normalized number.
    if (auto* rp = processor.apvts.getParameter (paramID))
    {
        f.slider.textFromValueFunction = [rp] (double v)
        {
            auto t = rp->getText (rp->convertTo0to1 ((float) v), 0);
            auto unit = rp->getLabel();
            return unit.isNotEmpty() ? t + " " + unit : t;
        };
        f.slider.updateText();
    }

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
    // Header toolbar (right to left): LOAD, SAVE, >, preset box, <.
    const int hy = (kHeaderH - 26) / 2;
    int hx = getWidth() - 16;
    auto placeRight = [&] (juce::Component& c, int w) { hx -= w; c.setBounds (hx, hy, w, 26); hx -= 6; };
    placeRight (loadButton, 52);
    placeRight (saveButton, 52);
    placeRight (nextButton, 26);
    placeRight (presetBox, 240);
    placeRight (prevButton, 26);

    sections.clear();

    int x = 14;
    const int gap = 10;
    const int btnW = 46;
    int rowY = kSectionY;
    int rowH = kRow1H;

    auto placeFader = [&] (Fader* f, int& fx)
    {
        f->slider.setBounds (fx, rowY + 18, kFaderW, rowH - 18 - kLabelH);
        f->label.setBounds (fx, rowY + rowH - kLabelH, kFaderW, kLabelH);
        fx += kFaderW;
    };

    auto beginSection = [&] (const juce::String& name) { return std::make_pair (name, x); };
    auto endSection = [&] (std::pair<juce::String, int> sec)
    {
        sections.push_back ({ sec.first, { sec.second - 6, rowY, x - sec.second + 12, rowH } });
        x += gap + 12;
    };

    // ---- Row 1: the classic Juno panel ----
    auto s = beginSection ("PORTA");
    placeFader (portTime, x);
    endSection (s);

    s = beginSection ("LFO");
    placeFader (lfoRate, x);
    placeFader (lfoDelay, x);
    endSection (s);

    s = beginSection ("DCO");
    placeFader (dcoRange, x);
    placeFader (dcoLfo, x);
    placeFader (dcoPwm, x);
    placeFader (pwmSrc, x);
    sawBtn->button.setBounds   (x + 4, rowY + 34, btnW, 30);
    pulseBtn->button.setBounds (x + 4, rowY + 78, btnW, 30);
    x += btnW + 8;
    placeFader (subLevel, x);
    placeFader (noiseLevel, x);
    endSection (s);

    s = beginSection ("HPF");
    placeFader (hpfFreq, x);
    endSection (s);

    s = beginSection ("VCF");
    placeFader (vcfFreq, x);
    placeFader (vcfRes, x);
    placeFader (vcfPol, x);
    placeFader (vcfEnv, x);
    placeFader (vcfLfo, x);
    placeFader (vcfKybd, x);
    endSection (s);

    s = beginSection ("ENV");
    placeFader (attack, x);
    placeFader (decay, x);
    placeFader (sustain, x);
    placeFader (release, x);
    endSection (s);

    s = beginSection ("VCA");
    placeFader (vcaMode, x);
    placeFader (vcaLevel, x);
    endSection (s);

    s = beginSection ("CHORUS");
    chorusIBtn->button.setBounds  (x + 4, rowY + 34, btnW, 30);
    chorusIIBtn->button.setBounds (x + 4, rowY + 78, btnW, 30);
    x += btnW + 12;
    endSection (s);

    s = beginSection ("ARP");
    arpOnBtn->button.setBounds   (x + 4, rowY + 34, btnW, 30);
    arpHoldBtn->button.setBounds (x + 4, rowY + 78, btnW, 30);
    x += btnW + 8;
    placeFader (arpMode, x);
    placeFader (arpRate, x);
    placeFader (arpOct, x);
    placeFader (arpGate, x);
    endSection (s);

    // ---- Row 2: modern extensions ----
    x = 14;
    rowY = kRow2Y;
    rowH = kRow2H;

    s = beginSection ("DRIVE");
    placeFader (vcfDrive, x);
    endSection (s);

    s = beginSection ("BEND");
    placeFader (bendRange, x);
    placeFader (benderVcf, x);
    endSection (s);

    s = beginSection ("VELOCITY");
    placeFader (velVcf, x);
    placeFader (velVca, x);
    endSection (s);

    s = beginSection ("AFTER");
    placeFader (atVcf, x);
    endSection (s);

    // On-screen keyboard fills the rest of row 2.
    const int kbX = x + 4;
    const int kbW = getWidth() - kbX - 20;
    keyboard.setBounds (kbX, rowY + 18, kbW, rowH - 18 - 10);
    keyboard.setKeyWidth ((float) kbW / 52.0f);     // 52 white keys in A0..C8
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
    g.drawText ("Juno 106-ARP", 16, 0, 240, kHeaderH - 4, juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("POLYPHONIC SYNTHESIZER", 266, 0, 300, kHeaderH - 4, juce::Justification::centredLeft);

    // Caption to the left of the preset toolbar.
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("PRESET", prevButton.getX() - 66, 0, 60, kHeaderH - 2,
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
