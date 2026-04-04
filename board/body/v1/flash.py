#!/usr/bin/env python3

import os
import time
import argparse
import _thread
import subprocess
from panda import Panda, McuType
from panda.python.can import CanHandle
from opendbc.car import structs
from opendbc.car.uds import UdsClient, DATA_IDENTIFIER_TYPE, MessageTimeoutError
from openpilot.common.params import Params

FIRMWARE_VERSION = "v0.3.1"
BIN_URL = f"https://github.com/commaai/body/releases/download/{FIRMWARE_VERSION}/body.bin.signed"
BIN_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), f"body-{FIRMWARE_VERSION}.bin.signed")

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

def get_body_firmware_signature(p, bus=0):
  """Read the 128-byte RSA firmware signature from a body board via UDS ReadDataByIdentifier (DID F184)"""
  tx_addr, rx_addr = (0x720, 0x728) if bus == 0 else (0x730, 0x738)
  uds_client = UdsClient(p, tx_addr, rx_addr, bus=bus, timeout=2)
  try:
    return uds_client.read_data_by_identifier(DATA_IDENTIFIER_TYPE.APPLICATION_SOFTWARE_FINGERPRINT)
  except MessageTimeoutError:
    return b""

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

def update(addr=0x250, file=BIN_PATH, skip_check=False):
  params = Params()
  p = Panda()
  _thread.start_new_thread(heartbeat_thread, (p,))
  params.put_bool("BodyFirmwareFlashing", True)

  if not os.path.exists(file):
    print("local bin is not up-to-date, fetching latest")
    fetch_bin()
    file = BIN_PATH # revert to default version

  if not skip_check:
    p.set_safety_mode(structs.CarParams.SafetyModel.elm327) # needed for UDS
    print("checking body firmware signature")
    current_signature = get_body_firmware_signature(p)
    with open(file, "rb") as f:
      expected_signature = f.read()[-128:]
    print(f"expected body signature: {expected_signature.hex()}")
    print(f"current body signature: {current_signature.hex()}")

  if skip_check or current_signature != expected_signature:
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

  if not args.fn:
    args.fn = BIN_URL

  update(0x250, args.fn, args.skip)
