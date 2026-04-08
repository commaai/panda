#!/usr/bin/env python3

import argparse
import os
import signal
import struct
import sys
import threading
import time
from itertools import accumulate

from opendbc.car import structs
from opendbc.car.isotp import isotp_recv, isotp_send
from panda import Panda

BODY_V1_DIR = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(BODY_V1_DIR, "..")))

from flash_helpers import BODY_V1_F4_FIRMWARE, build_body_artifact, ensure_firmware_file, resolve_firmware_path

BODY_V1_FLASH_MAGIC = b"\xce\xfa\xad\xde\x1e\x0b\xb0\x0a"
BODY_V1_FLASH_ADDR = 0x250
BODY_V1_SECTOR_SIZES = [0x4000 for _ in range(4)] + [0x10000] + [0x20000 for _ in range(11)]
BODY_V1_FLASH_STEP = 0x10


class CanHandle:
  def __init__(self, panda, bus):
    self.panda = panda
    self.bus = bus

  def transact(self, payload):
    def _handle_timeout(signum, frame):
      raise TimeoutError

    signal.signal(signal.SIGALRM, _handle_timeout)
    signal.alarm(1)
    try:
      isotp_send(self.panda, payload, 1, self.bus, recvaddr=2)
    finally:
      signal.alarm(0)

    signal.signal(signal.SIGALRM, _handle_timeout)
    signal.alarm(1)
    try:
      return isotp_recv(self.panda, 2, self.bus, sendaddr=1)
    finally:
      signal.alarm(0)

  def control_write(self, request_type, request, value, index, length):
    payload = struct.pack("HHBBHHH", 0, 0, request_type, request, value, index, length)
    return self.transact(payload)

  def control_read(self, request_type, request, value, index, length):
    payload = struct.pack("HHBBHHH", 0, 0, request_type, request, value, index, length)
    return self.transact(payload)

  def bulk_write(self, endpoint, data):
    payload = struct.pack("HH", endpoint, len(data)) + data
    return self.transact(payload)

def flasher_present(handle):
  response = handle.control_read(Panda.REQUEST_IN, 0xB0, 0, 0, 0xC)
  return response[4:8] == b"\xde\xad\xd0\x0d"


def flash_static_f4(handle, code):
  assert flasher_present(handle)

  apps_sectors_cumsum = accumulate(BODY_V1_SECTOR_SIZES[1:])
  last_sector = next((i + 1 for i, value in enumerate(apps_sectors_cumsum) if value > len(code)), -1)
  assert last_sector >= 1, "Binary too small? No sector to erase."
  assert last_sector < 12, "Binary too large for STM32F4 flash layout."

  handle.control_write(Panda.REQUEST_IN, 0xB1, 0, 0, 0)
  for sector in range(1, last_sector + 1):
    handle.control_write(Panda.REQUEST_IN, 0xB2, sector, 0, 0)

  for offset in range(0, len(code), BODY_V1_FLASH_STEP):
    handle.bulk_write(2, code[offset:offset + BODY_V1_FLASH_STEP])

  try:
    handle.control_write(Panda.REQUEST_IN, 0xD8, 0, 0, 0)
  except Exception:
    pass


def heartbeat_thread(panda):
  while True:
    try:
      panda.send_heartbeat()
      time.sleep(0.5)
    except Exception:
      continue


def flush_panda(panda):
  while len(panda.can_recv()) != 0:
    pass


def flash_body_v1(panda, firmware_path, addr):
  panda.can_send(addr, BODY_V1_FLASH_MAGIC, 0)
  time.sleep(0.1)
  flush_panda(panda)

  with open(firmware_path, "rb") as f:
    code = f.read()

  handle = CanHandle(panda, 0)
  retries = 3
  for attempt in range(1, retries + 1):
    try:
      flash_static_f4(handle, code)
      return
    except TimeoutError:
      if attempt == retries:
        raise


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Flash Body V1 over CAN")
  parser.add_argument("firmware", nargs="?", help="Optional path to firmware binary to flash")
  parser.add_argument("--addr", type=lambda value: int(value, 0), default=BODY_V1_FLASH_ADDR, help="CAN address to use for the flasher trigger")
  args = parser.parse_args()

  firmware_path = resolve_firmware_path(args.firmware, BODY_V1_F4_FIRMWARE)
  build_body_artifact("board/obj/body_v1_f4.bin.signed")
  ensure_firmware_file(parser, firmware_path)

  with Panda() as panda:
    threading.Thread(target=heartbeat_thread, args=(panda,), daemon=True).start()
    panda.set_safety_mode(structs.CarParams.SafetyModel.body)
    flash_body_v1(panda, firmware_path, args.addr)
