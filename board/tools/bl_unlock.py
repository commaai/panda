#!/usr/bin/env python
import sys
import usb1
import struct
from hexdump import hexdump

def dostat():
  while 1:
    dat = dev.controlRead(0x21, DFU_GETSTATUS, 0, 0, 6)
    hexdump(dat)
    if dat[1] == "\x00":
      break

context = usb1.USBContext()
dev = context.openByVendorIDAndProductID(0x0483, 0xdf11)
dev.claimInterface(0)
print dev

DFU_DNLOAD = 1
DFU_UPLOAD = 2
DFU_GETSTATUS = 3
DFU_CLRSTATUS = 4
DFU_ABORT = 6

# Clear status
stat = dev.controlRead(0x21, DFU_GETSTATUS, 0, 0, 6)
hexdump(stat)
if stat[4] == "\x0a":
  dev.controlRead(0x21, DFU_CLRSTATUS, 0, 0, 0)
elif stat[4] == "\x09":
  dev.controlWrite(0x21, DFU_ABORT, 0, 0, "")
  dostat()
hexdump(dev.controlRead(0x21, DFU_GETSTATUS, 0, 0, 6))

if len(sys.argv) > 1 and sys.argv[1] == "--unprotect":
  # Read Unprotect
  dev.controlWrite(0x21, DFU_DNLOAD, 0, 0, "\x92")
  dostat()
  exit(0)

# Set Address Pointer
dev.controlWrite(0x21, DFU_DNLOAD, 0, 0, "\x21" + struct.pack("I", 0x1fffc000))
dostat()

# Abort
dev.controlWrite(0x21, DFU_ABORT, 0, 0, "")
dostat()

# Dump
val = dev.controlRead(0xA1, DFU_UPLOAD, 2, 0, 0x10)
print "OLD:",
hexdump(val)

# Abort
dev.controlWrite(0x21, DFU_ABORT, 0, 0, "")
dostat()

# Set Address Pointer
dev.controlWrite(0x21, DFU_DNLOAD, 0, 0, "\x21" + struct.pack("I", 0x1fffc000))
dostat()

if len(sys.argv) > 1 and sys.argv[1] == "--lock":
  val = "\xef\xaa\x10\x55"*2 + "\xfe\x7f\x01\x80"*2
else:
  val = "\xef\xaa\x10\x55"*2 + "\xff\x7f\x00\x80"*2
print "NEW:",
hexdump(val)

# Program
dev.controlWrite(0x21, DFU_DNLOAD, 2, 0, val)

# triggers reboot
dat = dev.controlRead(0x21, DFU_GETSTATUS, 0, 0, 6)
hexdump(dat)

