#!/usr/bin/env python
import os
from panda.lib.panda import Panda

if __name__ == "__main__":
  if os.getenv("WIFI") is not None:
    p = Panda("WIFI")
  else:
    p = Panda()
  print p.health()

