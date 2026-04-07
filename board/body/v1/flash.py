#!/usr/bin/env python3

import sys
import _thread
import time
import argparse
from subprocess import check_output, CalledProcessError

from panda import Panda
from opendbc.car import structs
from opendbc.car.can_definitions import CanData
from opendbc.car.body.flash import update
from opendbc.car.body.values import FLASH_ADDR, BUS, BIN_URL, BIN_PATH

def heartbeat_thread(p):
  while True:
    try:
      p.send_heartbeat()
      time.sleep(0.5)
    except Exception:
      continue


def flash(addr=FLASH_ADDR, file=BIN_PATH):
  p = Panda()
  _thread.start_new_thread(heartbeat_thread, (p,))

  p.set_safety_mode(structs.CarParams.SafetyModel.body)

  update(
    can_send=p.can_send,
    can_recv=p.can_recv,
    addr=addr,
    bus=BUS,
    file=file,
    update_url=BIN_URL,
  )


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Flash body v2 over CAN")
  parser.add_argument("fn", type=str, nargs="?", help="flash file")
  parser.add_argument("--kill-pandad", action="store_true", help="kill pandad processes before flashing")
  args = parser.parse_args()

  if args.kill_pandad:
    try:
      check_output(["pkill", "-f", "selfdrive.pandad.pandad"])
      check_output(["pkill", "-x", "pandad"])
      print("pandad processes killed")
      time.sleep(1)
    except CalledProcessError:
      print("no pandad processes found")
  else:
    try:
      check_output(["pidof", "pandad"])
      print("pandad is running, please kill it before running this script! (use --kill-pandad to kill automatically)")
      sys.exit(1)
    except CalledProcessError as e:
      if e.returncode != 1:  # 1 == no process found (pandad not running)
        raise e

  if not args.fn:
    args.fn = BIN_PATH

  flash(FLASH_ADDR, args.fn)
