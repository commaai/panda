import time
import pytest

from panda import PandaSerial
from panda.tests.hitl.conftest import PandaGroup


@pytest.mark.test_panda_types(PandaGroup.GPS)
def test_gps_version(p):
  serial = PandaSerial(p, 1, 9600)
  # Reset and check twice to make sure the enabling works
  for _ in range(2):
    # Reset GPS
    p.set_esp_power(0)
    time.sleep(2)
    p.set_esp_power(1)
    time.sleep(1)

    # Read startup message and check if version is contained
    dat = serial.read(0x1000)    # Read one full panda DMA buffer. This should include the startup message
    assert b'HPG 1.40ROV' in dat