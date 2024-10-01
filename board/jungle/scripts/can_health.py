#!/usr/bin/env python3

import time
from panda import PandaJungle

if __name__ == "__main__":
  jungle = PandaJungle()

  while True:
    print(chr(27) + "[2J") # clear screen
    for bus in range(3):
      print(f"\nBus {bus}:\n{jungle.can_health(bus)}")
    print()
    time.sleep(1)
