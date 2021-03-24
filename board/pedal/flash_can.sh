#!/usr/bin/env sh
set -e

PEDAL=1 scons -u
../../tests/pedal/enter_canloader.py ../obj/pedal.bin.signed
