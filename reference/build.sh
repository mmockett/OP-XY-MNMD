#!/bin/sh
# Builds mdref against a headless build of gearmulator-md-mm beside this repo.
#   sh reference/build.sh            (GEARMULATOR=path to override)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
G=${GEARMULATOR:-$ROOT/../gearmulator-md-mm}
B=$G/temp/headless
if [ ! -f "$B/source/elektron/md/mdLib/libmdLib.a" ]; then
  git -C "$G" submodule update --init --depth 1 source/dsp56300 source/mc68k source/cpp-terminal \
    source/3rdparty/RmlUi source/3rdparty/freetype source/3rdparty/lunasvg
  git -C "$G/source/dsp56300" submodule update --init --depth 1
  cmake -S "$G" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -Dgearmulator_BUILD_JUCEPLUGIN=OFF -Dgearmulator_BUILD_JUCEPLUGIN_CLAP=OFF \
    -Dgearmulator_SYNTH_OSIRUS=OFF -Dgearmulator_SYNTH_OSTIRUS=OFF -Dgearmulator_SYNTH_VAVRA=OFF \
    -Dgearmulator_SYNTH_XENIA=OFF -Dgearmulator_SYNTH_NODALRED2X=OFF -Dgearmulator_SYNTH_JE8086=OFF
  ninja -C "$B" -j 8 mdLib
fi
S=$G/source
mkdir -p "$ROOT/local"
c++ -std=gnu++17 -O3 -Ofast -funroll-loops -fno-lto -fno-stack-protector -fvisibility=hidden -fvisibility-inlines-hidden -arch arm64 -mmacosx-version-min=10.12 -DNDEBUG -DRELEASE -DDSP56300_DEBUGGER=0 -DMD_TRANSPORT_DIAGNOSTICS=0 -DASMJIT_STATIC \
  -I"$S" -I"$S/elektron/md" -I"$S/dsp56300/source" -I"$S/dsp56300/source/asmjit/src" \
  "$ROOT/reference/mdref.cpp" -o "$ROOT/local/mdref" \
  "$B/source/elektron/md/mdLib/libmdLib.a" "$B/source/hardwareLib/libhardwareLib.a" "$B/source/mc68k/lib68kEmu.a" \
  "$B/source/synthLib/libsynthLib.a" "$B/source/libresample/libresample.a" "$B/source/baseLib/libbaseLib.a" \
  "$B/source/dsp56300/source/dsp56kEmu/libdsp56kEmu.a" "$B/source/dsp56300/source/dsp56kBase/libdsp56kBase.a" \
  "$B/source/dsp56300/source/asmjit/libasmjit.a" -ldl -lpthread
echo "built $ROOT/local/mdref"
