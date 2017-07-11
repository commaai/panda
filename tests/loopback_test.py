#!/usr/bin/env python
from __future__ import print_function
import os
import sys
import time
import random
import argparse

from hexdump import hexdump
from itertools import permutations

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), ".."))
from panda import Panda

def get_test_string():
  return b"test"+os.urandom(10)

def run_test(can_speeds, gmlan_speeds, sleep_duration=0):
  pandas = Panda.list()
  print(pandas)

  if len(pandas) == 0:
    print("NO PANDAS")
    assert False

  if len(pandas) == 1:
    # if we only have one on USB, assume the other is on wifi
    pandas.append("WIFI")
  run_test_w_pandas(pandas, can_speeds, gmlan_speeds, sleep_duration)

def run_test_w_pandas(pandas, can_speeds, gmlan_speeds, sleep_duration=0):
  h = list(map(lambda x: Panda(x), pandas))
  print("H", h)

  for hh in h:
    hh.set_controls_mode()

  # test both directions
  for ho in permutations(range(len(h)), r=2):
    print("***************** TESTING", ho)

    panda_snd, panda_rcv = h[ho[0]], h[ho[1]]

    if(panda_snd._serial == "WIFI"):
      print("  *** Can not send can data over wifi panda. Skipping! ***")
      continue

    # **** test health packet ****
    print("health", ho[0], panda_snd.health())

    # **** test K/L line loopback ****
    for bus in [2,3]:
      # flush the output
      panda_rcv.kline_drain(bus=bus)

      # send the characters
      st = get_test_string()
      st = b"\xaa"+chr(len(st)+3).encode()+st
      panda_snd.kline_send(st, bus=bus, checksum=False)

      # check for receive
      ret = panda_rcv.kline_drain(bus=bus)

      print("ST Data:")
      hexdump(st)
      print("RET Data:")
      hexdump(ret)
      assert st == ret
      print("K/L pass", bus, ho, "\n")
      time.sleep(sleep_duration)

    # **** test can line loopback ****
    for bus, gmlan in [(0, None), (1, False), (2, False), (1, True), (2, True)]:
      print("\ntest can", bus, "gmlan" if gmlan else "")
      # flush
      cans_echo = panda_snd.can_recv()
      cans_loop = panda_rcv.can_recv()

      # set GMLAN mode
      if(gmlan is not None):
        panda_snd.set_gmlan(bus, gmlan)
        panda_rcv.set_gmlan(bus, gmlan)

      if gmlan:
        print("Setting GMLAN %d Speed to %d" % (bus, gmlan_speeds[bus]))
        panda_snd.set_can_baud(bus, gmlan_speeds[bus])
        panda_rcv.set_can_baud(bus, gmlan_speeds[bus])
      else:
        print("Setting CanBus %d Speed to %d" % (bus, can_speeds[bus]))
        panda_snd.set_can_baud(bus, can_speeds[bus])
        panda_rcv.set_can_baud(bus, can_speeds[bus])

      # send the characters
      # pick addresses high enough to not conflict with honda code
      at = random.randint(1024, 2000)
      st = get_test_string()[0:8]
      panda_snd.can_send(at, st, bus)
      time.sleep(0.1)

      # check for receive
      cans_echo = panda_snd.can_recv()
      cans_loop = panda_rcv.can_recv()

      print("Bus", bus, "echo", cans_echo, "loop", cans_loop)

      assert len(cans_echo) == 1
      assert len(cans_loop) == 1

      assert cans_echo[0][0] == at
      assert cans_loop[0][0] == at

      assert cans_echo[0][2] == st
      assert cans_loop[0][2] == st

      assert cans_echo[0][3] == 0x80 | bus
      if cans_loop[0][3] != bus:
        print("EXPECTED %d GOT %d" % (bus, cans_loop[0][3]))
      assert cans_loop[0][3] == bus
      time.sleep(sleep_duration)

      print("CAN pass", bus, ho)

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("-n", type=int, help="Number of test iterations to run")
  parser.add_argument("-can1baud", type=int, help="Baud Rate of CAN1", default=500000)
  parser.add_argument("-can2baud", type=int, help="Baud Rate of CAN2", default=500000)
  parser.add_argument("-can3baud", type=int, help="Baud Rate of CAN3", default=500000)
  parser.add_argument("-gmlan2baud", type=int, help="Baud Rate of GMLAN2", default=33333)
  parser.add_argument("-gmlan3baud", type=int, help="Baud Rate of GMLAN3", default=33333)
  parser.add_argument("-sleep", type=int, help="Sleep time between tests", default=0)
  args = parser.parse_args()

  can_speeds = (args.can1baud, args.can2baud, args.can3baud)
  gmlan_speeds = (None, args.gmlan2baud, args.gmlan2baud)

  if args.n is None:
    while True:
      run_test(can_speeds, gmlan_speeds, sleep_duration=args.sleep)
  else:
    for i in range(args.n):
      run_test(can_speeds, gmlan_speeds, sleep_duration=args.sleep)
