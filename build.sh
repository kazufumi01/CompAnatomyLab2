#!/bin/bash
# Full universal build (AU + VST3 + Standalone), installs AU / VST3, then validates the AU.
set -e
cd "$(dirname "$0")"

export DEVELOPER_DIR="/Users/kazufumi/Downloads/Xcode.app/Contents/Developer"
export PATH="$DEVELOPER_DIR/usr/bin:$PATH"

AR=$(xcrun -f ar)
RANLIB=$(xcrun -f ranlib)

cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=$(xcrun -f clang) -DCMAKE_CXX_COMPILER=$(xcrun -f clang++) \
  -DCMAKE_AR="$AR" -DCMAKE_RANLIB="$RANLIB" \
  -DCMAKE_OSX_SYSROOT=$(xcrun --show-sdk-path) -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DJUCE_PATH=$HOME/Documents/jpkplugins/JUCE
cmake --build build --config Release -j

# Install plug-ins where macOS and DAWs discover them.
ART="build/CompAnatomyLab2_artefacts/Release"
mkdir -p /Library/Audio/Plug-Ins/VST3 /Library/Audio/Plug-Ins/Components
rm -rf "/Library/Audio/Plug-Ins/VST3/Compressor Anatomy Lab 2 Dev.vst3"
rm -rf "/Library/Audio/Plug-Ins/Components/Compressor Anatomy Lab 2 Dev.component"
rm -rf "/Library/Audio/Plug-Ins/VST3/Compressor Anatomy Lab 2.vst3"
rm -rf "/Library/Audio/Plug-Ins/Components/Compressor Anatomy Lab 2.component"
cp -R "$ART/VST3/Compressor Anatomy Lab 2.vst3" /Library/Audio/Plug-Ins/VST3/
cp -R "$ART/AU/Compressor Anatomy Lab 2.component" /Library/Audio/Plug-Ins/Components/

auval -v aufx Cmp2 Jpkl | tail -5
