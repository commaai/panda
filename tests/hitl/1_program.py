import os
import time

from panda import Panda, PandaDFU, McuType, BASEDIR
from .helpers import test_all_pandas, panda_connect_and_init, check_signature


@test_all_pandas
@panda_connect_and_init
def test_a_known_bootstub(p):
  """
  Test that compiled app can work with known production bootstub
  """
  known_bootstubs = {
    McuType.F4: "bootstub.panda.bin",
    McuType.H7: "bootstub.panda_h7.bin",
  }

  p.reset(enter_bootstub=True)
  p.reset(enter_bootloader=True)

  dfu_serial = PandaDFU.st_serial_to_dfu_serial(p._serial, p._mcu_type)
  assert Panda.wait_for_dfu(dfu_serial, timeout=30)

  dfu = PandaDFU(dfu_serial)
  fn = known_bootstubs[p._mcu_type]
  with open(os.path.join(BASEDIR, "tests/hitl/known_bootstub", fn), "rb") as f:
    code = f.read()

  dfu.program_bootstub(code)
  p.connect(True, True)
  p.flash()
  check_signature(p)

@test_all_pandas
@panda_connect_and_init
def test_b_recover(p):
  assert p.recover(timeout=30)
  check_signature(p)

@test_all_pandas
@panda_connect_and_init
def test_c_flash(p):
  # test flash from bootstub
  serial = p._serial
  assert serial is not None
  p.reset(enter_bootstub=True)
  p.close()
  time.sleep(2)

  with Panda(serial) as np:
    assert np.bootstub
    assert np._serial == serial
    np.flash()

  p.reconnect()
  p.reset()
  check_signature(p)

  # test flash from app
  p.flash()
  check_signature(p)
