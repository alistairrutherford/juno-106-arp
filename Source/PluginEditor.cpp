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
    constexpr int kInfoH     = 54;    // the Info View, with a 10 px gap under it

    struct Info { const char* key; const char* title; const char* text; };

    // One entry per parameter, plus the header, the sections and the keyboard. Keep each to a
    // line or two: what it does, and what its settings mean.
    const Info kInfo[] = {
        { "header.preset", "Preset", "The preset loaded: factory presets by category, then any .juno or Juno-106 .syx file loaded or saved this session." },
        { "header.prev", "Previous", "Loads the preset above in the list." },
        { "header.next", "Next", "Loads the preset below in the list." },
        { "header.save", "Save", "Saves these settings as a .juno file, which then appears in the list." },
        { "header.load", "Load", "Loads a .juno preset, or a Juno-106 .syx bank (one entry per patch)." },
        { "keyboard", "Keyboard", "Shows the notes sounding, after the arpeggiator. Click to play." },

        { "section.PORTA", "Portamento", "Glide from one note's pitch to the next (not on the original 106)." },
        { "portamento", "Glide time", "How long each note takes to glide from the last one's pitch, up to 2 seconds. 0: no glide." },
        { "section.LFO", "LFO", "The low-frequency oscillator: vibrato for the DCO, PWM and the filter." },
        { "lfoRate", "LFO rate", "The LFO's speed, 0.1 Hz to 30 Hz." },
        { "lfoDelay", "LFO delay", "How long after a note starts the LFO fades in, up to 3 seconds." },
        { "section.DCO", "DCO", "The oscillator: its range, modulation, waveforms, sub-oscillator and noise." },
        { "dcoRange", "Range", "The oscillator's octave: 16' (low), 8' (normal) or 4' (high)." },
        { "dcoLfo", "DCO LFO", "How much the LFO wobbles the pitch: vibrato." },
        { "dcoPwm", "PWM", "Pulse width modulation: how far the pulse wave's width moves, or with SRC Manual, its fixed width." },
        { "pwmSource", "PWM source", "LFO: the LFO moves the pulse width. Manual: the PWM fader sets it." },
        { "sawOn", "Saw", "Turns the sawtooth wave on or off." },
        { "pulseOn", "Pulse", "Turns the pulse (square) wave on or off. PWM changes its width." },
        { "subLevel", "Sub", "The sub-oscillator's level: a square wave an octave below." },
        { "noiseLevel", "Noise", "The level of white noise added to the oscillator." },
        { "section.HPF", "HPF", "The high-pass filter switch in front of the VCF." },
        { "hpf", "HPF", "0: a bass boost. 1: flat. 2 and 3: cut more and more of the lows." },
        { "section.VCF", "VCF", "The resonant low-pass filter and what moves its cutoff." },
        { "vcfFreq", "Cutoff", "The filter's cutoff: lower is darker." },
        { "vcfRes", "Resonance", "Emphasis at the cutoff. High settings make the filter ring." },
        { "vcfEnvPol", "Envelope polarity", "Positive: the envelope opens the filter. Negative: it closes it." },
        { "vcfEnv", "VCF env", "How far the envelope moves the cutoff." },
        { "vcfLfo", "VCF LFO", "How far the LFO moves the cutoff: a wah." },
        { "vcfKybd", "Keyboard follow", "How much the cutoff follows the key played: higher notes brighter." },
        { "section.ENV", "Envelope", "The ADSR envelope, shared by the VCF and (in Env mode) the VCA." },
        { "attack", "Attack", "How long the envelope takes to reach full level, up to 3 seconds." },
        { "decay", "Decay", "How long it takes to fall from full level to the sustain level, up to 12 seconds." },
        { "sustain", "Sustain", "The level held while the key is down." },
        { "release", "Release", "How long the sound takes to fade after the key is let go, up to 12 seconds." },
        { "section.VCA", "VCA", "The amplifier: its mode and level." },
        { "vcaMode", "VCA mode", "Env: the envelope shapes the volume. Gate: full volume while the key is down, off when it is let go." },
        { "vcaLevel", "VCA level", "The output level." },
        { "section.CHORUS", "Chorus", "The Juno's chorus: I, II, or both together." },
        { "chorusI", "Chorus I", "The gentler chorus. With II too, both run together." },
        { "chorusII", "Chorus II", "The deeper chorus. With I too, both run together." },
        { "section.ARP", "Arpeggiator", "Plays the held notes one at a time, locked to the host's tempo (not on the original 106)." },
        { "arpOn", "Arp on", "Turns the arpeggiator on or off." },
        { "arpHold", "Arp hold", "Latch: the arp keeps playing after you let go, until you play new notes." },
        { "arpMode", "Arp mode", "The note order: Up, Down, Up/Down, Down/Up, Random, or As Played." },
        { "arpRate", "Arp rate", "The step length: 1/4 to 1/32 (T: triplets)." },
        { "arpOctaves", "Arp octaves", "How many octaves the arp climbs (1 to 4)." },
        { "arpGate", "Arp gate", "How much of each step a note sounds, 5% to 100%." },
        { "section.DRIVE", "Drive", "Filter drive (not on the original 106)." },
        { "vcfDrive", "Drive", "Pushes the filter harder, for a warmer, grittier sound." },
        { "section.BEND", "Bend", "What the pitch bender does." },
        { "bendRange", "Bend range", "How far the bender moves the pitch, in semitones (0 to 12)." },
        { "benderVcf", "Bender VCF", "How far the bender moves the cutoff too, up to 2 octaves." },
        { "section.VELOCITY", "Velocity", "How playing harder changes the sound (not on the original 106)." },
        { "velVcf", "Velocity VCF", "How much velocity moves the cutoff: harder notes open it, softer ones close it (up to 2 octaves each way)." },
        { "velVca", "Velocity VCA", "How much harder notes play louder. 0: every note at the same level." },
        { "section.AFTER", "Aftertouch", "What pressing harder on a held key does (not on the original 106)." },
        { "atVcf", "Aftertouch VCF", "How far pressure opens the filter, up to 4 octaves." },
    };

    const char* kInfoHint = "Point at a control to see what it does.";

    const Info* findInfo (const juce::String& key)
    {
        if (key.isEmpty())
            return nullptr;
        for (const auto& i : kInfo)
            if (key == i.key)
                return &i;
        return nullptr;
    }
}

