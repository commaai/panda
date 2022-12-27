#!/usr/bin/env python3
import os
import time
from panda import Panda

from common.realtime import config_realtime_process

if __name__ == "__main__":
  spi = "SPI" in os.environ
  print("using SPI" if spi else "using USB")

  config_realtime_process([4, ], 5)

  i = 0
  pi = 0

  panda = Panda(spi=spi)
  while True:
    st = time.monotonic()
    while time.monotonic() - st < 1:
      panda.health()
      i += 1
    print(i, panda.health(), "\n")
    print(f"Speed: {i - pi}Hz")
    pi = i

