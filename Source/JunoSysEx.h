#pragma once

#include <JuceHeader.h>
#include <vector>

namespace juno
{

//==============================================================================
// One Roland Juno-106 patch, decoded from SysEx.
//
// The 106 sends a patch as
//     F0 41 30 0n pp  s0..s15  sw1 sw2  F7      (0x30 = patch, pp = 0..127)
//     F0 41 31 0n 00  s0..s15  sw1 sw2  F7      (0x31 = manual mode)
// where s0..s15 are the slider positions (0..127) in this order:
//     LFO rate, LFO delay, DCO LFO, DCO PWM, noise, VCF freq, VCF res,
//     VCF env, VCF LFO, VCF kybd, VCA level, A, D, S, R, sub
// sw1: bit0 16', bit1 8', bit2 4', bit3 pulse, bit4 saw,
//      bit5 chorus (0 = on), bit6 chorus level (0 = II, 1 = I)
// sw2: bit0 PWM (0 = LFO, 1 = manual), bit1 env polarity (1 = negative),
//      bit2 VCA (1 = gate), bits3-4 HPF (inverted: 0 = position 3)
struct Juno106Patch
{
    enum Slider { lfoRate, lfoDelay, dcoLfo, dcoPwm, noise, vcfFreq, vcfRes, vcfEnv,
                  vcfLfo, vcfKybd, vcaLevel, attack, decay, sustain, release, sub,
                  numSliders };

    int number = 0;                       // 0..127 (A11..B88)
    juce::uint8 slider[numSliders] {};    // raw 0..127

    int  range = 1;                       // 0 = 16', 1 = 8', 2 = 4'
    bool pulse = false, saw = false;
    int  chorus = 0;                      // 0 = off, 1 = I, 2 = II
    bool pwmManual = false, envNegative = false, vcaGate = false;
    int  hpf = 0;                         // panel position 0..3

    float sliderNorm (Slider s) const noexcept { return (float) slider[s] / 127.0f; }

    // Roland's bank/group/number naming: patch 0 = "A11", 63 = "A88", 127 = "B88".
    static juce::String nameFor (int n)
    {
        const char bank = n < 64 ? 'A' : 'B';
        return juce::String::charToString (bank)
             + juce::String (((n % 64) / 8) + 1)
             + juce::String ((n % 8) + 1);
    }

    juce::String name() const { return nameFor (number); }

    static Juno106Patch decode (int patchNumber, const juce::uint8* d18)
    {
        Juno106Patch p;
        p.number = patchNumber;
        for (int i = 0; i < numSliders; ++i)
            p.slider[i] = d18[i];

        const auto sw1 = d18[16];
        const auto sw2 = d18[17];

        p.range = (sw1 & 0x01) ? 0 : (sw1 & 0x04) ? 2 : 1;   // default 8'
        p.pulse = (sw1 & 0x08) != 0;
        p.saw   = (sw1 & 0x10) != 0;

        const bool chorusOn = (sw1 & 0x20) == 0;
        p.chorus = ! chorusOn ? 0 : ((sw1 & 0x40) ? 1 : 2);

        p.pwmManual   = (sw2 & 0x01) != 0;
        p.envNegative = (sw2 & 0x02) != 0;
        p.vcaGate     = (sw2 & 0x04) != 0;
        p.hpf         = 3 - ((sw2 >> 3) & 0x03);
        return p;
    }
};

//==============================================================================
// Pulls every Juno-106 patch message out of a .syx blob. Messages from other
// devices or with the wrong length are skipped. Patches from 0x31 (manual mode)
// messages, or repeated patch numbers, are numbered by their order in the file.
inline std::vector<Juno106Patch> parseJuno106SysEx (const void* data, size_t size)
{
    std::vector<Juno106Patch> patches;
    const auto* b = static_cast<const juce::uint8*> (data);
    constexpr size_t msgLen = 24;   // F0 41 cmd ch pp + 18 data + F7

    for (size_t i = 0; i + msgLen <= size; ++i)
    {
        if (b[i] != 0xF0 || b[i + 1] != 0x41)
            continue;

        const auto cmd = b[i + 2];
        if ((cmd != 0x30 && cmd != 0x31) || b[i + msgLen - 1] != 0xF7)
            continue;

        bool valid = true;
        for (size_t k = i + 3; k < i + msgLen - 1; ++k)
            if (b[k] & 0x80) { valid = false; break; }   // data bytes are 7-bit
        if (! valid)
            continue;

        const int number = cmd == 0x30 ? (int) b[i + 4] : (int) patches.size();
        patches.push_back (Juno106Patch::decode (number, b + i + 5));
        i += msgLen - 1;
    }

    // A bank with duplicate numbers (or manual-mode dumps) falls back to file order.
    for (size_t a = 0; a < patches.size(); ++a)
        for (size_t c = a + 1; c < patches.size(); ++c)
            if (patches[a].number == patches[c].number)
            {
                for (size_t k = 0; k < patches.size(); ++k)
                    patches[k].number = (int) k;
                return patches;
            }

    return patches;
}

} // namespace juno
