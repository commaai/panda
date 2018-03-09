#!/usr/bin/env python
from panda import Panda

if __name__ == "__main__":
  p = Panda()
  p.set_safety_mode(0x1337)
  p.can_send(0x200, "\xce\xfa\xad\xde\x1e\x0b\xb0\x0a", 0)

