#!/usr/bin/env python2

import sys
import struct
import panda.tests.safety.libpandasafety_py as libpandasafety_py
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
  "TOYOTA_NOLIMITS": 0x1336,
  "ALLOUTPUT": 0x1337,
  "ELM327": 0xE327
}


# replay a drive to check for safety violations
def replay_drive(lr, safety_mode, param):
  safety = libpandasafety_py.libpandasafety

  err = safety.set_mode(safety_mode, param)
  assert err == 0, "invalid safety mode: %d" % safety_mode

  tx_tot, tx_blocked, tx_controls, tx_controls_blocked = 0, 0, 0, 0
  start_t = None

  for msg in lr:
    if start_t is None:
      start_t = msg.logMonoTime
    t = (msg.logMonoTime - start_t) & 0xFFFFFFFF

    if msg.which() == 'sendcan':
      for canmsg in msg.sendcan:
        to_send = libpandasafety_py.ffi.new('CAN_FIFOMailBox_TypeDef *')
        to_send[0].RIR = canmsg.address << 21
        to_send[0].RDTR = (canmsg.src & 0xF) << 4
        to_send[0].RDHR = struct.unpack('<I', canmsg.dat.ljust(8, '\x00')[4:])[0]
        to_send[0].RDLR = struct.unpack('<I', canmsg.dat.ljust(8, '\x00')[:4])[0]

        # nanoseconds to microseconds
        safety.set_timer(int(t/1000.))

        blocked = not safety.tx(to_send)
        tx_blocked += blocked
        tx_controls += safety.get_controls_allowed()
        tx_controls_blocked += blocked and safety.get_controls_allowed()
        tx_tot += 1
    elif msg.which() == 'can':
      for canmsg in msg.can:
        to_push = libpandasafety_py.ffi.new('CAN_FIFOMailBox_TypeDef *')
        to_push[0].RIR = canmsg.address << 21
        to_push[0].RDTR = (canmsg.src & 0xF) << 4
        to_push[0].RDHR = struct.unpack('<I', canmsg.dat.ljust(8, '\x00')[4:])[0]
        to_push[0].RDLR = struct.unpack('<I', canmsg.dat.ljust(8, '\x00')[:4])[0]
        safety.rx(to_push)

  print "total openpilot msgs:", tx_tot
  print "total msgs with controls allowed:", tx_controls
  print "blocked msgs:", tx_blocked
  print "blocked with controls allowed:", tx_controls_blocked

  return tx_controls_blocked == 0


if __name__ == "__main__":
  if sys.argv[2] in safety_modes:
    mode = safety_modes[sys.argv[2]]
  else:
    mode = int(sys.argv[2])
  param = 0 if len(sys.argv) < 4 else int(sys.argv[3])
  lr = LogReader(sys.argv[1])

  print "replaying drive %s with safety model %d and param %d" % (sys.argv[1], mode, param)

  replay_drive(lr, mode, param)

