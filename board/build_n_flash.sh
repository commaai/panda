#!/usr/bin/env sh
set -e

DFU_UTIL="dfu-util"

#scons -u

cd ..

scons --clean
rm -rf board/obj/*
RED=1 scons

#PYTHONPATH=.. python3 -c "from python import Panda; Panda().reset(enter_bootstub=True); Panda().reset(enter_bootloader=True)" || true
sleep 1
$DFU_UTIL -d 0483:df11 -a 0 -s 0x08004000 -D board/obj/panda_red.bin.signed
#$DFU_UTIL -d 0483:df11 -a 0 -s 0x08000000 -D board/obj/bootstub.panda_red.bin