//==============================================================================
void Juno106AudioProcessorEditor::InfoView::show (const juce::String& newKey, const juce::String& newTitle, const juce::String& newText)
{
    key = newKey;
    if (newTitle == title && newText == text)
        return;
    title = newTitle;
    text = newText;
    repaint();
}

void Juno106AudioProcessorEditor::InfoView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (kHeader);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    r = r.reduced (14.0f, 8.0f);
    if (title.isNotEmpty())
    {
        g.setColour (kOrangeLine);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (title, r.removeFromTop (18.0f), juce::Justification::centredLeft);
    }
    g.setColour (juce::Colours::white.withAlpha (title.isEmpty() ? 0.55f : 0.85f));
    g.setFont (juce::Font (juce::FontOptions (12.5f)));
    g.drawFittedText (text, r.toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
}

juce::String Juno106AudioProcessorEditor::infoTitle (const juce::String& key)
{
    const auto* i = findInfo (key);
    return i != nullptr ? juce::String::fromUTF8 (i->title) : juce::String();
}

juce::String Juno106AudioProcessorEditor::infoText (const juce::String& key)
{
    const auto* i = findInfo (key);
    return i != nullptr ? juce::String::fromUTF8 (i->text) : juce::String();
}

juce::StringArray Juno106AudioProcessorEditor::infoKeys()
{
    juce::StringArray out;
    for (const auto& i : kInfo)
        out.add (i.key);
    return out;
}

juce::String Juno106AudioProcessorEditor::infoHint() { return kInfoHint; }

juce::String Juno106AudioProcessorEditor::infoKeyAt (juce::Point<int> p)
{
    // The innermost thing under the point with a key, else the section it is in. (From the
    // children down: the editor's own getComponentAt needs it to be on screen.)
    juce::Component* hit = nullptr;
    for (int i = getNumChildComponents(); --i >= 0 && hit == nullptr;)
    {
        auto* child = getChildComponent (i);
        if (child->isVisible() && child->getBounds().contains (p))
            hit = child->getComponentAt (child->getLocalPoint (this, p));
    }
    for (auto* c = hit; c != nullptr && c != this; c = c->getParentComponent())
        if (findInfo (c->getComponentID()) != nullptr)
            return c->getComponentID();
    if (hit == nullptr)
        for (const auto& s : sections)
            if (s.bounds.contains (p))
                return "section." + s.name;
    return {};
}

void Juno106AudioProcessorEditor::showInfoFor (const juce::String& key)
{
    if (const auto* i = findInfo (key))
        info.show (key, juce::String::fromUTF8 (i->title), juce::String::fromUTF8 (i->text));
    else
        info.show ({}, {}, kInfoHint);
}

void Juno106AudioProcessorEditor::updateInfo (const juce::MouseEvent& e)
{
    const auto key = infoKeyAt (e.getEventRelativeTo (this).getPosition());
    if (key != info.key || info.text.isEmpty())
        showInfoFor (key);
}

void Juno106AudioProcessorEditor::mouseEnter (const juce::MouseEvent& e) { updateInfo (e); }
void Juno106AudioProcessorEditor::mouseMove (const juce::MouseEvent& e)  { updateInfo (e); }
void Juno106AudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)  { updateInfo (e); }

void Juno106AudioProcessorEditor::mouseExit (const juce::MouseEvent& e)
{
    // Moving between children also sends exits: clear only on leaving the editor.
    if (! getLocalBounds().contains (e.getEventRelativeTo (this).getPosition()))
        showInfoFor ({});
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

    // The Info View's keys for what isn't a fader or a switch (those carry their parameter's ID).
    presetBox.setComponentID ("header.preset");
    prevButton.setComponentID ("header.prev");
    nextButton.setComponentID ("header.next");
    saveButton.setComponentID ("header.save");
    loadButton.setComponentID ("header.load");
    keyboard.setComponentID ("keyboard");
    addAndMakeVisible (info);
    showInfoFor ({});
    addMouseListener (this, true);   // hovering over any child updates the Info View

    setSize (1430, 452 + kInfoH + 10);   // 64 px taller than before the Info View

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
    f.slider.setComponentID (paramID);   // the Info View's key, for the fader and its caption
    f.label.setComponentID (paramID);

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
    t.button.setComponentID (paramID);   // the Info View's key
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

    // The Info View along the bottom, under both rows.
    info.setBounds (14, kRow2Y + kRow2H + 12, getWidth() - 28, kInfoH);
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
