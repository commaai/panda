import os
import time
from panda import Panda
from nose.tools import timed, assert_equal, assert_less

# must run first
def test_build_download_connect():
  # download the latest code
  Panda.program()

  # connect to the panda
  p = Panda()

def connect_wo_esp():
  # connect to the panda
  p = Panda()

  # power down the ESP
  p.set_esp_power(False)
  return p

def test_can_loopback():
  p = connect_wo_esp()

  # enable output mode
  p.set_safety_mode(Panda.SAFETY_ALLOUTPUT)

  # enable CAN loopback mode
  p.set_can_loopback(True)

  # set bus 0 speed to 250
  p.set_can_speed_kbps(0, 250)

  # send a message on bus 0
  p.can_send(0x1aa, "message", 0)

  # confirm receive both on loopback and send receipt
  time.sleep(0.05)
  r = p.can_recv()
  sr = filter(lambda x: x[3] == 0x80, r)
  lb = filter(lambda x: x[3] == 0, r)
  assert len(sr) == 1
  assert len(lb) == 1

  # confirm data is correct
  assert 0x1aa == sr[0][0] == lb[0][0]
  assert "message" == sr[0][2] == lb[0][2]

def test_safety_nooutput():
  p = connect_wo_esp()

  # enable output mode
  p.set_safety_mode(Panda.SAFETY_NOOUTPUT)

  # enable CAN loopback mode
  p.set_can_loopback(True)

  # send a message on bus 0
  p.can_send(0x1aa, "message", 0)

  # confirm receive nothing
  time.sleep(0.05)
  r = p.can_recv()
  assert len(r) == 0

def test_throughput():
  p = connect_wo_esp()

  # enable output mode
  p.set_safety_mode(Panda.SAFETY_ALLOUTPUT)

  # enable CAN loopback mode
  p.set_can_loopback(True)

  for speed in [100,250,500,1000]:
    # set bus 0 speed to speed
    p.set_can_speed_kbps(0, speed)
    time.sleep(0.05)

    st = time.time()
    for i in range(100):
      # send a message on bus 0
      p.can_send(0x1aa, "message", 0)
    r = []
    while len(r) < 200 and (time.time() - st) < 3:
      r.extend(p.can_recv())
    assert_equal(len(r), 200)
    et = (time.time()-st)*1000.0
    mt = (20000.0/speed)
    assert_less(et, mt)
    print "loopback 100 messages at speed %d in %.2f ms < %.2f ms" % (speed, et, mt)





