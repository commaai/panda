#!/usr/bin/env python3

# connects to PANDA on BOARD the comma and then sends CAN commands through that COMMA to flash the STM32F4 on the body v1 board

import os
import time
import argparse
import _thread
import subprocess
from panda import Panda, McuType  # pylint: disable=import-error
from panda.python.can import CanHandle  # pylint: disable=import-error
from opendbc.car import structs
from opendbc.car.uds import UdsClient, DATA_IDENTIFIER_TYPE
from openpilot.common.params import Params

FIRMWARE_COMMIT = "c433da94"
BIN_URL = f"https://github.com/commaai/body/releases/download/{FIRMWARE_COMMIT}/body.bin.signed"

BIN_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "body.bin.signed")

def fetch_bin():
  result = subprocess.run(
    ["curl", "-L", "-o", BIN_PATH, "-w", "%{http_code}", "--silent", BIN_URL],
    capture_output=True, text=True
  )
  status = result.stdout.strip()
  if status == "200":
    print("downloaded latest body firmware binary")
  else:
    raise RuntimeError(f"download failed with HTTP {status}")

def get_body_firmware_version(p):
  """Read the git commit hash from a body board via UDS ReadDataByIdentifier"""
  uds_client = UdsClient(p, 0x721, 0x729, bus=0, timeout=1)
  return uds_client.read_data_by_identifier(DATA_IDENTIFIER_TYPE.APPLICATION_SOFTWARE_IDENTIFICATION).decode("ascii")

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

def update(addr=0x250, file=BIN_PATH, skip_version_check=False):
  params = Params()
  p = Panda()
  _thread.start_new_thread(heartbeat_thread, (p,))

  params.put_bool("BodyFirmwareFlashing", True)

  if not skip_version_check:
    p.set_safety_mode(structs.CarParams.SafetyModel.elm327) # needed for UDS

    print("checking local bin firmware version")
    with open(file, "rb") as f:
      f.seek(0x1D8)
      try:
        expected_version = f.read(8).decode("ascii")
      except (UnicodeDecodeError, ValueError):
        expected_version = None
      f.close()

    if expected_version is None or expected_version != FIRMWARE_COMMIT:
      print("local bin is not up-to-date, fetching latest")
      fetch_bin()

    print("checking body firmware version")
    current_version = get_body_firmware_version(p)

    print(f"expected body version: {expected_version}, current body version: {current_version}")

  if skip_version_check or current_version != expected_version:
    p.set_safety_mode(structs.CarParams.SafetyModel.body) # needed for CAN Flash
    print("Flashing motherboard")
    flasher(p, addr, file)
  else:
    print("body firmware is up to date")

  params.put_bool("BodyFirmwareFlashing", False)

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='Flash body over can')
  parser.add_argument("fn", type=str, nargs='?', help="flash file")
  parser.add_argument("--skip", action="store_true", help="skip firmware version check and force flash")
  args = parser.parse_args()

  assert os.path.exists(args.fn), f"file not found: {args.fn}"

  update(0x250, args.fn, args.skip)
