#!/usr/bin/env sh
set -e

DFU_UTIL="dfu-util"

scons
PYTHONPATH=. python3 -c "from python import Panda; Panda().flash('board/obj/panda.bin.signed')"
