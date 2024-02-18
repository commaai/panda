#!/usr/bin/env python3
import time
from panda import Panda

if __name__ == "__main__":
  i = 0
  pi = 0

  panda = Panda()
  while True:
    st = time.monotonic()
    while time.monotonic() - st < 1:
      panda.can_recv()
      i += 1
    #print(i, panda.health(), "\n")
    h = panda.health()
    print(f"Speed: {i - pi}Hz")
    print(f"- spi {h['spi_checksum_error_count']:6d}")
    print()
    pi = i

