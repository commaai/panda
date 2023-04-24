import pytest
import random

from panda import Panda, PandaDFU
from panda.python.spi import SpiDevice

def test_dfu(p):
  app_mcu_type = p.get_mcu_type()
  dfu_serial = p.get_dfu_serial()

  p.reset(enter_bootstub=True)
  p.reset(enter_bootloader=True)
  assert Panda.wait_for_dfu(dfu_serial, timeout=20), "failed to enter DFU"

  dfu = PandaDFU(dfu_serial)
  assert dfu.get_mcu_type() == app_mcu_type

  assert dfu_serial in PandaDFU.list()

  dfu._handle.clear_status()
  dfu.reset()
  p.reconnect()


@pytest.mark.test_panda_types((Panda.HW_TYPE_TRES, ))
def test_dfu_with_spam(p):
  dfu_serial = p.get_dfu_serial()

  # enter DFU
  p.reset(enter_bootstub=True)
  p.reset(enter_bootloader=True)
  assert Panda.wait_for_dfu(dfu_serial, timeout=20), "failed to enter DFU"

  # send junk
  for _ in range(10):
    speed = 1000000 * random.randint(1, 5)
    d = SpiDevice(speed=speed)
    with d.acquire() as spi:
      dat = [random.randint(0, 255) for _ in range(random.randint(1, 100))]
      spi.xfer(dat)

  # should still show up
  assert dfu_serial in PandaDFU.list()
