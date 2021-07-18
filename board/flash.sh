#!/usr/bin/env sh
set -e

scons -u
PYTHONPATH=.. python3 -c "from python import Panda; Panda().flash('obj/panda.bin.signed')"
PYTHONPATH=../ python3 -c "from python import Panda; print('flash succeeded') if Panda().get_signature() == Panda().get_signature_from_firmware('obj/panda.bin') else print('flashing failed')"
