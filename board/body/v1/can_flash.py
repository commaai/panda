#!/usr/bin/env python3


# connects to PANDA on BOARD the comma and then sends CAN commands through that COMMA to flash the STM32F4 on the body v1 board

import os
import time
import argparse
import _thread
from panda import Panda, McuType  # pylint: disable=import-error
from panda.python.can import CanHandle  # pylint: disable=import-error
from opendbc.car import structs
from openpilot.common.params import Params


def heartbeat_thread(p):
  while True:
    try:
      p.send_heartbeat()
      time.sleep(0.5)
    except Exception:
      continue

def flush_panda(p):
  while(1):
    if len(p.can_recv()) == 0:
      break

def flasher(p, addr, file):
  p.can_send(addr, b"\xce\xfa\xad\xde\x1e\x0b\xb0\x0a", 0)
  time.sleep(0.1)
  print("flashing", file)
  flush_panda(p)
  code = open(file, "rb").read()
  retries = 3
  for i in range(retries):
    try:
      Panda.flash_static(CanHandle(p, 0), code, McuType.F4)
    except (TimeoutError):
      print(f"Flash failed (attempt {i + 1}/{retries}), trying again...")
    else:
      print("Successfully flashed")
      return
  raise RuntimeError(f"Flash failed after {retries} attempts")


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='Flash body over can')
  parser.add_argument("board", type=str, nargs='?', help="choose base or knee")
  parser.add_argument("fn", type=str, nargs='?', help="flash file")
  args = parser.parse_args()

  assert args.board in ["base", "knee"]
  assert os.path.isfile(args.fn)

  addr = 0x250 if args.board == "base" else 0x350

  params = Params()
  p = Panda()
  _thread.start_new_thread(heartbeat_thread, (p,))
  p.set_safety_mode(structs.CarParams.SafetyModel.body)

  params.put_bool("BodyFirmwareFlashing", True)
  try:
    print("Flashing motherboard")
    flasher(p, addr, args.fn)
  finally:
    params.put_bool("BodyFirmwareFlashing", False)