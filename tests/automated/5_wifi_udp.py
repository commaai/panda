from __future__ import print_function
import sys
import time
from helpers import time_many_sends, connect_wifi, test_white, panda_color_to_serial
from panda import Panda, PandaWifiStreaming
from nose.tools import timed, assert_equal, assert_less, assert_greater

@test_white
@panda_color_to_serial
def test_udp_doesnt_drop(serial=None):
  connect_wifi(serial)

  p = Panda(serial)
  p.set_safety_mode(Panda.SAFETY_ALLOUTPUT)
  p.set_can_loopback(True)

  pwifi = PandaWifiStreaming()
  while 1:
    if len(pwifi.can_recv()) == 0:
      break

  for msg_count in [1, 100]:
    saturation_pcts = []
    for i in range({1: 0x80, 100: 0x20}[msg_count]):
      pwifi.kick()

      speed = 500
      p.set_can_speed_kbps(0, speed)
      comp_kbps = time_many_sends(p, 0, pwifi, msg_count=msg_count, msg_id=0x100+i)
      saturation_pct = (comp_kbps/speed) * 100.0

      if msg_count == 1:
        sys.stdout.write(".")
        sys.stdout.flush()
      else:
        print("UDP WIFI loopback %d messages at speed %d, comp speed is %.2f, percent %.2f" % (msg_count, speed, comp_kbps, saturation_pct))
        assert_greater(saturation_pct, 20) #sometimes the wifi can be slow...
        assert_less(saturation_pct, 100)
        saturation_pcts.append(saturation_pct)
    if len(saturation_pcts) > 0:
      assert_greater(sum(saturation_pcts)/len(saturation_pcts), 60)
    print("")
