// Integration test: the real processor's preset save/load round trip must
// restore parameters AND still produce audio. Run via run_integration.sh.
#include "PluginProcessor.h"
#include <iostream>
#include <cmath>

namespace
{
    int failures = 0;
    void check (bool cond, const char* what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    float rawVal (Juno106AudioProcessor& p, const char* id) { return *p.apvts.getRawParameterValue (id); }
    void  setNorm (Juno106AudioProcessor& p, const char* id, float n) { p.apvts.getParameter (id)->setValueNotifyingHost (n); }

    // Play a held middle-C for ~0.4s and return output RMS.
    double renderRms (Juno106AudioProcessor& p)
    {
        juce::MidiBuffer noteOn; noteOn.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer empty;
        double acc = 0.0; int n = 0;
        for (int b = 0; b < 40; ++b)
        {
            buf.clear();
            p.processBlock (buf, b == 0 ? noteOn : empty);
            for (int i = 0; i < 512; ++i) { const float s = buf.getSample (0, i); acc += (double) s * s; ++n; }
        }
        juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        buf.clear(); p.processBlock (buf, off);
        for (int b = 0; b < 120; ++b) { buf.clear(); p.processBlock (buf, empty); }
        return std::sqrt (acc / n);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::cout << "Processor state round-trip test\n";

    Juno106AudioProcessor proc;
    proc.setRateAndBufferSizeDetails (48000.0, 512);   // host does this before prepareToPlay
    proc.prepareToPlay (48000.0, 512);

    check (renderRms (proc) > 1.0e-3, "Default patch produces sound");

    // On-screen keyboard state mirrors incoming MIDI.
    {
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer on;  on.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
        proc.processBlock (buf, on);
        check (proc.keyboardState.isNoteOn (1, 64), "Keyboard state lights incoming note-on");

        juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
        buf.clear(); proc.processBlock (buf, off);
        check (! proc.keyboardState.isNoteOn (1, 64), "Keyboard state clears on note-off");

        // GUI clicks are injected ahead of the arp and end up displayed too.
        juce::MidiBuffer empty;
        proc.clickState.noteOn (1, 72, 0.8f);
        buf.clear(); proc.processBlock (buf, empty);
        check (proc.keyboardState.isNoteOn (1, 72), "Clicked key is injected and displayed");
        proc.clickState.noteOff (1, 72, 0.0f);
        buf.clear(); proc.processBlock (buf, empty);
        check (! proc.keyboardState.isNoteOn (1, 72), "Clicked key clears on release");

        for (int b = 0; b < 120; ++b) { buf.clear(); proc.processBlock (buf, empty); }
    }

    // Distinctive sounding patch.
    setNorm (proc, "sawOn", 1.0f);
    setNorm (proc, "vcaLevel", 1.0f);
    setNorm (proc, "vcfFreq", 0.85f);
    setNorm (proc, "sustain", 1.0f);
    const float wantLevel = rawVal (proc, "vcaLevel");
    const float wantVcf   = rawVal (proc, "vcfFreq");

    // Save to a file exactly as the plugin does.
    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("juno_state_test.juno");
    file.deleteFile();
    if (auto xml = proc.apvts.copyState().createXml())
        xml->writeTo (file);
    check (file.existsAsFile() && file.getSize() > 0, "Preset file is written");

    // Scramble to silence.
    setNorm (proc, "vcaLevel", 0.0f);
    setNorm (proc, "vcfFreq", 0.0f);
    check (renderRms (proc) < 1.0e-3, "Scrambled patch is silent (sanity)");

    // Load the file back, as the plugin does.
    if (auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName (proc.apvts.state.getType()))
            proc.apvts.replaceState (juce::ValueTree::fromXml (*xml));

    check (std::abs (rawVal (proc, "vcaLevel") - wantLevel) < 1.0e-4f, "Load restores VCA level");
    check (std::abs (rawVal (proc, "vcfFreq")   - wantVcf)   < 1.0e-4f, "Load restores VCF freq");
    check (renderRms (proc) > 1.0e-3, "Loaded preset produces sound");

    // Arp ON with the host transport STOPPED (no playhead => free-run) must
    // still turn held keys into sound.
    setNorm (proc, "arpOn", 1.0f);
    check (renderRms (proc) > 1.0e-3, "Arp on + transport stopped produces sound");

    // The GUI keyboard shows the ARP OUTPUT: with a held C and octaves=2, the
    // octave-up C (never physically held) must light up at some point.
    {
        setNorm (proc, "arpOctaves", 1.0f / 3.0f);   // choice index 1 -> 2 octaves
        proc.clickState.noteOn (1, 60, 0.8f);
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer empty;
        bool sawOctaveUp = false;
        for (int b = 0; b < 200 && ! sawOctaveUp; ++b)
        {
            buf.clear(); proc.processBlock (buf, empty);
            sawOctaveUp = proc.keyboardState.isNoteOn (1, 72);
        }
        check (sawOctaveUp, "Keyboard displays arp output (octave-up note lights)");
        proc.clickState.noteOff (1, 60, 0.0f);
        for (int b = 0; b < 120; ++b) { buf.clear(); proc.processBlock (buf, empty); }
        setNorm (proc, "arpOn", 0.0f);
        setNorm (proc, "arpOctaves", 0.0f);
    }

    // Filter recalibration keeps existing sounds the same.
    {
        namespace cal = juno::calibration;
        auto hzOf = [&] { return cal::cutoffHz (rawVal (proc, "vcfFreq")); };
        auto near = [] (float a, float b) { return std::abs (a / b - 1.0f) < 2.0e-3f; };

        const float peakFactor = cal::attackPeakFactor();

        proc.setCurrentProgram (0);
        check (near (hzOf(), cal::legacyCutoffHz (0.7f)), "Recalibration: Init cutoff unchanged");
        check (near (rawVal (proc, "attack"), 0.005f * peakFactor), "Recalibration: Init attack time unchanged");

        proc.setCurrentProgram (1);   // Lush Strings, voiced as vcfFreq 0.55 / vcfEnv 0.2
        check (near (hzOf(), cal::legacyCutoffHz (0.55f))
                   && std::abs (rawVal (proc, "vcfEnv") * cal::envOctaves - 0.2f * cal::legacyEnvOctaves) < 1.0e-3f,
               "Recalibration: plugin's own presets keep their cutoff and env sweep");
        check (near (rawVal (proc, "attack"), 0.35f * peakFactor) && near (rawVal (proc, "decay"), 1.2f),
               "Recalibration: plugin's own presets keep their attack and decay times");

        // A project saved before the recalibration (no calibration attribute).
        auto old = proc.createStateXml();
        old->removeAttribute ("calibration");
        for (auto* c : old->getChildIterator())
        {
            if (c->getStringAttribute ("id") == "vcfFreq") c->setAttribute ("value", 0.5);
            if (c->getStringAttribute ("id") == "attack")  c->setAttribute ("value", 0.1);
        }
        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (*old, block);
        proc.setStateInformation (block.getData(), (int) block.getSize());
        check (near (hzOf(), cal::legacyCutoffHz (0.5f)) && near (rawVal (proc, "attack"), 0.1f * peakFactor),
               "Recalibration: version-1 state converts filter and attack on load");

        // Saved between the two recalibrations: only attack needs converting.
        old->setAttribute ("calibration", 2);
        juce::MemoryBlock block2;
        juce::AudioProcessor::copyXmlToBinary (*old, block2);
        proc.setStateInformation (block2.getData(), (int) block2.getSize());
        check (std::abs (rawVal (proc, "vcfFreq") - 0.5f) < 1.0e-5f && near (rawVal (proc, "attack"), 0.1f * peakFactor),
               "Recalibration: version-2 state converts attack only");

        // Current state round-trips without being converted twice.
        const float before = rawVal (proc, "vcfFreq");
        juce::MemoryBlock now;
        proc.getStateInformation (now);
        proc.setStateInformation (now.getData(), (int) now.getSize());
        check (std::abs (rawVal (proc, "vcfFreq") - before) < 1.0e-5f, "Recalibration: current state is not converted again");
        proc.setCurrentProgram (0);
    }

    // Juno-106 SysEx patch -> processor parameters -> sound.
    {
        auto near = [] (float a, float b) { return std::abs (a / b - 1.0f) < 2.0e-3f; };
        const juce::uint8 msg[] = {
            0xF0, 0x41, 0x30, 0x00, 0x05,                       // patch 5 = "A16"
            40, 0, 0, 0, 0, 100, 30, 0, 0, 64, 127, 0, 0, 127, 20, 64,   // 16 sliders
            0x10 | 0x02,                                        // saw, 8', chorus on, level II
            0x08,                                               // PWM LFO, env +, VCA env, HPF bits 01 -> pos 2
            0xF7 };
        auto patches = juno::parseJuno106SysEx (msg, sizeof (msg));
        check (patches.size() == 1 && patches[0].name() == "A16", "SysEx patch parses as A16");

        setNorm (proc, "arpOn", 1.0f);   // must be reset to default by the import
        if (! patches.empty())
            proc.applyParameterValues (proc.parameterValuesFor (patches[0]));

        check (std::abs (rawVal (proc, "vcfFreq") - 100.0f / 127.0f) < 1.0e-3f, "SysEx VCF freq slider maps to same position");
        check (std::abs (rawVal (proc, "vcaLevel") - 1.0f) < 1.0e-3f,          "SysEx VCA level 127 -> 1.0");

        // VCA slider at 0 is a -14 dB trim, not silence.
        auto quiet = patches[0];
        quiet.slider[juno::Juno106Patch::vcaLevel] = 0;
        proc.applyParameterValues (proc.parameterValuesFor (quiet));
        check (std::abs (rawVal (proc, "vcaLevel") - std::pow (10.0f, -0.7f)) < 1.0e-3f, "SysEx VCA level 0 -> -14 dB trim, not mute");
        proc.applyParameterValues (proc.parameterValuesFor (patches[0]));
        check (std::abs (rawVal (proc, "subLevel") - 64.0f / 127.0f) < 1.0e-3f, "SysEx sub slider maps");
        check (rawVal (proc, "sawOn") > 0.5f && rawVal (proc, "pulseOn") < 0.5f, "SysEx saw on, pulse off");
        check (rawVal (proc, "chorusII") > 0.5f && rawVal (proc, "chorusI") < 0.5f, "SysEx chorus II");
        check ((int) rawVal (proc, "hpf") == 2 && (int) rawVal (proc, "dcoRange") == 1, "SysEx HPF pos 2, range 8'");
        check (rawVal (proc, "arpOn") < 0.5f, "SysEx import resets plugin extras (arp off)");
        check (std::abs (rawVal (proc, "attack") - 0.0015f) < 1.0e-5f && near (rawVal (proc, "release"),
                   juno::calibration::secondsAt (juno::calibration::decayReleaseKnots, 20.0f / 127)),
               "SysEx envelope sliders land on the 106 time curves");
        check (renderRms (proc) > 1.0e-3, "Imported Juno-106 patch produces sound");
    }

    // Juno-106 factory bank: 128 programs after the plugin's own presets, and
    // every one must make sound (fresh processor each, so long releases from
    // one patch can't mask a silent next one).
    {
        const int numOwn = proc.getNumPrograms() - juno::numFactoryPatches;
        check (juno::numFactoryPatches == 128, "Factory bank holds 128 patches");
        check (proc.getProgramName (0) == "Init", "Program 0 is still Init");
        check (proc.getProgramName (numOwn) == "Accordion"
                   && proc.getProgramName (numOwn + 127) == "Xylophone", "Factory patches follow the plugin's own presets");
        check (proc.getProgramCategory (numOwn) == "Juno-106 Factory"
                   && proc.getProgramCategory (0) == "Juno 106-ARP", "Program categories for the preset list headings");

        juce::StringArray silent, broken;
        std::vector<std::pair<float, juce::String>> peaks;
        for (int f = 0; f < juno::numFactoryPatches; ++f)
        {
            Juno106AudioProcessor p;
            p.setRateAndBufferSizeDetails (48000.0, 512);
            p.prepareToPlay (48000.0, 512);
            p.setCurrentProgram (numOwn + f);

            juce::MidiBuffer on; on.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer empty;
            float peak = 0.0f; bool bad = false;
            for (int b = 0; b < 188; ++b)   // ~2 s held
            {
                buf.clear();
                p.processBlock (buf, b == 0 ? on : empty);
                for (int i = 0; i < 512; ++i)
                {
                    const float s = buf.getSample (0, i);
                    if (! std::isfinite (s)) bad = true;
                    peak = juce::jmax (peak, std::abs (s));
                }
            }
            peaks.emplace_back (peak, p.getProgramName (numOwn + f));
            if (bad)             broken.add (p.getProgramName (numOwn + f));
            else if (peak < 1.0e-3f) silent.add (p.getProgramName (numOwn + f) + " (peak " + juce::String (peak, 5) + ")");
        }
        std::sort (peaks.begin(), peaks.end());
        std::cout << "        bank peak level: median " << juce::Decibels::gainToDecibels (peaks[peaks.size() / 2].first)
                  << " dB; quietest:";
        for (size_t q = 0; q < 5; ++q)
            std::cout << " " << peaks[q].second << " (" << juce::String (juce::Decibels::gainToDecibels (peaks[q].first), 1) << " dB)";
        std::cout << "\n        loudest:";
        for (size_t q = peaks.size() - 3; q < peaks.size(); ++q)
            std::cout << " " << peaks[q].second << " (" << juce::String (juce::Decibels::gainToDecibels (peaks[q].first), 1) << " dB)";
        std::cout << "\n";
        if (! silent.isEmpty()) std::cout << "        silent: " << silent.joinIntoString (", ") << "\n";
        if (! broken.isEmpty()) std::cout << "        NaN/Inf: " << broken.joinIntoString (", ") << "\n";
        check (broken.isEmpty(), "No factory patch produces NaN/Inf");
        check (silent.isEmpty(), "Every factory patch produces sound (held C4, peak > -60 dBFS)");
        check (peaks.back().first < 1.0f, "No factory patch clips a single held note (peak < 0 dBFS)");
    }

    // Fuzz: no random preset may ever make the plugin emit NaN/Inf (a host such
    // as Ableton disables a device that outputs NaN, which kills MIDI + sound).
    {
        juce::Random rng (0xAB1E);
        auto params = proc.getParameters();
        bool anyBad = false;
        for (int trial = 0; trial < 300 && ! anyBad; ++trial)
        {
            for (auto* p : params)
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                    rp->setValueNotifyingHost (rng.nextFloat());

            juce::MidiBuffer noteOn; noteOn.addEvent (juce::MidiMessage::noteOn (1, 48 + rng.nextInt (36), (juce::uint8) 100), 0);
            juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer empty;
            for (int b = 0; b < 12 && ! anyBad; ++b)
            {
                buf.clear();
                proc.processBlock (buf, b == 0 ? noteOn : empty);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i)
                        if (! std::isfinite (buf.getSample (ch, i))) anyBad = true;
            }
            juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 48, (juce::uint8) 0), 0);
            buf.clear(); proc.processBlock (buf, off);
        }
        check (! anyBad, "No random preset produces NaN/Inf output (300 trials)");
    }

    file.deleteFile();
    std::cout << (failures == 0 ? "\nALL TESTS PASSED\n" : "\nTESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
