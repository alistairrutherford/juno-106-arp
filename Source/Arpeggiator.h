#pragma once

#include <JuceHeader.h>
#include <vector>
#include <algorithm>
#include <limits>
#include <cmath>

//==============================================================================
// Tempo-synced arpeggiator that sits between MIDI input and the synth.
// Not a feature of the original Juno-106; modelled on common practice from
// later polysynths: host-synced note divisions, several play orders, an
// octave range, adjustable gate length, and hold/latch.
//
// It consumes incoming note on/off to maintain the held-note set, passes all
// other messages (CC, pitch bend, sustain) straight through, and emits its own
// timed note on/off for the synth to play.
class Arpeggiator
{
public:
    enum Mode { Up = 0, Down, UpDown, DownUp, Random, AsPlayed };

    struct Params
    {
        bool  on      = false;
        int   mode    = Up;
        int   rate    = 3;      // index into kRatePpq
        int   octaves = 1;      // 1..4
        float gate    = 0.5f;   // fraction of a step the note is held
        bool  hold    = false;  // latch: keep playing after keys released
    };

    // Note division of each step, in quarter notes.
    // 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32
    static constexpr int kNumRates = 6;
    static double ratePpq (int i)
    {
        static const double table[kNumRates] =
            { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        return table[juce::jlimit (0, kNumRates - 1, i)];
    }

    void prepare (double sr)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        held.clear();
        physicallyDown.clear();
        sequence.clear();
        stepCounter = 0;
        activeNote = -1;
        noteOffCountdown = -1;
        lastStepIndex = std::numeric_limits<long long>::min();
        freeQuarter = 0.0;
        latchArmed = false;
        rng.setSeedRandomly();
    }

    // Rewrites midi in place: incoming notes become the held set, generated
    // arp notes and passed-through controllers become the output.
    void process (juce::MidiBuffer& midi, int numSamples, const Params& p,
                  double bpm, double ppqStart, bool isPlaying)
    {
        if (bpm <= 0.0)
            bpm = 120.0;

        juce::MidiBuffer output;

        if (! p.on)
        {
            // Bypass: flush any sounding arp note, then pass everything through
            // and forget held state so re-enabling starts clean.
            if (activeNote >= 0)
            {
                output.addEvent (juce::MidiMessage::noteOff (1, activeNote), 0);
                activeNote = -1;
                noteOffCountdown = -1;
            }
            output.addEvents (midi, 0, numSamples, 0);
            midi.swapWith (output);

            held.clear();
            physicallyDown.clear();
            sequence.clear();
            latchArmed = false;
            return;
        }

        // 1. Absorb note on/off, pass other messages through.
        bool sequenceDirty = false;
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            const int  s = meta.samplePosition;

            if (m.isNoteOn())
            {
                noteOn (m.getNoteNumber(), m.getVelocity());
                sequenceDirty = true;
            }
            else if (m.isNoteOff())
            {
                if (noteOff (m.getNoteNumber(), p.hold))
                    sequenceDirty = true;
            }
            else
            {
                output.addEvent (m, s);   // CC / pitch bend / sustain pass through
            }
        }

        if (sequenceDirty)
            rebuildSequence (p);

        // 2. Advance the clock sample by sample and emit steps on the grid.
        const double inc          = bpm / 60.0 / sampleRate;      // quarter notes per sample
        const double ppqPerStep   = ratePpq (p.rate);
        const int    samplesPerStep = juce::jmax (1, (int) std::llround (ppqPerStep / inc));
        const int    gateSamples  = juce::jlimit (1, samplesPerStep,
                                                  (int) (p.gate * samplesPerStep));

        for (int i = 0; i < numSamples; ++i)
        {
            double qn;
            if (isPlaying)
            {
                qn = ppqStart + i * inc;
                freeQuarter = qn + inc;   // keep the free clock ready for when transport stops
            }
            else
            {
                qn = freeQuarter;
                freeQuarter += inc;
            }

            const long long stepIndex = (long long) std::floor (qn / ppqPerStep);
            if (stepIndex != lastStepIndex)
            {
                lastStepIndex = stepIndex;
                triggerStep (output, i, p, gateSamples);
            }

            if (noteOffCountdown > 0 && --noteOffCountdown == 0 && activeNote >= 0)
            {
                output.addEvent (juce::MidiMessage::noteOff (1, activeNote), i);
                activeNote = -1;
            }
        }

