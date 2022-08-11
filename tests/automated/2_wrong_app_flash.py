import time

from panda import Panda, DEFAULT_FW_FN, DEFAULT_H7_FW_FN, MCU_TYPE_H7, MCU_TYPE_F4
from .helpers import test_red, test_black, panda_connect_and_init, panda_type_to_serial, check_signature

# Flash H7 panda with F4 app and vise versa. Check if recovery is possible.
@test_red
@panda_type_to_serial
@panda_connect_and_init(full_reset=False)
def test_red_wrong_app(p):
  serial = p._serial
  assert serial != None
  p.reset(enter_bootstub=True)
  p.close()
  time.sleep(2)

  np = Panda(serial)
  assert np.bootstub
  assert np._serial == serial
  with open(DEFAULT_FW_FN, "rb") as f:
    code = f.read()
  Panda.flash_static(np._handle, code, MCU_TYPE_F4)
  np.close()

  p.reconnect()
  assert p.bootstub
  p.flash()
  p.reset()
  check_signature(p)

@test_black
@panda_type_to_serial
@panda_connect_and_init(full_reset=False)
def test_black_wrong_app(p):
  serial = p._serial
  assert serial != None
  p.reset(enter_bootstub=True)
  p.close()
  time.sleep(2)

  np = Panda(serial)
  assert np.bootstub
  assert np._serial == serial
  with open(DEFAULT_H7_FW_FN, "rb") as f:
    code = f.read()
  Panda.flash_static(np._handle, code, MCU_TYPE_H7)
  np.close()

  p.reconnect()
  assert p.bootstub
  p.flash()
  p.reset()
  check_signature(p)
