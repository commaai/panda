#!/bin/bash
git clone --recursive https://github.com/pfalcon/esp-open-sdk.git
cd esp-open-sdk
git checkout c70543e57fb18e5be0315aa217bca27d0e26d23d
git submodule update --recursive
LD_LIBRARY_PATH="" make STANDALONE=y
