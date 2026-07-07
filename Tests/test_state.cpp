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

    file.deleteFile();
    std::cout << (failures == 0 ? "\nALL TESTS PASSED\n" : "\nTESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
