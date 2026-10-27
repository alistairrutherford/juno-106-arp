# Juno 106-ARP

A Roland Juno-106 style polyphonic synthesizer with an added arpeggiator, built
with JUCE. Builds as an **Audio Unit** and **VST3** (universal arm64 + x86_64),
plus a Standalone app for quick testing. Loads into Ableton Live and any other
AU/VST3 host on macOS.

![Alt text](/screenshot.png?raw=true "Instrument Interface")


## Architecture (faithful to the original signal path)

```
6 voices, each:  DCO (saw + PWM pulse + sub + noise) --> VCF (4-pole ZDF lowpass) --> VCA (env/gate)
then, global:    HPF (4-position switch) --> BBD chorus (I / II / I+II) --> stereo out
```

- **DCO** — PolyBLEP band-limited sawtooth and pulse with PWM (manual or LFO),
  square sub-oscillator one octave down, white noise. Range switch 16' / 8' / 4'.
- **VCF** — zero-delay-feedback 4-pole (24 dB/oct) lowpass in the spirit of the
  IR3109 OTA cascade, with resonance, envelope amount (normal/inverted polarity),
  LFO modulation and key follow. Self-oscillates at maximum resonance and has a
  **Drive** control for saturating filter dirt.
- **ENV** — single exponential ADSR shared by filter and amp, like the real unit.
- **VCA** — envelope or gate mode, level control.
- **LFO** — global free-running triangle with rate and per-note onset delay.
- **HPF** — 4-position switch: position 0 is a gentle bass boost, 1 flat, 2 and 3
  progressively cut lows.
- **Chorus** — BBD-style stereo chorus; buttons I, II, or both together for the
  fast vibrato-like third mode. Wet taps modulate in opposite directions per side.
- 6-voice polyphony with voice stealing. Velocity is off by default, as on the
  original, with optional velocity depth (see below).
- **Analog variance** — each voice has a fixed few-cents detune and small filter
  trim so stacked voices aren't digitally identical.
- **Parameter smoothing** — cutoff, resonance, level and drive are smoothed to
  avoid zipper noise on fast automation.

### Modern extensions (not on the original hardware)

- **Portamento** — glide time; 0 = off.
- **Mod wheel** (CC1) — adds immediate LFO vibrato, up to ± half a semitone.
- **Bend** — pitch-bend range (0–12 semitones) and an optional bender→VCF sweep.
- **Velocity** — optional velocity→VCF and velocity→VCA depth (both default 0).
- **Aftertouch** — channel pressure → VCF depth.
- **Arpeggiator** — tempo-synced to the host. Modes: Up, Down, Up/Down, Down/Up,
  Random, As Played. Rate 1/4 to 1/32 including triplets. Octave range 1-4,
  adjustable gate length, and Hold/latch (releasing Hold drops the latched notes).
  It locks to the host transport grid when playing and free-runs at the host tempo
  when stopped. Controllers such as the mod wheel pass straight through.
- **Presets** — ten of the plugin's own programs (Init, Lush Strings, Analog Brass,
  PWM Pad, Deep Sub Bass, Resonant Sweep, Pipe Organ, Vibrato Keys, Porta Lead,
  Wind Noise), followed by the **128 original Juno-106 factory patches** (Accordion …
  Xylophone), each under its own heading in the preset list and exposed to the host
  as programs. A fresh instance shows only these. **Save** writes the current patch to a `.juno`
  file you choose and adds it to the dropdown. **Load** opens a `.juno` file or a
  **Juno-106 SysEx dump (`.syx`)**; the loaded patch becomes the selection and is
  added to the dropdown for the rest of the session, so you can flip back to it.
  A `.syx` bank adds every patch it contains under a heading named after the file,
  labelled Roland-style A11–B88. Each 106 slider maps to the same position on the
  matching control here, and the plugin's extras (arp, drive, bend, velocity) reset
  to defaults. prev/next step through the whole list.

### Factory bank

`Source/JunoFactoryBank.h` is generated from a Juno-106 librarian `.106` file
(a Java-serialized library of named patches, each holding the standard 18-byte
106 patch data):

```sh
python3 -I Tools/import_106_library.py Juno-106-Factory.106 Source/JunoFactoryBank.h
```

Notes on fidelity: the 106's VCA LEVEL is a trim, so slider 0 maps to −14 dB rather
than silence; patches that play the self-oscillating filter (Tomita, Toy Rhodes, FX
Rise…) start ringing at note-on, as the hardware filter is already running.

The VCF is calibrated to Roland's Juno-106 Service Notes: cutoff 5 Hz–50 kHz across
the FREQ slider, envelope modulation up to 14 octaves, keyboard follow +3/−2 octaves
over the C2–C7 keyboard, and HPF positions of +10 dB bass boost below 150 Hz, flat,
236 Hz and 754 Hz. Envelope times use Roland's spec ranges — attack 1.5 ms–3 s to
full level, decay and release 1.5 ms–12 s time constants — with the fader travel
following the 106 firmware's slider curves (as analysed for the KR-106 emulator),
so imported patches get the hardware's timing. Saved state carries a `calibration`
version; presets and projects saved before a recalibration are converted on load so
they sound as they did.

## Tests

`Tests/run_tests.sh` compiles and runs the arpeggiator, DSP and voice unit tests
against the JUCE modules (Xcode clang only, no cmake):

```sh
JUCE_DIR=~/Development/JUCE Tests/run_tests.sh
```

`Tests/run_integration.sh` links against the built plugin and checks that a preset
save/load round trip restores parameters *and* still produces audio (needs a
Release build first).

CI (`.github/workflows/build.yml`) builds the AU/VST3/Standalone and runs both on
every push.

## Building

Requires Xcode and a JUCE checkout at `~/Development/JUCE` (no brew, no cmake).

```sh
# one-time: build Projucer
xcodebuild -project ~/Development/JUCE/extras/Projucer/Builds/MacOSX/Projucer.xcodeproj \
           -configuration Release build

# regenerate the Xcode project after editing Juno106.jucer
~/Development/JUCE/extras/Projucer/Builds/MacOSX/build/Release/Projucer.app/Contents/MacOS/Projucer \
    --resave Juno106.jucer

# build AU + VST3 + Standalone (universal binary)
cd Builds/MacOSX
xcodebuild -project Juno106.xcodeproj -target "Juno106 - All" -configuration Release build
```

The build copies `Juno106.component` to `~/Library/Audio/Plug-Ins/Components/` and
`Juno106.vst3` to `~/Library/Audio/Plug-Ins/VST3/`.

## Using in Ableton Live

1. Preferences → Plug-Ins → make sure **Audio Units** (or **VST3**) is enabled and rescan.
2. Find it in the browser under Plug-Ins → Audio Units → Alistair Rutherford → Juno 106-ARP.

If Live doesn't see it after a rescan, run `auval -v aumu Jn06 ARut` in Terminal
and/or `killall -9 AudioComponentRegistrar`, then restart Live.
