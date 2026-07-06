// Functional test for the Juno106 arpeggiator logic.
#include <JuceHeader.h>
#include "Arpeggiator.h"
#include <iostream>
#include <vector>

namespace
{
    int failures = 0;

    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    // Runs the arp for `numBlocks` blocks of `blockSize`, feeding `input` on
    // block 0 only, and returns the ordered list of note-on numbers plus a
    // stuck-note check.
    struct Result
    {
        std::vector<int> noteOns;
        int netHeld = 0;         // should return to 0 (every on matched by off)
        bool passedThroughCC = false;
    };

    Result run (Arpeggiator& arp, const Arpeggiator::Params& p,
                const juce::MidiBuffer& input, int numBlocks, int blockSize,
                double bpm, double sr, bool playing)
    {
        Result r;
        const double ppqPerBlock = blockSize * (bpm / 60.0) / sr;

        for (int b = 0; b < numBlocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                midi.addEvents (input, 0, blockSize, 0);

            const double ppqStart = ppqPerBlock * b;
            arp.process (midi, blockSize, p, bpm, ppqStart, playing);

            for (const auto meta : midi)
            {
                const auto m = meta.getMessage();
                if (m.isNoteOn())  { r.noteOns.push_back (m.getNoteNumber()); ++r.netHeld; }
                if (m.isNoteOff()) { --r.netHeld; }
                if (m.isController() && m.getControllerNumber() == 1) r.passedThroughCC = true;
            }
        }
        return r;
    }

    juce::MidiBuffer chord (std::vector<int> notes)
    {
        juce::MidiBuffer b;
        for (int n : notes)
            b.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
        return b;
    }
}

