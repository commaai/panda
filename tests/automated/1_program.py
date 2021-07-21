import os

from nose.tools import assert_equal

from panda import Panda, BASEDIR
from .helpers import reset_pandas, test_all_pandas, panda_connect_and_init


# Reset the pandas before flashing them
def aaaa_reset_before_tests():
  reset_pandas()


@test_all_pandas
@panda_connect_and_init
def test_recover_gen3(p):
  assert p.recover(timeout=30)


@test_all_pandas
@panda_connect_and_init
def test_flash_gen3(p):
  p.flash()


@test_all_pandas
@panda_connect_and_init
def test_get_signature(p):
  if p.get_hw_generation() == 3:
    fn = os.path.join(BASEDIR, "board/obj/panda_gen3.bin.signed")
  else:
    fn = os.path.join(BASEDIR, "board/obj/panda.bin.signed")

  firmware_sig = Panda.get_signature_from_firmware(fn)
  panda_sig = p.get_signature()

  assert_equal(panda_sig, firmware_sig)
