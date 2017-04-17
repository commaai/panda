#!/usr/bin/env python
import sys
import struct

with open(sys.argv[1]) as f:
  dat = f.read()

print "signing", len(dat), "bytes"

with open(sys.argv[2], "wb") as f:
  x = struct.pack("I", len(dat)) + dat[4:]
  # mock signature of dat[4:]
  x += "\xaa"*0x80
  f.write(x)

