import os

from nose.tools import assert_equal

from panda import Panda, BASEDIR, DEFAULT_FW_FN, DEFAULT_H7_FW_FN
from .helpers import reset_pandas, test_all_pandas, panda_connect_and_init


# Reset the pandas before flashing them
def aaaa_reset_before_tests():
  reset_pandas()


@test_all_pandas
@panda_connect_and_init
def test_recover(p):
  assert p.recover(timeout=30)


@test_all_pandas
@panda_connect_and_init
def test_flash(p):
  p.flash()


@test_all_pandas
@panda_connect_and_init
def test_get_signature(p):
  if p.is_hw_h7():
    fn = DEFAULT_H7_FW_FN
  else:
    fn = DEFAULT_FW_FN

  firmware_sig = Panda.get_signature_from_firmware(fn)
  panda_sig = p.get_signature()

  assert_equal(panda_sig, firmware_sig)
