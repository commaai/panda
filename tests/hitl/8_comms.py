import time
import pytest
import random

from panda import Panda
from panda.python.spi import SpiDevice, PandaSpiNackResponse

pytestmark = [
  pytest.mark.test_panda_types((Panda.HW_TYPE_TRES, ))
]

class TestSpi:
  def _ping(self, mocker, panda):
    # should work with no retries
    spy = mocker.spy(panda._handle, '_wait_for_ack')
    panda.health()
    assert spy.call_count == 2
    mocker.stop(spy)

  def test_bad_header(self, mocker, p):
    with mocker.patch('panda.python.spi.SYNC', return_value=0):
      with pytest.raises(PandaSpiNackResponse):
        p.health()
    mocker.stopall()
    self._ping(mocker, p)

  def test_bad_checksum(self, mocker, p):
    with mocker.patch('panda.python.spi.PandaSpiHandle._calc_checksum', return_value=0):
      with pytest.raises(PandaSpiNackResponse):
        p.health()
    mocker.stopall()
    self._ping(mocker, p)

  def test_non_existent_endpoint(self, mocker, p):
    for _ in range(10):
      ep = random.randint(4, 20)
      with pytest.raises(PandaSpiNackResponse):
        p._handle.bulkRead(ep, random.randint(1, 1000))

      self._ping(mocker, p)

      with pytest.raises(PandaSpiNackResponse):
        p._handle.bulkWrite(ep, b"abc")

      self._ping(mocker, p)
