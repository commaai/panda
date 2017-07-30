from __future__ import print_function
import requests
import time
from panda import Panda
from helpers import time_many_sends
from nose.tools import timed, assert_equal, assert_less, assert_greater

def test_webpage_fetch():
  r = requests.get("http://192.168.0.10/")
  print(r.text)

  assert "This is your comma.ai panda" in r.text

def test_get_serial_wifi():
  p = Panda("WIFI")
  print(p.get_serial())

def test_throughput():
  p = Panda()

  # enable output mode
  p.set_safety_mode(Panda.SAFETY_ALLOUTPUT)

  # enable CAN loopback mode
  p.set_can_loopback(True)

  p = Panda("WIFI")

  for speed in [100,250,500,750,1000]:
    # set bus 0 speed to speed
    p.set_can_speed_kbps(0, speed)
    time.sleep(0.05)

    comp_kbps = time_many_sends(p, 0)

    # bit count from https://en.wikipedia.org/wiki/CAN_bus
    saturation_pct = (comp_kbps/speed) * 100.0
    #assert_greater(saturation_pct, 80)
    #assert_less(saturation_pct, 100)

    print("WIFI loopback 100 messages at speed %d, comp speed is %.2f, percent %.2f" % (speed, comp_kbps, saturation_pct))


