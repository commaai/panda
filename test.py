#!/usr/bin/env python3
from panda.python.spi import PandaSpiHandle


h = PandaSpiHandle()
with h.dev.acquire() as spi:
  for _ in range(10):
    print(_, bytes(spi.xfer2([0]*4096)[:10]))
