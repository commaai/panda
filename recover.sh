#!/usr/bin/env sh
set -e

DFU_UTIL="dfu-util"

scons

PYTHONPATH=. python3 -c "from python import Panda; Panda().reset(enter_bootstub=True); Panda().reset(enter_bootloader=True)"
sleep 1
$DFU_UTIL -d 0483:df11 -a 0 -s 0x08004000 -D board/obj/panda.bin.signed
$DFU_UTIL -d 0483:df11 -a 0 -s 0x08000000:leave -D board/obj/bootstub.bin
