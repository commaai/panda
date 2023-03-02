import os
import time

from panda import Panda, PandaDFU, McuType, BASEDIR
from .helpers import test_all_pandas, panda_connect_and_init, check_signature


# TODO: make more comprehensive bootstub tests and run on a few production ones + current
# TODO: also test release-signed app
@test_all_pandas
@panda_connect_and_init
def test_a_known_bootstub(p):
  """
  Test that compiled app can work with known production bootstub
  """
  known_bootstubs = {
    # covers the two cases listed in Panda.connect
    McuType.F4: [
      # case A - no bcdDevice or panda type, has to assume F4
      "bootstub_f4_first_dos_production.panda.bin",

      # case B - just bcdDevice
      "bootstub_f4_only_bcd.panda.bin",
    ],
    McuType.H7: ["bootstub.panda_h7.bin"],
  }

  for kb in known_bootstubs[p.get_mcu_type()]:
    app_ids = (p.get_mcu_type(), p.get_usb_serial())
    assert None not in app_ids

    p.reset(enter_bootstub=True)
    p.reset(enter_bootloader=True)

    dfu_serial = PandaDFU.st_serial_to_dfu_serial(p._serial, p._mcu_type)
    assert Panda.wait_for_dfu(dfu_serial, timeout=30)

    dfu = PandaDFU(dfu_serial)
    with open(os.path.join(BASEDIR, "tests/hitl/known_bootstub", kb), "rb") as f:
      code = f.read()

    dfu.program_bootstub(code)

    p.connect(claim=False, wait=True)

    # check for MCU or serial mismatch
    with Panda(p._serial, claim=False) as np:
      bootstub_ids = (np.get_mcu_type(), np.get_usb_serial())
      assert app_ids == bootstub_ids

    # ensure we can flash app and it jumps to app
    p.flash()
    check_signature(p)
    assert not p.bootstub

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
