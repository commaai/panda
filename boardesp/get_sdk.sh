#!/bin/bash
sudo apt-get install make unrar-free autoconf automake libtool gcc g++ gperf \
    flex bison texinfo gawk ncurses-dev libexpat-dev python-dev python python-serial \
    sed git unzip bash help2man wget bzip2
# huh?
sudo apt-get install libtool
sudo apt-get install libtool-bin
git clone --recursive https://github.com/pfalcon/esp-open-sdk.git
cd esp-open-sdk
git checkout c70543e57fb18e5be0315aa217bca27d0e26d23d
git submodule update --recursive
LD_LIBRARY_PATH="" make STANDALONE=y

