#!/usr/bin/env python3
import os
import time
from panda import Panda

if __name__ == "__main__":
  spi = "SPI" in os.environ
  print("using SPI" if spi else "using USB")


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