        midi.swapWith (output);
    }

private:
    void noteOn (int note, juce::uint8 vel)
    {
        // Starting a fresh chord after a full release clears the latched set.
        if (latchArmed)
        {
            held.clear();
            latchArmed = false;
        }

        physicallyDown.push_back (note);

        if (std::none_of (held.begin(), held.end(),
                          [note] (const Note& n) { return n.note == note; }))
            held.push_back ({ note, vel });
    }

    // Returns true if the held set changed.
    bool noteOff (int note, bool hold)
    {
        physicallyDown.erase (std::remove (physicallyDown.begin(), physicallyDown.end(), note),
                              physicallyDown.end());

        if (! hold)
        {
            const auto before = held.size();
            held.erase (std::remove_if (held.begin(), held.end(),
                                        [note] (const Note& n) { return n.note == note; }),
                        held.end());
            return held.size() != before;
        }

        // In hold mode, keep the note; when the last key is released, arm so the
        // next fresh press replaces the chord.
        if (physicallyDown.empty())
            latchArmed = true;

        return false;
    }

    void rebuildSequence (const Params& p)
    {
        sequence.clear();
        if (held.empty())
            return;

        std::vector<Note> base = held;   // played order (used as-is for AsPlayed)
        if (p.mode != AsPlayed)
            std::sort (base.begin(), base.end(),
                       [] (const Note& a, const Note& b) { return a.note < b.note; });

        // Expand across the octave range.
        std::vector<Note> up;
        for (int oct = 0; oct < juce::jlimit (1, 4, p.octaves); ++oct)
            for (const auto& n : base)
            {
                const int transposed = n.note + 12 * oct;
                if (transposed <= 127)
                    up.push_back ({ transposed, n.velocity });
            }

        if (up.empty())
            return;

        switch (p.mode)
        {
            case Down:
                sequence.assign (up.rbegin(), up.rend());
                break;

            case UpDown:
                sequence = up;
                for (int i = (int) up.size() - 2; i >= 1; --i)
                    sequence.push_back (up[(size_t) i]);
                break;

            case DownUp:
                sequence.assign (up.rbegin(), up.rend());
                for (int i = 1; i <= (int) up.size() - 2; ++i)
                    sequence.push_back (up[(size_t) i]);
                break;

            case Up:
            case Random:
            case AsPlayed:
            default:
                sequence = up;
                break;
        }
    }

    void triggerStep (juce::MidiBuffer& output, int samplePos, const Params& p, int gateSamples)
    {
        // Release the previous step's note before starting the next.
        if (activeNote >= 0)
        {
            output.addEvent (juce::MidiMessage::noteOff (1, activeNote), samplePos);
            activeNote = -1;
        }

        if (sequence.empty())
            return;

        int idx;
        if (p.mode == Random)
            idx = rng.nextInt ((int) sequence.size());
        else
            idx = (int) (stepCounter % (long long) sequence.size());

        ++stepCounter;

        const auto& n = sequence[(size_t) idx];
        output.addEvent (juce::MidiMessage::noteOn (1, n.note, n.velocity), samplePos);
        activeNote = n.note;
        noteOffCountdown = gateSamples;
    }

    struct Note { int note; juce::uint8 velocity; };

    double sampleRate = 44100.0;
    std::vector<Note> held;            // notes currently feeding the arp
    std::vector<int>  physicallyDown;  // keys physically held (for latch logic)
    std::vector<Note> sequence;        // expanded play order

    long long stepCounter = 0;
    long long lastStepIndex = std::numeric_limits<long long>::min();
    int  activeNote = -1;
    int  noteOffCountdown = -1;
    double freeQuarter = 0.0;
    bool latchArmed = false;
    juce::Random rng;
};
