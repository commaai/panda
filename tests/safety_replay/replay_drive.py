#!/usr/bin/env python2

import os
import sys
import panda.tests.safety.libpandasafety_py as libpandasafety_py
from panda.tests.safety_replay.replay_helpers import is_steering_msg, get_torque, \
                                                    set_desired_torque_last, package_can_msg

from openpilot_tools.lib.logreader import LogReader

safety_modes = {
  "NOOUTPUT": 0,
  "HONDA": 1,
  "TOYOTA": 2,
  "GM": 3,
  "HONDA_BOSCH": 4,
  "FORD": 5,
  "CADILLAC": 6,
  "HYUNDAI": 7,
  "TESLA": 8,
  "CHRYSLER": 9,
  "SUBARU": 10,
  "GM_ASCM": 0x1334,
  "TOYOTA_IPAS": 0x1335,
  "ALLOUTPUT": 0x1337,
  "ELM327": 0xE327
}


# replay a drive to check for safety violations
def replay_drive(lr, safety_mode, param):
  safety = libpandasafety_py.libpandasafety

  err = safety.safety_set_mode(safety_mode, param)
  assert err == 0, "invalid safety mode: %d" % safety_mode

  if "SEGMENT" in os.environ:
    print "ignoring start"
    cnt = 0
    for msg in lr:
      if msg.which() != 'sendcan':
        continue
      for canmsg in msg.sendcan:
        if is_steering_msg(mode, canmsg.address):
          to_send = package_can_msg(canmsg)
          torque = get_steer_torque(mode, to_send)
          if torque != 0:
            safety.set_controls_allowed(1)
            set_desired_torque_last(safety, mode, torque)
      break

  tx_tot, tx_blocked, tx_controls, tx_controls_blocked = 0, 0, 0, 0
  blocked_addrs = set()

  for msg in lr:
    safety.set_timer(((msg.logMonoTime / 1000))  % 0xFFFFFFFF)

    if msg.which() == 'sendcan':
     for canmsg in msg.sendcan:
        to_send = package_can_msg(canmsg)
        sent = safety.safety_tx_hook(to_send)
        if not sent:
          tx_blocked += 1
          tx_controls_blocked += safety.get_controls_allowed()
          blocked_addrs.add(canmsg.address)
        tx_controls += safety.get_controls_allowed()
        tx_tot += 1
    elif msg.which() == 'can':
     for canmsg in msg.can:
        # ignore msgs we sent
        if canmsg.src >= 128:
          continue
        to_push = package_can_msg(canmsg)
        safety.safety_rx_hook(to_push)

  print "total openpilot msgs:", tx_tot
  print "total msgs with controls allowed:", tx_controls
  print "blocked msgs:", tx_blocked
  print "blocked with controls allowed:", tx_controls_blocked
  print "blocked addrs:", blocked_addrs

  return tx_controls_blocked == 0

if __name__ == "__main__":
  if sys.argv[2] in safety_modes:
    mode = safety_modes[sys.argv[2]]
  else:
    mode = int(sys.argv[2])
  param = 0 if len(sys.argv) < 4 else int(sys.argv[3])
  lr = LogReader(sys.argv[1])

  print "replaying drive %s with safety mode %d and param %d" % (sys.argv[1], mode, param)

  replay_drive(lr, mode, param)

