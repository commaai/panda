import os
import subprocess
from typing import Optional

BODY_DIR = os.path.dirname(os.path.realpath(__file__))
BOARD_DIR = os.path.abspath(os.path.join(BODY_DIR, ".."))
REPO_ROOT = os.path.abspath(os.path.join(BOARD_DIR, ".."))
BODY_V1_DIR = os.path.join(BODY_DIR, "v1")

BODY_H7_FIRMWARE = os.path.join(BOARD_DIR, "obj", "body_h7.bin.signed")
BODY_V1_F4_FIRMWARE = os.path.join(BOARD_DIR, "obj", "body_v1_f4.bin.signed")
BODY_V1_FLASH_SCRIPT = os.path.join(BODY_V1_DIR, "can_flash.py")


def build_body_artifact(target: str) -> None:
  subprocess.check_call(
    f"scons -C {REPO_ROOT} -j$(nproc) {target}",
    shell=True,
  )


def resolve_firmware_path(firmware: Optional[str], default_firmware: str) -> str:
  return os.path.abspath(firmware) if firmware is not None else default_firmware


def ensure_firmware_file(parser, firmware_path: str) -> None:
  if not os.path.isfile(firmware_path):
    parser.error(f"firmware file not found: {firmware_path}")
