// Unit tests for the Juno-106 SysEx patch decoder.
#include <JuceHeader.h>
#include "JunoSysEx.h"
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

    // Builds one Juno-106 patch message.
    std::vector<juce::uint8> patchMsg (juce::uint8 cmd, int number, const juce::uint8 sliders[16],
                                       juce::uint8 sw1, juce::uint8 sw2)
    {
        std::vector<juce::uint8> m { 0xF0, 0x41, cmd, 0x00, (juce::uint8) number };
        for (int i = 0; i < 16; ++i) m.push_back (sliders[i]);
        m.push_back (sw1);
        m.push_back (sw2);
        m.push_back (0xF7);
        return m;
    }
}

int main()
{
    std::cout << "Juno-106 SysEx decoder test\n";
    using juno::Juno106Patch;

    // --- One fully specified patch decodes every field ---
    {
        const juce::uint8 s[16] { 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 1, 2, 3, 4, 127 };
        // sw1: 4' + pulse + saw, chorus ON (bit5 clear), level I (bit6 set)
        // sw2: PWM manual, env negative, VCA gate, HPF bits = 01 -> position 2
        const juce::uint8 sw1 = 0x04 | 0x08 | 0x10 | 0x40;
        const juce::uint8 sw2 = 0x01 | 0x02 | 0x04 | (0x01 << 3);
        auto msg = patchMsg (0x30, 42, s, sw1, sw2);

        auto p = juno::parseJuno106SysEx (msg.data(), msg.size());
        check (p.size() == 1, "Single patch message parses to one patch");
        if (p.size() == 1)
        {
            const auto& q = p[0];
            bool slidersOk = true;
            for (int i = 0; i < 16; ++i) slidersOk = slidersOk && q.slider[i] == s[i];
            check (slidersOk, "All 16 slider bytes preserved in order");
            check (q.number == 42 && q.name() == "A63", "Patch number 42 -> name A63");
            check (q.range == 2, "Range bit2 -> 4'");
            check (q.pulse && q.saw, "Pulse + saw switches");
            check (q.chorus == 1, "Chorus on + level bit set -> chorus I");
            check (q.pwmManual && q.envNegative && q.vcaGate, "PWM manual, env negative, VCA gate");
            check (q.hpf == 2, "HPF bits 01 -> panel position 2 (inverted encoding)");
            check (std::abs (q.sliderNorm (Juno106Patch::sub) - 1.0f) < 1.0e-6f, "Slider 127 normalizes to 1.0");
        }
    }

    // --- Switch edge cases ---
    {
        const juce::uint8 s[16] {};
        auto a = patchMsg (0x30, 0, s, 0x01 | 0x20, 0x18);   // 16', chorus OFF, HPF bits 11 -> pos 0
        auto b = patchMsg (0x30, 1, s, 0x00,        0x00);   // no range bit -> 8', chorus II, HPF 00 -> pos 3
        std::vector<juce::uint8> blob (a); blob.insert (blob.end(), b.begin(), b.end());

        auto p = juno::parseJuno106SysEx (blob.data(), blob.size());
        check (p.size() == 2, "Two concatenated messages parse to two patches");
        if (p.size() == 2)
        {
            check (p[0].range == 0 && p[0].chorus == 0 && p[0].hpf == 0, "16', chorus off, HPF pos 0");
            check (p[1].range == 1 && p[1].chorus == 2 && p[1].hpf == 3, "Default 8', chorus II, HPF pos 3");
            check (! p[1].pulse && ! p[1].saw && ! p[1].pwmManual && ! p[1].vcaGate, "Cleared switches decode false");
        }
    }

    // --- A full 128-patch bank, mixed with junk, names A11..B88 ---
    {
        std::vector<juce::uint8> blob { 0xF0, 0x43, 0x10, 0x01, 0xF7 };   // a Yamaha message to skip
        const juce::uint8 s[16] {};
        for (int n = 0; n < 128; ++n)
        {
            auto m = patchMsg (0x30, n, s, 0x02, 0x00);
            blob.insert (blob.end(), m.begin(), m.end());
        }
        auto bad = patchMsg (0x30, 5, s, 0x02, 0x00);
        bad[10] = 0x90;                                                // 8-bit data byte -> invalid
        blob.insert (blob.end(), bad.begin(), bad.end());

        auto p = juno::parseJuno106SysEx (blob.data(), blob.size());
        check (p.size() == 128, "128-patch bank parses; foreign and corrupt messages skipped");
        if (p.size() == 128)
        {
            check (p[0].name() == "A11",   "Patch 0 is A11");
            check (p[7].name() == "A18",   "Patch 7 is A18");
            check (p[8].name() == "A21",   "Patch 8 is A21");
            check (p[63].name() == "A88",  "Patch 63 is A88");
            check (p[64].name() == "B11",  "Patch 64 is B11");
            check (p[127].name() == "B88", "Patch 127 is B88");
        }
    }

    // --- Manual-mode dumps are numbered by file order ---
    {
        const juce::uint8 s[16] {};
        auto a = patchMsg (0x31, 0, s, 0x02, 0x00);
        auto b = patchMsg (0x31, 0, s, 0x02, 0x00);
        std::vector<juce::uint8> blob (a); blob.insert (blob.end(), b.begin(), b.end());
        auto p = juno::parseJuno106SysEx (blob.data(), blob.size());
        check (p.size() == 2 && p[0].number == 0 && p[1].number == 1, "Manual-mode messages numbered in order");
    }

    // --- Garbage in, nothing out ---
    {
        const juce::uint8 junk[] { 0x00, 0x01, 0xF0, 0x41, 0x30, 0x00, 0xF7 };
        check (juno::parseJuno106SysEx (junk, sizeof (junk)).empty(), "Truncated/garbage data yields no patches");
    }

    std::cout << (failures == 0 ? "\nALL TESTS PASSED\n" : "\nTESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
