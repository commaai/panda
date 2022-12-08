#!/usr/bin/python3

from panda import Panda


for serial in Panda.list():
  p = Panda(serial)
  print(p.get_type())
  print(p.health())
  print(p.get_serial())
  print(p.get_usb_serial())
  print("================")