int main()
{
    const double sr = 48000.0, bpm = 120.0;
    const int blockSize = 6000;   // at 120bpm/48k, 1/16 note = one step per block

    std::cout << "Arpeggiator functional test\n";

    // --- Up mode, 1 octave: C E G repeating ---
    {
        Arpeggiator arp; arp.prepare (sr);
        Arpeggiator::Params p; p.on = true; p.mode = Arpeggiator::Up; p.rate = 3; p.octaves = 1; p.gate = 0.5f;
        auto r = run (arp, p, chord ({ 60, 64, 67 }), 7, blockSize, bpm, sr, true);
        const std::vector<int> expected { 60, 64, 67, 60, 64, 67, 60 };
        check (r.noteOns == expected, "Up 1-oct order 60,64,67,60,64,67,60");
        check (r.netHeld == 0, "Up: no stuck notes (every note-on has a note-off)");
    }

    // --- Up mode, 2 octaves ---
    {
        Arpeggiator arp; arp.prepare (sr);
        Arpeggiator::Params p; p.on = true; p.mode = Arpeggiator::Up; p.rate = 3; p.octaves = 2; p.gate = 0.5f;
        auto r = run (arp, p, chord ({ 60, 64, 67 }), 6, blockSize, bpm, sr, true);
        const std::vector<int> expected { 60, 64, 67, 72, 76, 79 };
        check (r.noteOns == expected, "Up 2-oct order 60,64,67,72,76,79");
    }

    // --- Down mode ---
    {
        Arpeggiator arp; arp.prepare (sr);
        Arpeggiator::Params p; p.on = true; p.mode = Arpeggiator::Down; p.rate = 3; p.octaves = 1; p.gate = 0.5f;
        auto r = run (arp, p, chord ({ 60, 64, 67 }), 4, blockSize, bpm, sr, true);
        const std::vector<int> expected { 67, 64, 60, 67 };
        check (r.noteOns == expected, "Down order 67,64,60,67");
    }

    // --- Up/Down mode (no repeated endpoints): 60,64,67,64,...) ---
    {
        Arpeggiator arp; arp.prepare (sr);
        Arpeggiator::Params p; p.on = true; p.mode = Arpeggiator::UpDown; p.rate = 3; p.octaves = 1; p.gate = 0.5f;
        auto r = run (arp, p, chord ({ 60, 64, 67 }), 5, blockSize, bpm, sr, true);
        const std::vector<int> expected { 60, 64, 67, 64, 60 };
        check (r.noteOns == expected, "Up/Down order 60,64,67,64,60");
    }

    // --- Gate length: note-off ~50% into the step ---
    {
        Arpeggiator arp; arp.prepare (sr);
        Arpeggiator::Params p; p.on = true; p.mode = Arpeggiator::Up; p.rate = 3; p.octaves = 1; p.gate = 0.5f;
        juce::MidiBuffer midi = chord ({ 60 });
        arp.process (midi, blockSize, p, bpm, 0.0, true);
        int onPos = -1, offPos = -1;
        for (const auto meta : midi)
        {
            if (meta.getMessage().isNoteOn())  onPos  = meta.samplePosition;
            if (meta.getMessage().isNoteOff()) offPos = meta.samplePosition;
        }
        check (onPos == 0, "Gate: note-on at start of step");
        check (offPos > 2700 && offPos < 3300, "Gate: 50% note-off near sample 3000");
    }

    // --- CC pass-through ---
    {
        Arpeggiator arp; arp.prepare (sr);
        Arpeggiator::Params p; p.on = true; p.mode = Arpeggiator::Up; p.rate = 3; p.octaves = 1; p.gate = 0.5f;
        juce::MidiBuffer in;
        in.addEvent (juce::MidiMessage::controllerEvent (1, 1, 64), 0);   // CC1 mod wheel
        auto r = run (arp, p, in, 1, blockSize, bpm, sr, true);
        check (r.passedThroughCC, "CC1 passes through the arp");
    }

    // --- Hold/latch: notes keep playing after release ---
    {
        Arpeggiator arp; arp.prepare (sr);
        Arpeggiator::Params p; p.on = true; p.mode = Arpeggiator::Up; p.rate = 3; p.octaves = 1; p.gate = 0.5f; p.hold = true;
        std::vector<int> ons;
        int net = 0;
        const double ppqPerBlock = blockSize * (bpm / 60.0) / sr;
        for (int b = 0; b < 6; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0) midi = chord ({ 60, 64, 67 });
            if (b == 1)  // release all keys during block 1
            {
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 10);
                midi.addEvent (juce::MidiMessage::noteOff (1, 64), 10);
                midi.addEvent (juce::MidiMessage::noteOff (1, 67), 10);
            }
            arp.process (midi, blockSize, p, bpm, ppqPerBlock * b, true);
            for (const auto meta : midi)
            {
                if (meta.getMessage().isNoteOn())  { ons.push_back (meta.getMessage().getNoteNumber()); ++net; }
                if (meta.getMessage().isNoteOff() && ons.size() > 0) --net;
            }
        }
        // Should keep cycling all three notes even after release in block 1.
        check (ons.size() >= 6, "Hold: keeps arpeggiating after keys released");
        bool hasAllThree = false;
        for (size_t i = 3; i + 2 < ons.size(); ++i)
            if (ons[i] == 60 || ons[i] == 64 || ons[i] == 67) hasAllThree = true;
        check (hasAllThree, "Hold: still plays held notes post-release");
    }

    // --- HOLD release drops latched notes and stops the arp ---
    {
        Arpeggiator arp; arp.prepare (sr);
        Arpeggiator::Params p; p.on = true; p.mode = Arpeggiator::Up; p.rate = 3; p.octaves = 1; p.gate = 0.5f;
        const double ppqPerBlock = blockSize * (bpm / 60.0) / sr;
        std::vector<int> ons;
        int net = 0;

        for (int b = 0; b < 8; ++b)
        {
            juce::MidiBuffer midi;
            p.hold = (b < 4);   // hold ON for blocks 0-3, released from block 4

            if (b == 0) midi = chord ({ 60, 64, 67 });
            if (b == 1)   // release all keys while holding -> notes stay latched
            {
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 10);
                midi.addEvent (juce::MidiMessage::noteOff (1, 64), 10);
                midi.addEvent (juce::MidiMessage::noteOff (1, 67), 10);
            }

            arp.process (midi, blockSize, p, bpm, ppqPerBlock * b, true);
            for (const auto meta : midi)
            {
                if (meta.getMessage().isNoteOn())  { ons.push_back (meta.getMessage().getNoteNumber()); ++net; }
                if (meta.getMessage().isNoteOff()) --net;
            }
        }

        // Blocks 0-3 latched (4 note-ons), blocks 4-7 should be silent.
        check (ons.size() == 4, "Hold release: no new notes after HOLD off with keys up");
        check (net == 0, "Hold release: sounding note is stopped (no stuck note)");
    }

    // --- Bypass: arp off passes note-ons straight through ---
    {
        Arpeggiator arp; arp.prepare (sr);
        Arpeggiator::Params p; p.on = false;
        auto r = run (arp, p, chord ({ 60, 64, 67 }), 1, blockSize, bpm, sr, true);
        const std::vector<int> expected { 60, 64, 67 };
        check (r.noteOns == expected, "Bypass: notes pass straight through when arp off");
    }

    std::cout << (failures == 0 ? "\nALL TESTS PASSED\n" : "\nTESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
