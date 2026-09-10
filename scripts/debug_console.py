#!/usr/bin/env python3

import os
import sys
import time
import select
import codecs

from panda import Panda

setcolor = ["\033[1;32;40m", "\033[1;31;40m"]
unsetcolor = "\033[00m"

port_number = int(os.getenv("PORT", "0"))
claim = os.getenv("CLAIM") is not None
no_color = os.getenv("NO_COLOR") is not None
no_reconnect = os.getenv("NO_RECONNECT") is not None


def _stdin_has_line() -> bool:
  # select() only works on sockets on Windows, so poll the console with msvcrt there
  if sys.platform == "win32":
    import msvcrt
    return bool(msvcrt.kbhit())
  return select.select([sys.stdin], [], [], 0) == ([sys.stdin], [], [])


if __name__ == "__main__":
  while True:
    try:
      serials = Panda.list()
      if os.getenv("SERIAL"):
        serials = [x for x in serials if x == os.getenv("SERIAL")]

      pandas = [Panda(x, claim=claim) for x in serials]
      decoders = [codecs.getincrementaldecoder('utf-8')() for _ in pandas]

      if not len(pandas):
        print("no pandas found")
        if no_reconnect:
          sys.exit(0)
        time.sleep(1)
        continue

      if os.getenv("BAUD") is not None:
        for panda in pandas:
          panda.set_uart_baud(port_number, int(os.getenv("BAUD")))  # type: ignore

      while True:
        for i, panda in enumerate(pandas):
          while True:
            ret = panda.serial_read(port_number)
            if len(ret) > 0:
              decoded = decoders[i].decode(ret)
              if no_color:
                sys.stdout.write(decoded)
              else:
                sys.stdout.write(setcolor[i] + decoded + unsetcolor)
              sys.stdout.flush()
            else:
              break
          if _stdin_has_line():
            ln = sys.stdin.readline()
            if claim:
              panda.serial_write(port_number, ln)
          time.sleep(0.01)
    except KeyboardInterrupt:
      break
    except Exception:
      print("panda disconnected!")
      time.sleep(0.5)
