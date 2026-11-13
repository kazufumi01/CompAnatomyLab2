#!/bin/bash
# Full universal build (AU + VST3 + Standalone), installs AU / VST3, then validates the AU.
set -e
cd "$(dirname "$0")"
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=$(xcrun -f clang) -DCMAKE_CXX_COMPILER=$(xcrun -f clang++) \
  -DCMAKE_OSX_SYSROOT=$(xcrun --show-sdk-path) -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DJUCE_PATH=$HOME/Documents/jpkplugins/JUCE
cmake --build build --config Release -j
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -v aufx Cmp2 Jpkl | tail -3
