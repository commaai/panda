#!/bin/bash
sudo apt-get install -y make unrar-free autoconf automake libtool gcc g++ gperf \
    flex bison texinfo gawk ncurses-dev libexpat-dev python-dev python python-serial \
    sed git unzip bash help2man wget bzip2
# huh?
sudo apt-get install -y libtool
sudo apt-get install -y libtool-bin
git clone --recursive https://github.com/pfalcon/esp-open-sdk.git
cd esp-open-sdk
git checkout 03f5e898a059451ec5f3de30e7feff30455f7cec
cp ../python2_make.py .

if [ ! -z $DOCKER ]; then
  python2 python2_make.py 'CT_ALLOW_BUILD_AS_ROOT_SURE=y LD_LIBRARY_PATH="" make STANDALONE=y'
else
  python2 python2_make.py 'LD_LIBRARY_PATH="" make STANDALONE=y'
fi
