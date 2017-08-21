#!/bin/bash

if [ ! -d "../../pandaextra" ]; then
  echo "No release cert found, cannot build release."
  echo "You probably aren't looking to do this anyway."
  exit
fi

# make ST
pushd .
cd ../board
make clean
RELEASE=1 make obj/panda.bin
popd


# make ESP
pushd .
cd ../boardesp
make clean
RELEASE=1 make user1.bin
RELEASE=1 make user2.bin
popd

# make zip file
pushd .
cd ..
cat VERSION > /tmp/version
echo -en "-" >> /tmp/version
git rev-parse HEAD >> /tmp/version
cat /tmp/version
zip -j release/panda-$(cat /tmp/version).zip ~/one/panda/board/obj/panda.bin ~/one/panda/boardesp/user?.bin /tmp/version
popd

