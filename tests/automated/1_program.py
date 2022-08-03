import time
from nose.tools import assert_equal

from panda import Panda, DEFAULT_FW_FN, DEFAULT_H7_FW_FN, MCU_TYPE_H7
from .helpers import test_all_pandas, panda_connect_and_init

def check_signature(p):
  assert not p.bootstub, "Flashed firmware not booting. Stuck in bootstub."
  fn = DEFAULT_H7_FW_FN if p.get_mcu_type() == MCU_TYPE_H7  else DEFAULT_FW_FN
  firmware_sig = Panda.get_signature_from_firmware(fn)
  panda_sig = p.get_signature()
  assert_equal(panda_sig, firmware_sig)

@test_all_pandas
@panda_connect_and_init(full_reset=False)
def test_a_recover(p):
  assert p.recover(timeout=30)
  check_signature(p)

@test_all_pandas
@panda_connect_and_init(full_reset=False)
def test_b_flash(p):
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
