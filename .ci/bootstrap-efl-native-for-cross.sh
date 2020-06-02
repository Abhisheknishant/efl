#!/bin/bash

set -e

travis_fold start "cross-native"
travis_time_start "cross-native"
mkdir build-bootstrap-native
meson --prefix=/usr/ --libdir=/usr/lib -Dbuild-examples=false -Dbuild-tests=false -Dbindings=cxx build-bootstrap-native
ninja -C build-bootstrap-native install
rm -rf build-bootstrap-native
ldconfig
travis_time_finish "cross-native"
travis_fold end "cross-native"
