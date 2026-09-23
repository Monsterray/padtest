#!/usr/bin/env bash
# Build PadTest DX: build/padtest.exe and the CD image build/padtest.bin + .cue.
# Needs PSn00bSDK (C:\PSn00bSDK, PSN00BSDK_LIBS set). Uses the Windows CMake by path, because
# devkitPro's MSYS cmake comes first on this machine's PATH and cannot drive native Ninja.
set -e
cd "$(dirname "$0")"
CMAKE="${CMAKE:-/c/Program Files/CMake/bin/cmake.exe}"
export PSN00BSDK_LIBS="${PSN00BSDK_LIBS:-C:\PSn00bSDK\lib\libpsn00b}"
export PATH="$PATH:/c/PSn00bSDK/bin"
[ -f build/build.ninja ] || "$CMAKE" --preset default .
"$CMAKE" --build build
