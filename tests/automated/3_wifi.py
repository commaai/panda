from __future__ import print_function
import os
from panda import Panda

def test_get_serial():
  p = Panda()
  print(p.get_serial())

def test_get_serial_in_flash_mode():
  p = Panda()
  p.reset(enter_bootstub=True)
  assert(p.bootstub)
  print(p.get_serial())
  p.reset()

def test_connect_wifi():
  p = Panda()
  ssid, pw = p.get_serial()
  ssid = ssid.strip("\x00")
  assert(ssid.isalnum())
  assert(pw.isalnum())
  ssid = "panda-" + ssid

  # Mac OS X only
  # TODO: Ubuntu
  os.system("networksetup -setairportnetwork en0 %s %s" % (ssid, pw))



