#!/bin/sh
set -eu
cd "$(dirname "$0")"
./tools/fetch_ymfm.sh
# Use the simulator by default; IOS_SDK=iphoneos produces a device build.
sdk=${IOS_SDK:-iphonesimulator}
cmake -S . -B "build/ios-$sdk" -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT="$sdk" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DBUILD_TESTING=OFF
cmake --build "build/ios-$sdk" --config Debug --parallel 4
