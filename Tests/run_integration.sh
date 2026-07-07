#!/usr/bin/env bash
# Integration test: constructs the real Juno106AudioProcessor and verifies the
# preset save/load round trip restores parameters AND produces audio.
#
# Unlike run_tests.sh this links against the compiled plugin, so it needs a
# Release build first:
#     (cd Builds/MacOSX && xcodebuild -project Juno106.xcodeproj \
#          -target "Juno106 - All" -configuration Release build)
#     JUCE_DIR=~/Development/JUCE Tests/run_integration.sh
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ="$HERE/.."
JUCE_DIR="${JUCE_DIR:-$HOME/Development/JUCE}"
LIB="$PROJ/Builds/MacOSX/build/Release/libJuno106.a"
XCPROJ="$PROJ/Builds/MacOSX/Juno106.xcodeproj"

[[ -f "$LIB" ]] || { echo "Build the plugin first (Release). Missing $LIB" >&2; exit 2; }

BUILD="$HERE/build"; mkdir -p "$BUILD"

# The processor sources use JucePlugin_* macros that Xcode passes on the command
# line. Reproduce them as a defines header from the target build settings.
xcodebuild -project "$XCPROJ" -target "Juno106 - Standalone Plugin" \
           -configuration Release -showBuildSettings -json 2>/dev/null > "$BUILD/settings.json"
/usr/bin/python3 - "$BUILD/settings.json" "$BUILD/defs.h" <<'PY'
import json, shlex, sys
data = json.load(open(sys.argv[1]))
defs = next(e["buildSettings"]["GCC_PREPROCESSOR_DEFINITIONS"]
            for e in data if "GCC_PREPROCESSOR_DEFINITIONS" in e.get("buildSettings", {}))
with open(sys.argv[2], "w") as f:
    for t in shlex.split(defs):
        k, _, v = t.partition("=")
        f.write(f"#define {k} {v}\n" if _ else f"#define {k}\n")
PY

clang++ -std=c++17 -x objective-c++ -O0 -include "$BUILD/defs.h" \
    -I "$PROJ/JuceLibraryCode" -I "$JUCE_DIR/modules" -I "$PROJ/Source" \
    -c "$HERE/test_state.cpp" -o "$BUILD/test_state.o"

clang++ -O0 "$BUILD/test_state.o" "$LIB" \
    -framework Cocoa -framework Foundation -framework CoreFoundation -framework Security \
    -framework IOKit -framework CoreAudio -framework CoreMIDI -framework CoreAudioKit \
    -framework AudioToolbox -framework Accelerate -framework CoreServices -framework AudioUnit \
    -framework Carbon -framework QuartzCore -framework Metal -framework WebKit \
    -o "$BUILD/test_state"

"$BUILD/test_state"
