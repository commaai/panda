from __future__ import print_function
import sys
import time
from helpers import time_many_sends
from panda import Panda, PandaWifiStreaming

def test_udp_doesnt_drop():
  p = Panda()
  p.set_safety_mode(Panda.SAFETY_ALLOUTPUT)
  p.set_can_loopback(True)

  pwifi = PandaWifiStreaming()

  for i in range(30):
    pwifi.kick()

    speed = 500
    p.set_can_speed_kbps(0, speed)
    comp_kbps = time_many_sends(p, 0, pwifi, msg_count=10)
    saturation_pct = (comp_kbps/speed) * 100.0

    sys.stdout.write(".")
    sys.stdout.flush()



