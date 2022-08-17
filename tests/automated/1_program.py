import os
import time

from panda import Panda, PandaDFU, MCU_TYPE_H7, BASEDIR
from .helpers import test_all_pandas, panda_connect_and_init, check_signature


@test_all_pandas
@panda_connect_and_init(full_reset=False)
def test_a_known_bootstub(p):
  # Test that compiled app can work with known production bootstub
  KNOWN_H7_BOOTSTUB_FN = os.path.join(BASEDIR, "tests", "automated", "known_bootstub", "bootstub.panda_h7.bin")
  KNOWN_BOOTSTUB_FN = os.path.join(BASEDIR, "tests", "automated", "known_bootstub", "bootstub.panda.bin")

  p.reset(enter_bootstub=True)
  p.reset(enter_bootloader=True)

  dfu_serial = PandaDFU.st_serial_to_dfu_serial(p._serial, p._mcu_type)
  assert Panda.wait_for_dfu(dfu_serial, timeout=30)

  dfu = PandaDFU(dfu_serial)
  fn = KNOWN_H7_BOOTSTUB_FN if p._mcu_type == MCU_TYPE_H7 else KNOWN_BOOTSTUB_FN
  with open(fn, "rb") as f:
    code = f.read()

  dfu.program_bootstub(code)
  p.connect(True, True)
  p.flash()
  check_signature(p)

@test_all_pandas
@panda_connect_and_init(full_reset=False)
def test_b_recover(p):
  assert p.recover(timeout=30)
  check_signature(p)

@test_all_pandas
@panda_connect_and_init(full_reset=False)
def test_c_flash(p):
  # test flash from bootstub
  serial = p._serial
  assert serial != None
  p.reset(enter_bootstub=True)
  p.close()
  time.sleep(2)

  np = Panda(serial)
  assert np.bootstub
  assert np._serial == serial
  np.flash()
  np.close()

  p.reconnect()
  p.reset()
  check_signature(p)

  # test flash from app
  p.flash()
  check_signature(p)
