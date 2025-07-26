#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import argparse

# Add the parent directory to the path so we can import panda
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from panda import Panda, PandaDFU, BASEDIR


def flash():
    """Flash Panda device(s) with the latest firmware."""
    parser = argparse.ArgumentParser(description='Flash Panda device(s)')
    parser.add_argument("--all", action="store_true", help="Flash all connected Panda devices")
    args = parser.parse_args()

    # Build the firmware
    board_path = os.path.join(BASEDIR, "board")
    print("Building firmware...")
    try:
        subprocess.check_call(f"scons -C {BASEDIR} -j$(nproc) {board_path}", shell=True)
    except subprocess.CalledProcessError:
        print("Failed to build firmware. Make sure you have all build dependencies installed.")
        return 1

    # Get list of devices to flash
    if args.all:
        serials = Panda.list()
        print(f"Found {len(serials)} panda(s) - {serials}")
    else:
        serials = [None]

    # Flash each device
    for s in serials:
        try:
            with Panda(serial=s) as p:
                print(f"Flashing {p.get_usb_serial()}")
                p.flash()
        except Exception as e:
            print(f"Failed to flash device: {e}")
            return 1

    return 0 if len(serials) > 0 else 1


def recover():
    """Recover Panda device(s) by putting them in DFU mode and reflashing."""
    # Build the firmware
    board_path = os.path.join(BASEDIR, "board")
    print("Building firmware...")
    try:
        subprocess.check_call(f"scons -C {BASEDIR} -j$(nproc) {board_path}", shell=True)
    except subprocess.CalledProcessError:
        print("Failed to build firmware. Make sure you have all build dependencies installed.")
        return 1

    # Put all connected pandas in DFU mode
    pandas = Panda.list()
    for s in pandas:
        try:
            with Panda(serial=s) as p:
                print(f"Putting {p.get_usb_serial()} in DFU mode")
                p.reset(enter_bootstub=True)
                p.reset(enter_bootloader=True)
        except Exception as e:
            print(f"Failed to put device in DFU mode: {e}")

    # Wait for devices to come back up in DFU mode
    time.sleep(1)

    # Flash all devices in DFU mode
    dfu_serials = PandaDFU.list()
    print(f"Found {len(dfu_serials)} panda(s) in DFU - {dfu_serials}")
    
    for s in dfu_serials:
        try:
            print(f"Flashing {s}")
            PandaDFU(s).recover()
        except Exception as e:
            print(f"Failed to recover device: {e}")
            return 1

    return 0 if len(dfu_serials) > 0 else 1


if __name__ == "__main__":
    # For testing purposes
    import sys
    if len(sys.argv) > 1 and sys.argv[1] == "flash":
        sys.exit(flash())
    elif len(sys.argv) > 1 and sys.argv[1] == "recover":
        sys.exit(recover())
    else:
        print("Usage: python cli.py [flash|recover]")
        sys.exit(1)