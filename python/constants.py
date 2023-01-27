import os
import copy
from dataclasses import dataclass
from typing import List

BASEDIR = os.path.join(os.path.dirname(os.path.realpath(__file__)), "../")


@dataclass
class McuConfig:
  block_size: int
  sector_sizes: List[int]
  serial_number_address: int
  app_address: int
  app_path: str
  bootstub_address: int
  bootstub_path: str

FxConfig = McuConfig(
  0x800,
  [0x4000 for _ in range(4)] + [0x10000] + [0x20000 for _ in range(11)],
  0x1FFF79C0,
  0x8004000,
  os.path.join(BASEDIR, "board", "obj", "panda.bin.signed"),
  0x8000000,
  os.path.join(BASEDIR, "board", "obj", "bootstub.panda.bin"),
)

H7Config = McuConfig(
  0x400,
  # there is an 8th sector, but we use that for the provisioning chunk, so don't program over that!
  [0x20000 for _ in range(7)],
  0x080FFFC0,
  0x8020000,
  os.path.join(BASEDIR, "board", "obj", "panda_h7.bin.signed"),
  0x8000000,
  os.path.join(BASEDIR, "board", "obj", "bootstub.panda_h7.bin"),
)

class McuType:
  F2 = copy.deepcopy(FxConfig)
  F4 = copy.deepcopy(FxConfig)
  H7 = H7Config
