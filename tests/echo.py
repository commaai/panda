#!/usr/bin/env python3
import os
import sys

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), ".."))
from panda import Panda  # noqa: E402

# This script is intended to be used in conjunction with the echo_loopback_test.py test script from panda jungle.
# It sends a reversed response back for every message received containing b"test".
if __name__ == "__main__":
  p = Panda()
  p.set_safety_mode(Panda.SAFETY_ALLOUTPUT)
  p.set_power_save(False)

  while True:
    incoming = p.can_recv()
    for message in incoming:
      address, notused, data, bus = message
      if b'test' in data:
        p.can_send(address, data[::-1], bus)
