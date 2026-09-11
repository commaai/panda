#!/usr/bin/env python3
import os
import subprocess
import argparse

from panda import PandaJungle

board_path = os.path.dirname(os.path.realpath(__file__))

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("--all", action="store_true", help="Recover all panda jungle devices")
  args = parser.parse_args()

  subprocess.check_call(["make", "-C", os.path.join(board_path, "../.."), f"-j{os.cpu_count() or 1}", "panda_jungle_h7"])

  if args.all:
    serials = PandaJungle.list()
    print(f"found {len(serials)} panda jungles(s) - {serials}")
  else:
    serials = [None]

  for s in serials:
    with PandaJungle(serial=s) as p:
      print("flashing", p.get_usb_serial())
      p.flash()
  exit(1 if len(serials) == 0 else 0)
