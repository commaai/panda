#!/usr/bin/env python3
from panda import Panda

if __name__ == "__main__":
  with Panda() as p:
    p.set_safety_mode(Panda.SAFETY_ALLOUTPUT)
    p.set_can_loopback(True)

    for b in range(3):
      p.can_send(0x123, b"msg", b)
    print(p.can_recv())
    print(p.can_recv())
