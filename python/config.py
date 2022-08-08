import os

BASEDIR = os.path.join(os.path.dirname(os.path.realpath(__file__)), "../")

BOOTSTUB_ADDRESS = 0x8000000

BLOCK_SIZE_FX = 0x800
APP_ADDRESS_FX = 0x8004000
SECTOR_SIZES_FX = [0x4000 for _ in range(4)] + [0x10000] + [0x20000 for _ in range(11)]
DEVICE_SERIAL_NUMBER_ADDR_FX = 0x1FFF79C0
DEFAULT_FW_FN = os.path.join(BASEDIR, "board", "obj", "panda.bin.signed")
DEFAULT_SSPOOF_FW_FN = os.path.join(BASEDIR, "board", "obj", "panda.bin.sspoof.signed")
TESTING_FW_FN = os.path.join(BASEDIR, "board", "obj", "panda.bin.testing.signed")
TESTING_SSPOOF_FW_FN = os.path.join(BASEDIR, "board", "obj", "panda.bin.testing.sspoof.signed")

atl_enabled = False
if os.path.exists('/data/params/d/dp_atl'):
  with open('/data/params/d/dp_atl') as f:
    if (int(f.read().strip())) != 0:
      atl_enabled = True

sspoof_enabled = False
if os.path.exists('/data/params/d/dp_sspoof'):
  with open('/data/params/d/dp_sspoof') as f:
    if (int(f.read().strip())) != 0:
      sspoof_enabled = True

if atl_enabled and sspoof_enabled and os.path.exists(TESTING_SSPOOF_FW_FN):
  DEFAULT_FW_FN = TESTING_SSPOOF_FW_FN
elif atl_enabled and not sspoof_enabled and os.path.exists(TESTING_FW_FN):
  DEFAULT_FW_FN = TESTING_FW_FN
elif not atl_enabled and sspoof_enabled and os.path.exists(DEFAULT_SSPOOF_FW_FN):
  DEFAULT_FW_FN = DEFAULT_SSPOOF_FW_FN

DEFAULT_BOOTSTUB_FN = os.path.join(BASEDIR, "board", "obj", "bootstub.panda.bin")

BLOCK_SIZE_H7 = 0x400
APP_ADDRESS_H7 = 0x8020000
SECTOR_SIZES_H7 = [0x20000 for _ in range(7)] # there is an 8th sector, but we use that for the provisioning chunk, so don't program over that!
DEVICE_SERIAL_NUMBER_ADDR_H7 = 0x080FFFC0
DEFAULT_H7_FW_FN = os.path.join(BASEDIR, "board", "obj", "panda_h7.bin.signed")
DEFAULT_H7_BOOTSTUB_FN = os.path.join(BASEDIR, "board", "obj", "bootstub.panda_h7.bin")
