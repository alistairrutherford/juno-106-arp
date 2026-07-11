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
