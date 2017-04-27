#!/usr/bin/env python
import os
from panda.lib.panda import Panda
import time

if __name__ == "__main__":
  if os.getenv("WIFI") is not None:
    p = Panda("WIFI")
  else:
    p = Panda()
  print p.health()

  while 1:
    # flood
    p.can_send(0xaa, "\xaa"*8, 0)
    p.can_send(0xaa, "\xaa"*8, 1)
    p.can_send(0xaa, "\xaa"*8, 4)
    time.sleep(0.01)

    print p.can_recv()

