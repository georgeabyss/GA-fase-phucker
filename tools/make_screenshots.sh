#!/bin/bash
# Screenshots of the real interface.
#   tools/make_screenshots.sh           check against tests/reference/screenshots (any differing pixel fails)
#   tools/make_screenshots.sh baseline  write new references (only when the interface changes on purpose)
set -e
cd "$(dirname "$0")/.."
cmake -B build-tools -G Ninja -DCMAKE_BUILD_TYPE=Release -DFPH_SCREENSHOTS=ON > /dev/null
cmake --build build-tools --target FasePhuckerScreenshots
case "$1" in
    baseline) build-tools/FasePhuckerScreenshots tests/reference/screenshots ;;
    *)        build-tools/FasePhuckerScreenshots check tests/reference/screenshots ;;
esac
