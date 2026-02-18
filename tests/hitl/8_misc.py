import time
import pytest

from panda import Panda

def test_boot_time(p):
  # boot time should be instant
  st = time.monotonic()
  p.reset(reconnect=False)
  assert Panda.wait_for_panda(p.get_usb_serial(), timeout=3.0)

  # USB enumeration is slow, so SPI is faster
  assert time.monotonic() - st < (1.0 if p.spi else 5.0)

@pytest.mark.test_panda_types((Panda.HW_TYPE_CUATRO, ))
def test_stop_mode(p, panda_jungle):
  serial = p.get_usb_serial()

  # ignition off so panda can enter stop mode
  panda_jungle.set_ignition(False)
  time.sleep(1)

  # enter stop mode
  p.set_safety_mode()
  p.enter_stop_mode()

  # verify panda disconnected
  time.sleep(0.5)
  assert serial not in Panda.list()

  # wake via ignition
  panda_jungle.set_ignition(True)

  # panda should reset and come back
  assert Panda.wait_for_panda(serial, timeout=10)
  p.reconnect()

