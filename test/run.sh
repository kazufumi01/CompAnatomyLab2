#!/bin/bash
# Native build of the test apps, then DSP checks and screenshots (test/shots/*.png).
# MODEL=n (Models.h order) PRESET=n (Presets.h order) FREEZE=1 (input stops) NOFEED=1 (no audio at all) ATT2= (change attack while frozen)
cd "$(dirname "$0")"
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=$(xcrun -f clang) -DCMAKE_CXX_COMPILER=$(xcrun -f clang++) \
  -DCMAKE_OSX_SYSROOT=$(xcrun --show-sdk-path) -DJUCE_PATH=$HOME/Documents/jpkplugins/JUCE -DPLUG=$HOME/Developer/CompAnatomyLab2 > /dev/null
cmake --build build -j > build.log 2>&1 || { grep -E "error" build.log | sort -u | head -40; echo "BUILD FAILED"; exit 1; }
B=build/PTest_artefacts/Release/PTest; [ -f "$B" ] || B=build/PTest_artefacts/PTest
D=build/DTest_artefacts/Release/DTest; [ -f "$D" ] || D=build/DTest_artefacts/DTest
mkdir -p shots
$D > shots/dsp.txt 2>&1
JA=1 CARD=1 LESSON=0 MODEL=0 $B shots/lesson_01.png
JA=1 CARD=1 LESSON=12 SEE=2 MODEL=1 INPUT=6 $B shots/lesson_13.png
CARD=1 LESSON=22 MODEL=11 $B shots/lesson_23_en.png
JA=1 CARD=2 MODEL=1 INPUT=6 $B shots/ear.png
JA=1 MODEL=1 INPUT=6 $B shots/help.png
echo DONE
