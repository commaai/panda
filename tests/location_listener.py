#!/usr/bin/env python
import os
import time
import sys

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), ".."))
from panda import Panda, PandaSerial

def add_nmea_checksum(msg):
  d = msg[1:]
  cs = 0
  for i in d:
    cs ^= ord(i)
  return msg + "*%02X" % cs

if __name__ == "__main__":
  panda = Panda()
  ser = PandaSerial(panda, 1, 9600)

  # power cycle by toggling reset
  print "resetting"
  panda.set_esp_power(0)
  time.sleep(0.1)
  panda.set_esp_power(1)
  time.sleep(0.5)
  print "done"
  print ser.read(1024)

  # upping baud rate
  print "upping baud rate (broken)"
  msg = add_nmea_checksum("$PUBX,41,1,0007,0003,9600,0")+"\r\n"
  print msg
  ser.write(msg)

  # new panda serial
  ser = PandaSerial(panda, 1, 9600)

  while True:
    ret = ser.read(1024)
    if len(ret) > 0:
      sys.stdout.write(ret)
      sys.stdout.flush()
      #print str(ret).encode("hex")

