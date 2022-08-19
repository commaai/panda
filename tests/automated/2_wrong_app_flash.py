import os
import time

from panda import Panda, BASEDIR, DEFAULT_FW_FN, DEFAULT_H7_FW_FN, MCU_TYPE_H7, MCU_TYPE_F4, MCU_TYPE_F2
from .helpers import test_red, test_black, panda_connect_and_init, panda_type_to_serial, check_signature

PEDAL_USB_FN = os.path.join(BASEDIR, "board", "obj", "pedal_usb.bin.signed")

def wrong_flash_check(p, fn, mcu_type):
  serial = p._serial
  assert serial != None
  p.reset(enter_bootstub=True)
  p.close()
  time.sleep(2)

  np = Panda(serial)
  assert np.bootstub
  assert np._serial == serial
  assert os.path.isfile(fn)
  with open(fn, "rb") as f:
    code = f.read()
  Panda.flash_static(np._handle, code, mcu_type)
  np.close()

  p.reconnect()
  # Check that flashing failed and we are still in bootstub
  assert p.bootstub
  p.flash()
  p.reset()
  check_signature(p)

# Flash H7 panda with F4 app and vise versa. Check if recovery is possible.
@test_red
@panda_type_to_serial
@panda_connect_and_init(full_reset=False)
def test_red_wrong_app(p):
  wrong_flash_check(p, DEFAULT_FW_FN, MCU_TYPE_F4)
  wrong_flash_check(p, PEDAL_USB_FN, MCU_TYPE_F2)

@test_black
@panda_type_to_serial
@panda_connect_and_init(full_reset=False)
def test_black_wrong_app(p):
  wrong_flash_check(p, DEFAULT_H7_FW_FN, MCU_TYPE_H7)
  wrong_flash_check(p, PEDAL_USB_FN, MCU_TYPE_F2)
