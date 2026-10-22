#!/bin/bash
# Checks that no parameter ID ever changes or disappears, and runs the parameter tests.
#   tools/check_parameters.sh         compare with tests/reference/parameters.txt, then the tests
#   tools/check_parameters.sh write   write a new reference (only when parameters are added on purpose)
set -e
cd "$(dirname "$0")/.."
cmake -B build-tools -G Ninja -DCMAKE_BUILD_TYPE=Release -DFPH_PARAMS=ON > /dev/null
cmake --build build-tools --target FasePhuckerParams
build-tools/FasePhuckerParams list > build-tools/parameters.txt
if [ "$1" = "write" ]; then
    mkdir -p tests/reference
    cp build-tools/parameters.txt tests/reference/parameters.txt
    echo "Reference written: $(wc -l < tests/reference/parameters.txt | tr -d ' ') parameters."
else
    diff -u tests/reference/parameters.txt build-tools/parameters.txt && echo "Parameters unchanged: $(wc -l < build-tools/parameters.txt | tr -d ' ')."
fi
build-tools/FasePhuckerParams test
