#!/usr/bin/env python3

import _thread
import time
import argparse

from panda import Panda
from opendbc.car import structs
from opendbc.car.body.flash import update
from opendbc.car.body.values import FLASH_ADDR, BUS, UDS_TX, UDS_RX, BIN_URL, BIN_PATH
from openpilot.common.params import Params


def heartbeat_thread(p):
  while True:
    try:
      p.send_heartbeat()
      time.sleep(0.5)
    except Exception:
      continue


def flash(addr=FLASH_ADDR, file=BIN_PATH, skip_check=False):
  params = Params()
  p = Panda()
  _thread.start_new_thread(heartbeat_thread, (p,))

  params.put_bool("BodyFirmwareFlashing", True)
  try:
    p.set_safety_mode(structs.CarParams.SafetyModel.body)

    uds_tx = None if skip_check else UDS_TX
    uds_rx = None if skip_check else UDS_RX
    uds_bus = None if skip_check else BUS

    update(
      bootloader_addr=addr,
      bootloader_bus=BUS,
      file=file,
      update_url=BIN_URL,
      can_send=p.can_send_many,
      can_recv=p.can_recv,
      uds_tx=uds_tx,
      uds_rx=uds_rx,
      uds_bus=uds_bus,
    )
  finally:
    params.put_bool("BodyFirmwareFlashing", False)


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Flash body v2 over CAN")
  parser.add_argument("fn", type=str, nargs="?", help="flash file")
  parser.add_argument("--skip", action="store_true", help="skip firmware version check and force flash")
  args = parser.parse_args()

  if not args.fn:
    args.fn = BIN_PATH

  flash(FLASH_ADDR, args.fn, args.skip)
