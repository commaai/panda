#!/usr/bin/env python
import os
import sys
import time
import usb1
import random
import struct
from panda.lib.panda import Panda
from hexdump import hexdump
from itertools import permutations

def get_test_string():
  return "test"+os.urandom(10)

def run_test():
  pandas = Panda.list()
  print pandas

  if len(pandas) == 0:
    print "NO PANDAS"
    assert False

  if len(pandas) == 1:
    # if we only have one on USB, assume the other is on wifi
    pandas.append("WIFI")
  run_test_w_pandas(pandas)

def run_test_w_pandas(pandas):
  h = map(lambda x: Panda(x), pandas)
  print h

  for hh in h:
    hh.set_controls_allowed(True)

  # test both directions
  for ho in permutations(range(len(h)), r=2):
    print "***************** TESTING", ho

    # **** test health packet ****
    print "health", ho[0], h[ho[0]].health()

    # **** test K/L line loopback ****
    for bus in [2,3]:
      # flush the output
      h[ho[1]].kline_drain(bus=bus)

      # send the characters
      st = get_test_string()
      st = "\xaa"+chr(len(st)+3)+st
      h[ho[0]].kline_send(st, bus=bus, checksum=False)

      # check for receive
      ret = h[ho[1]].kline_drain(bus=bus)

      hexdump(st)
      hexdump(ret)
      assert st == ret
      print "K/L pass", bus, ho

    # **** test can line loopback ****
    for bus in [0,1,4,5]:
      print "test can", bus
      # flush
      cans_echo = h[ho[0]].can_recv()
      cans_loop = h[ho[1]].can_recv()

      # set GMLAN mode
      if bus == 5:
        h[ho[0]].set_gmlan(True)
        h[ho[1]].set_gmlan(True)
        bus = 1    # GMLAN is multiplexed with CAN2
        is_gmlan = True
      else:
        h[ho[0]].set_gmlan(False)
        h[ho[1]].set_gmlan(False)
        is_gmlan = False

      # send the characters
      # pick addresses high enough to not conflict with honda code
      at = random.randint(1024, 2000)
      st = get_test_string()[0:8]
      h[ho[0]].can_send(at, st, bus)
      time.sleep(0.1)

      # check for receive
      cans_echo = h[ho[0]].can_recv()
      cans_loop = h[ho[1]].can_recv()

      print bus, cans_echo, cans_loop

      assert len(cans_echo) == 1
      assert len(cans_loop) == 1

      assert cans_echo[0][0] == at
      assert cans_loop[0][0] == at

      assert cans_echo[0][2] == st
      assert cans_loop[0][2] == st

      assert cans_echo[0][3] == bus+2
      assert cans_loop[0][3] == bus

      print "CAN pass", bus, ho

if __name__ == "__main__":
  if len(sys.argv) > 1:
    for i in range(int(sys.argv[1])):
      run_test()
  else :
    i = 0
    while 1:
      print "************* testing %d" % i
      run_test()
    i += 1

