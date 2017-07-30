from panda import Panda
from nose.tools import timed, assert_equal, assert_less, assert_greater
import time

def connect_wo_esp():
  # connect to the panda
  p = Panda()

  # power down the ESP
  p.set_esp_power(False)
  return p

  # clear old junk
  while len(p.can_recv()) > 0:
    pass

def time_many_sends(p, bus):
  MSG_COUNT = 100

  st = time.time()
  p.can_send_many([(0x1aa, 0, "\xaa"*8, bus)]*MSG_COUNT)
  r = []

  while len(r) < 200 and (time.time() - st) < 3:
    r.extend(p.can_recv())

  sent_echo = filter(lambda x: x[3] == 0x80 | bus, r)
  loopback_resp = filter(lambda x: x[3] == bus, r)

  assert_equal(len(sent_echo), 100)
  assert_equal(len(loopback_resp), 100)

  et = (time.time()-st)*1000.0
  comp_kbps = (1+11+1+1+1+4+8*8+15+1+1+1+7)*MSG_COUNT / et

  return comp_kbps
