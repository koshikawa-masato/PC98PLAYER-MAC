#!/bin/sh
set -eu
cd "$(dirname "$0")"
./tools/fetch_ymfm.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
printf '\nBuilt: %s/build/PC98PLAYER.app\n' "$PWD"
