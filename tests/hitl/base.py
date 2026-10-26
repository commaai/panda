import os
import signal
import unittest

from panda import Panda, PandaDFU
from panda.tests.hitl.helpers import clear_can_buffers

SPEED_NORMAL = 500
BUS_SPEEDS = [(bus, SPEED_NORMAL) for bus in range(4)]

# Find all pandas connected
_up = None
# None means unknown; otherwise (configuration dirty, potentially pending TX buses).
# This belongs to the UP session, not a test class: unittest can revisit classes.
_up_dirty = None
_panda_type = None
_panda_serial = None
def init_devices():
  # the DUT is plugged into a UP: run the tests with `up run` or in `up ssh` on the UP.
  # ultimate_provisioning isn't a dependency, those put it on the DUT's PYTHONPATH
  from ultimate_provisioning import UP
  global _up, _up_dirty
  _up = UP.remote()
  _up_dirty = None

  with Panda(serial=None, claim=False) as p:
    global _panda_type
    global _panda_serial
    _panda_serial = p.get_usb_serial()
    _panda_type = bytes(p.get_type())
  assert _panda_serial is not None, "No panda found!"

def init_up():
  global _up_dirty
  configuration, tx_buses = _up_dirty if _up_dirty is not None else (True, range(4))
  # A failed restoration or subsequent panda reset must retry the full baseline.
  _up_dirty = None
  clear_can_buffers(_up, tx_buses=tx_buses)
  if configuration:
    _up.set_ignition(True)
    _up.set_harness_orientation(1)  # UP.HARNESS_ORIENTATION_1
    for bus, speed in BUS_SPEEDS:
      _up.set_can_speed_kbps(bus, speed)


def panda_test(**options):
  def decorate(test):
    test.panda_options = {**getattr(test, "panda_options", {}), **options}
    return test
  return decorate


@unittest.skipUnless(os.environ.get("HITL") == "1", "hardware tests require HITL=1")
class PandaTestCase(unittest.TestCase):
  panda_types = None
  # Conservative effects, including interrupted/failed tests. Opt out only when
  # UP configuration is untouched; list every UP bus that may transmit separately.
  reset_up = True
  up_can_tx = None  # default: all four buses if reset_up, otherwise none

  @classmethod
  def setUpClass(cls):
    if _panda_serial is None:
      init_devices()
    # Open one handle per class; setUp resets the hardware before each test.
    assert Panda.wait_for_panda(_panda_serial, timeout=10), "panda not found"
    p = Panda(serial=_panda_serial)
    cls.addClassCleanup(p.close)
    cls.p = p

  def setUp(self):
    global _up_dirty
    self.options = getattr(getattr(self, self._testMethodName), "panda_options", {})
    panda_types = self.options.get("panda_types", self.panda_types)
    if panda_types is not None and _panda_type not in panda_types:
      self.skipTest(f"Not applicable, {panda_types} pandas only")

    previous_handler = signal.signal(signal.SIGALRM, self._timeout)
    self.addCleanup(signal.signal, signal.SIGALRM, previous_handler)
    self.addCleanup(signal.setitimer, signal.ITIMER_REAL, 0)
    signal.setitimer(signal.ITIMER_REAL, self.options.get("timeout", 60))

    init_up()
    self.up = _up
    self.p.reset()
    assert self.p.up_to_date()
    configuration = self.options.get("reset_up", self.reset_up)
    tx_buses = self.options.get("up_can_tx", self.up_can_tx)
    if tx_buses is None:
      tx_buses = tuple(range(4)) if configuration else ()
    # Record possible effects BEFORE the test, never infer TX completion from RX.
    _up_dirty = (configuration, tx_buses)

  def _timeout(self, signum, frame):
    raise TimeoutError(f"{self.id()} exceeded its test timeout")

  def tearDown(self):
    global _up_dirty
    dirty = _up_dirty
    _up_dirty = None
    p = self.p
    # reset() closes the connection before entering DFU.
    if not p.connected:
      if p.get_dfu_serial() in PandaDFU.list():
        with PandaDFU(p.get_dfu_serial()) as dfu:
          dfu.reset()
      p.reconnect()
    if p.bootstub:
      p.reset()

    assert not p.bootstub

    # Check for faults
    assert p.health()['faults'] == 0
    assert p.health()['fault_status'] == 0

    # Check for SPI errors
    #assert p.health()['spi_error_count'] == 0

    # Check health of each CAN core after test, normal to fail for test_gen2_loopback on OBD bus, so skipping
    if not self.options.get("expect_can_error", False):
      for i in range(3):
        can_health = p.can_health(i)
        assert can_health['bus_off_cnt'] == 0
        assert can_health['receive_error_cnt'] < 127
        assert can_health['transmit_error_cnt'] < 255
        assert can_health['error_passive'] == 0
        assert can_health['error_warning'] == 0
        assert can_health['total_rx_lost_cnt'] == 0
        assert can_health['total_tx_lost_cnt'] == 0
        assert can_health['total_error_cnt'] == 0
        assert can_health['total_tx_checksum_error_cnt'] == 0

    _up_dirty = dirty

