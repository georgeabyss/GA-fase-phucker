#!/bin/bash
# The DSP checks and renders.
#   tools/check_dsp.sh                 measured checks, then the renders against tests/reference/renders
#   tools/check_dsp.sh write           write new reference renders (only when the sound changes on purpose)
#   tools/check_dsp.sh listen <dir>    longer musical renders to listen to
set -e
cd "$(dirname "$0")/.."
cmake -B build-tools -G Ninja -DCMAKE_BUILD_TYPE=Release -DFPH_RENDER=ON > /dev/null
cmake --build build-tools --target FasePhuckerRender
case "$1" in
    write)  build-tools/FasePhuckerRender write tests/reference/renders ;;
    listen) build-tools/FasePhuckerRender listen "${2:-build-tools/listen}" ;;
    *)      build-tools/FasePhuckerRender check tests/reference/renders ;;
esac
