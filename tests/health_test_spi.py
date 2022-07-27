#!/usr/bin/env python3
import time
from panda import Panda

if __name__ == "__main__":
  panda = Panda(spi=True)

  while True:
    print(panda.health())
    print("\n")
    time.sleep(0.01)
