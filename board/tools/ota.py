#!/usr/bin/env python
import sys
from hexdump import hexdump
import time
import socket
import struct

def connect(uart, cntl):
  uart.send("\x00\xff")
  try:
    while 1:
      ret = uart.recv(8192)
      if ret[0] != '\x79':
        print "GOT IDLE CRAP!"
        hexdump(ret)
      if ret[-1] == "\x79":
        print "already in bootloader"
        return
  except socket.timeout:
    print "first try entering bootloader"
    pass
  while 1:
    try:
      # boot, reset, uart is low for a while
      print "resetting"
      cntl.send("br")
      time.sleep(0.1)

      print "entering bootloader"
      uart.send("\x7f")
      ret = uart.recv(8192)
      hexdump(ret)
      if ret[-1] == "\x79":
        # phase 2, send get command
        # if this works it looks like the interface is stable
        time.sleep(0.1)
        uart.send("\x00\xff")
        ret = uart.recv(8192)
        hexdump(ret)
        if len(ret) == 0xf:
          print "CONNECTED!"
          break
      
      # retrying, drain first
      while 1:
        print "extra:",
        hexdump(uart.recv(8192))
    except socket.timeout:
      print "timeout"
      pass
    print "retrying in 100ms..."
    time.sleep(0.1)


def cmd(x, odat, resp=False):
  x.write(odat)
  x.flush()
  # recv until ACK
  while 1:
    d = x.read(1)
    if d == '\x1f':
      raise Exception("NACK")
    elif d == '\x79':
      break
    else:
      print "WTF got "+d.encode("hex")
      continue
  if resp:
    ll = ord(x.read(1))+1
    print "reading %d bytes" % ll
    dat = x.read(ll)
    hexdump(dat)
    ack2 = x.read(1)
    assert ack2 == '\x79'
    return dat
  else:
    return True

def cksum(x):
  ret = 0
  for c in x:
    ret ^= ord(c)
  return x + chr(ret)

def main():
  cntl = socket.create_connection(("192.168.0.10", 3333))
  uart = socket.create_connection(("192.168.0.10", 3334))
  print cntl, uart

  # connect in socket mode
  uart.settimeout(0.2)
  s = connect(uart, cntl)

  # file mode from here on
  uart.setblocking(True)
  uart = uart.makefile()

  cmd(uart, "\x00\xff", True)
  cmd(uart, "\x02\xfd", True)

  if len(sys.argv) == 1:
    return

  # flash file
  dat = open(sys.argv[1]).read()
  print "flashing 0x%X bytes" % len(dat)
  #addr = 0x08000000
  addr = 0x08004000

  # erase sector 1
  cmd(uart, "\x44\xbb")
  print "sent erase"
  #cmd(uart, cksum("\x00\x00\x00\x00"))
  cmd(uart, cksum("\x00\x00\x00\x01"))
  print "erase done"

  # fits in sector 0
  for a in range(0, len(dat), 0x100):
    print "flashing 0x%x" % (addr+a)
    cmd(uart, "\x31\xce")
    cmd(uart, cksum(struct.pack("!I", addr+a)))
    dd = dat[a:a+0x100]
    cmd(uart, cksum(chr(len(dd)-1)+dd))
  print "flashing done"

  cntl.send("nr")
  print "rebooted in normal mode"
    
if __name__ == "__main__":
  main()


