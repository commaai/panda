#!/usr/bin/env python
import os
import sys
import usb1
import time
import select
from panda.lib.panda import Panda

setcolor = ["\033[1;32;40m", "\033[1;31;40m"]
unsetcolor = "\033[00m"

if __name__ == "__main__":
  port_number = int(os.getenv("PORT", 0))

  serials = Panda.list()
  if os.getenv("SERIAL"):
    serials = filter(lambda x: x==os.getenv("SERIAL"), serials)

  pandas = map(lambda x: Panda(x, False), serials)
  while 1:
    for i, panda in enumerate(pandas):
      while 1:
        ret = panda.serial_read(port_number)
        if len(ret) > 0:
          sys.stdout.write(setcolor[i] + ret + unsetcolor)
          sys.stdout.flush()
        else:
          break
      if select.select([sys.stdin], [], [], 0) == ([sys.stdin], [], []):
        ln = sys.stdin.readline()
        panda.serial_write(port_number, ln)
      time.sleep(0.01)

