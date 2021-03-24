#!/usr/bin/env sh
set -e

PEDAL=1 scons -u
../../tests/pedal/enter_canloader.py; sleep 0.5
PYTHONPATH=../../ python3 -c "from python import Panda; p = [x for x in [Panda(x) for x in Panda.list()] if x.bootstub]; assert(len(p)==1); p[0].flash('../obj/pedal.bin.signed', reconnect=False)"
