#!/usr/bin/env python3
import argparse
import subprocess
import sys

from panda import Panda
from flash_helpers import BODY_H7_FIRMWARE, BODY_V1_FLASH_SCRIPT, build_body_artifact, ensure_firmware_file, resolve_firmware_path


if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("firmware", nargs="?", help="Optional path to firmware binary to flash")
  parser.add_argument("--all", action="store_true", help="Flash all Panda devices")
  parser.add_argument("--v1-can", action="store_true", help="Flash Body V1 over CAN using an attached panda")
  parser.add_argument(
    "--wait-usb",
    action="store_true",
    help="Wait for the panda to reconnect over USB after flashing (defaults to skipping reconnect).",
  )
  args = parser.parse_args()

  if args.v1_can:
    command = [sys.executable, BODY_V1_FLASH_SCRIPT]
    if args.firmware is not None:
      command.append(args.firmware)
    raise SystemExit(subprocess.call(command))

  firmware_path = resolve_firmware_path(args.firmware, BODY_H7_FIRMWARE)
  build_body_artifact("board/obj/body_h7.bin.signed")
  ensure_firmware_file(parser, firmware_path)

  if args.all:
    serials = Panda.list()
    print(f"found {len(serials)} panda(s) - {serials}")
  else:
    serials = [None]

  for s in serials:
    with Panda(serial=s) as p:
      print("flashing", p.get_usb_serial())
      p.flash(firmware_path, reconnect=args.wait_usb)
  exit(1 if len(serials) == 0 else 0)
