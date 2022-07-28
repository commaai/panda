#!/usr/bin/env python3
import time
from panda import Panda

if __name__ == "__main__":
  panda = Panda(spi=True)
  # panda = Panda(spi=False)

  i = 0
  pi = 0
  while True:    
    st = time.monotonic()
    while time.monotonic() - st < 1:
      panda.health()
      i += 1
    print(i, panda.health(), "\n")
    print(f"Speed: {i - pi}Hz")
    pi = i

