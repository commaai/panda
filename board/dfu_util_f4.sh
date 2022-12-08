#!/usr/bin/env sh
set -e

scons -u -j$(nproc)

DFU_UTIL="dfu-util --serial B63636593036"

$DFU_UTIL -d 0483:df11 -a 0 -s 0x08004000 -D obj/panda.bin.signed
$DFU_UTIL -d 0483:df11 -a 0 -s 0x08000000:leave -D obj/bootstub.panda.bin

DFU_UTIL="dfu-util --serial B64536583036"

$DFU_UTIL -d 0483:df11 -a 0 -s 0x08004000 -D obj/panda.bin.signed
$DFU_UTIL -d 0483:df11 -a 0 -s 0x08000000:leave -D obj/bootstub.panda.bin

DFU_UTIL="dfu-util --serial B63F36583036"

$DFU_UTIL -d 0483:df11 -a 0 -s 0x08004000 -D obj/panda.bin.signed
$DFU_UTIL -d 0483:df11 -a 0 -s 0x08000000:leave -D obj/bootstub.panda.bin
