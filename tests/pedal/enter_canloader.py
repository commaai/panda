#!/usr/bin/env python
import sys
import argparse
from panda import Panda

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='Flash pedal over can')
  parser.add_argument('--recover', action='store_true')
  parser.add_argument("fn", type=str, nargs='?', help="flash file")
  args = parser.parse_args()

  p = Panda()
  p.set_safety_mode(0x1337)

  if args.recover:
    p.can_send(0x200, "\xce\xfa\xad\xde\x1e\x0b\xb0\x02", 0)
    exit(0)
  else:
    p.can_send(0x200, "\xce\xfa\xad\xde\x1e\x0b\xb0\x0a", 0)

  if args.fn:
    print "flashing", args.fn



