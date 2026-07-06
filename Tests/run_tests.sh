#!/usr/bin/env bash
# Compiles and runs the Juno106-ARP unit tests against the JUCE modules.
# Requires only Xcode's clang and a JUCE checkout (no cmake, no brew).
#
#   JUCE_DIR=/path/to/JUCE ./run_tests.sh
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$HERE/../Source"
JUCE_DIR="${JUCE_DIR:-$HOME/Development/JUCE}"

if [[ ! -d "$JUCE_DIR/modules/juce_core" ]]; then
    echo "JUCE not found at $JUCE_DIR (set JUCE_DIR)." >&2
    exit 2
fi

BUILD="$HERE/build"
mkdir -p "$BUILD"

DEFS=(-DNDEBUG=1
      -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
      -DJUCE_MODULE_AVAILABLE_juce_core=1
      -DJUCE_MODULE_AVAILABLE_juce_audio_basics=1
      -DJUCE_STANDALONE_APPLICATION=1
      -DJUCE_USE_CURL=0 -DJUCE_WEB_BROWSER=0)

FRAMEWORKS=(-framework Cocoa -framework Foundation -framework CoreFoundation
            -framework Security -framework IOKit -framework CoreAudio
            -framework CoreMIDI -framework AudioToolbox -framework Accelerate
            -framework CoreServices)

compile_and_run () {
    local name="$1"
    echo "== building $name =="
    clang++ -std=c++17 -x objective-c++ -O1 "${DEFS[@]}" \
        -I "$HERE/shim" -I "$JUCE_DIR/modules" -I "$SRC" \
        "$HERE/$name.cpp" \
        "$JUCE_DIR/modules/juce_core/juce_core.mm" \
        "$JUCE_DIR/modules/juce_core/juce_core_CompilationTime.cpp" \
        "$JUCE_DIR/modules/juce_audio_basics/juce_audio_basics.mm" \
        "${FRAMEWORKS[@]}" \
        -o "$BUILD/$name"
    "$BUILD/$name"
}

compile_and_run test_arp
compile_and_run test_dsp

echo ""
echo "All test binaries passed."
