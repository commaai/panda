#!/usr/bin/env python
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

# Read Unprotect
#dev.controlWrite(0x21, DFU_DNLOAD, 0, 0, "\x92")
#hexdump(dev.controlRead(0x21, DFU_GETSTATUS, 0, 0, 6))

# Set Address Pointer
dev.controlWrite(0x21, DFU_DNLOAD, 0, 0, "\x21" + struct.pack("I", 0x1fffc000))
dostat()

# Abort
dev.controlWrite(0x21, DFU_ABORT, 0, 0, "")
dostat()

# Dump
val = dev.controlRead(0xA1, DFU_UPLOAD, 2, 0, 0x10)
hexdump(val)

# Abort
dev.controlWrite(0x21, DFU_ABORT, 0, 0, "")
dostat()

# Set Address Pointer
dev.controlWrite(0x21, DFU_DNLOAD, 0, 0, "\x21" + struct.pack("I", 0x1fffc000))
dostat()

#val = val[0:8] + "\xfe\x7f\x01\x80"*2
val = val[0:8] + "\xff\x7f\x00\x80"*2

# Program
dev.controlWrite(0x21, DFU_DNLOAD, 2, 0, val)
dostat()

