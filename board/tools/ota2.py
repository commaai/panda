#!/usr/bin/env python
import sys
import socket
from hexdump import hexdump
import time
from tqdm import tqdm

def cmd(sock, d1, d2=0, dat=None):
  if dat == None:
    dat = "\x00"*0x10
  sock.send(chr(d1) + chr(0xff^d1) + chr(d2) + chr(0xff^d2) + dat)

def main():
  cntl = socket.create_connection(("192.168.0.10", 3333))

  cntl.send("br")
  time.sleep(0.1)

  dat = open(sys.argv[1]).read()
  dat += "\xff"*(0x10-(len(dat)%0x10))
  print "flashing 0x%X bytes" % len(dat)

  sock = socket.create_connection(("192.168.0.10", 1337))
  
  # unlock flash
  cmd(sock, 0x10)
  sock.recv(0x100)

  # erase sector
  cmd(sock, 0x11, 1)
  sock.recv(0x100)

  # wait for erase
  while 1:
    cmd(sock, 0xf)
    ret = sock.recv(0x100)
    if ret[-4:] == "\xde\xad\xd0\x0d":
      break
    time.sleep(0.05)

  # program
  for i in tqdm(range(0, len(dat), 0x10)):
    td = dat[i:i+0x10]
    cmd(sock, 0x12, 0, td)
    ret = sock.recv(0x100)
    #hexdump(ret)
    assert ret[0x30:0x40] == td

  # reboot normal
  cntl.send("nr")
  time.sleep(0.1)

if __name__ == "__main__":
  main()

