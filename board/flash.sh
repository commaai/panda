#!/usr/bin/env sh
set -e

DFU_UTIL="dfu-util"

scons -u
PYTHONPATH=.. python3 -c "from python import Panda; Panda().flash('obj/panda.bin.signed')"
