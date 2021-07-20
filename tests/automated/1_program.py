import os

from nose.tools import assert_equal

from panda import Panda, BASEDIR
from .helpers import reset_pandas, test_all_pandas, test_non_gen3_pandas, test_all_gen3_pandas, panda_connect_and_init


# Reset the pandas before flashing them
def aaaa_reset_before_tests():
  reset_pandas()


@test_non_gen3_pandas
@panda_connect_and_init
def test_recover(p):
  assert p.recover(timeout=30)


@test_all_gen3_pandas
@panda_connect_and_init
def test_recover_gen3(p):
  assert p.recover(timeout=30)


@test_non_gen3_pandas
@panda_connect_and_init
def test_flash(p):
  p.flash()


@test_all_gen3_pandas
@panda_connect_and_init
def test_flash_gen3(p):
  p.flash()


@test_non_gen3_pandas
@panda_connect_and_init
def test_get_signature(p):
  fn = os.path.join(BASEDIR, "board/obj/panda.bin.signed")

  firmware_sig = Panda.get_signature_from_firmware(fn)
  panda_sig = p.get_signature()

  assert_equal(panda_sig, firmware_sig)


@test_all_gen3_pandas
@panda_connect_and_init
def test_get_signature_gen3(p):
  fn = os.path.join(BASEDIR, "board/obj/panda_gen3.bin.signed")

  firmware_sig = Panda.get_signature_from_firmware(fn)
  panda_sig = p.get_signature()

  assert_equal(panda_sig, firmware_sig)
