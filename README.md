# Juno106AU

A Roland Juno-106 style polyphonic synthesizer, built with JUCE as an Audio Unit
(plus a Standalone app for quick testing). Loads into Ableton Live and any other
AU host on macOS.

## Architecture (faithful to the original signal path)

```
6 voices, each:  DCO (saw + PWM pulse + sub + noise) --> VCF (4-pole ZDF lowpass) --> VCA (env/gate)
then, global:    HPF (4-position switch) --> BBD chorus (I / II / I+II) --> stereo out
```

- **DCO** — PolyBLEP band-limited sawtooth and pulse with PWM (manual or LFO),
  square sub-oscillator one octave down, white noise. Range switch 16' / 8' / 4'.
- **VCF** — zero-delay-feedback 4-pole (24 dB/oct) lowpass in the spirit of the
  IR3109 OTA cascade, with resonance, envelope amount (normal/inverted polarity),
  LFO modulation and key follow.
- **ENV** — single exponential ADSR shared by filter and amp, like the real unit.
- **VCA** — envelope or gate mode, level control.
- **LFO** — global free-running triangle with rate and per-note onset delay.
- **HPF** — 4-position switch: position 0 is a gentle bass boost, 1 flat, 2 and 3
  progressively cut lows.
- **Chorus** — BBD-style stereo chorus; buttons I, II, or both together for the
  fast vibrato-like third mode. Wet taps modulate in opposite directions per side.
- 6-voice polyphony with voice stealing; velocity is ignored, as on the original.
- **Portamento** — glide time fader (far left); 0 = off. An addition over the
  original hardware.
- **Mod wheel** (CC1) — adds immediate LFO vibrato, up to ± half a semitone.
- **Factory presets** — ten programs (Init, Lush Strings, Analog Brass, PWM Pad,
  Deep Sub Bass, Resonant Sweep, Pipe Organ, Vibrato Keys, Porta Lead, Wind Noise)
  exposed through the AU preset menu in the host, plus an in-plugin preset browser
  in the panel header.
- **Arpeggiator** (not on the original 106) — tempo-synced to the host. Modes:
  Up, Down, Up/Down, Down/Up, Random, As Played. Rate: 1/4 to 1/32 including
  triplets. Octave range 1-4, adjustable gate length, and Hold/latch. It locks to
  the host's transport grid when playing and free-runs (at the host tempo) when
  stopped, so held chords still arpeggiate. Controllers such as the mod wheel pass
  straight through.

## Building

Requires Xcode and a JUCE checkout at `~/Development/JUCE` (no brew, no cmake).

```sh
# one-time: build Projucer
xcodebuild -project ~/Development/JUCE/extras/Projucer/Builds/MacOSX/Projucer.xcodeproj \
           -configuration Release build

# regenerate the Xcode project after editing Juno106.jucer
~/Development/JUCE/extras/Projucer/Builds/MacOSX/build/Release/Projucer.app/Contents/MacOS/Projucer \
    --resave Juno106.jucer

# build AU + Standalone
cd Builds/MacOSX
xcodebuild -project Juno106.xcodeproj -target "Juno106 - All" -configuration Release build
```

The build copies `Juno106.component` to `~/Library/Audio/Plug-Ins/Components/`.

## Using in Ableton Live

1. Preferences → Plug-Ins → make sure **Audio Units** is enabled and rescan.
2. Find it in the browser under Plug-Ins → Audio Units → Alistair Rutherford → Juno 106.

If Live doesn't see it after a rescan, run `auval -v aumu Jn06 ARut` in Terminal
and/or `killall -9 AudioComponentRegistrar`, then restart Live.
