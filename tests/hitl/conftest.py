import concurrent.futures
import os
import time
import pytest

from panda import Panda
from panda_jungle import PandaJungle  # pylint: disable=import-error
from panda.tests.hitl.helpers import clear_can_buffers


SPEED_NORMAL = 500
SPEED_GMLAN = 33.3
BUS_SPEEDS = [(0, SPEED_NORMAL), (1, SPEED_NORMAL), (2, SPEED_NORMAL), (3, SPEED_GMLAN)]


PEDAL_SERIAL = 'none'
JUNGLE_SERIAL = os.getenv("PANDAS_JUNGLE")
PANDAS_EXCLUDE = os.getenv("PANDAS_EXCLUDE", "").strip().split(" ")
PARTIAL_TESTS = os.environ.get("PARTIAL_TESTS", "0") == "1"

class PandaGroup:
  H7 = (Panda.HW_TYPE_RED_PANDA, Panda.HW_TYPE_RED_PANDA_V2)
  GEN2 = (Panda.HW_TYPE_BLACK_PANDA, Panda.HW_TYPE_UNO) + H7
  GPS = (Panda.HW_TYPE_BLACK_PANDA, Panda.HW_TYPE_UNO)
  GMLAN = (Panda.HW_TYPE_WHITE_PANDA, Panda.HW_TYPE_GREY_PANDA)

  TESTED = (Panda.HW_TYPE_WHITE_PANDA, Panda.HW_TYPE_BLACK_PANDA, Panda.HW_TYPE_RED_PANDA, Panda.HW_TYPE_RED_PANDA_V2, Panda.HW_TYPE_UNO)

if PARTIAL_TESTS:
  # minimal set of pandas to get most of our coverage
  # * red panda covers GEN2, STM32H7
  # * black panda covers STM32F4, GEN2, and GPS
  PandaGroup.TESTED = (Panda.HW_TYPE_BLACK_PANDA, Panda.HW_TYPE_RED_PANDA)  # type: ignore

# Find all pandas connected
_all_pandas = {}
_panda_jungle = None
def init_all_pandas():
  global _panda_jungle
  _panda_jungle = PandaJungle(JUNGLE_SERIAL)
  _panda_jungle.set_panda_power(True)

  for serial in Panda.list():
    if serial not in PANDAS_EXCLUDE and serial != PEDAL_SERIAL:
      with Panda(serial=serial) as p:
        ptype = bytes(p.get_type())
        if ptype in PandaGroup.TESTED:
          _all_pandas[serial] = ptype

  # ensure we have all tested panda types
  missing_types = set(PandaGroup.TESTED) - set(_all_pandas.values())
  assert len(missing_types) == 0, f"Missing panda types: {missing_types}"

  print(f"{len(_all_pandas)} total pandas")
init_all_pandas()
_all_panda_serials = list(_all_pandas.keys())


def init_jungle():
  clear_can_buffers(_panda_jungle)
  _panda_jungle.set_panda_power(True)
  _panda_jungle.set_can_loopback(False)
  _panda_jungle.set_obd(False)
  _panda_jungle.set_harness_orientation(PandaJungle.HARNESS_ORIENTATION_1)
  for bus, speed in BUS_SPEEDS:
    _panda_jungle.set_can_speed_kbps(bus, speed)


def pytest_configure(config):
  config.addinivalue_line(
    "markers", "test_panda_types(name): mark test to run only on specified panda types"
  )
  config.addinivalue_line(
    "markers", "panda_expect_can_error: mark test to ignore CAN health errors"
  )


def pytest_make_parametrize_id(config, val, argname):
  if val in _all_pandas:
    # TODO: get nice string instead of int
    hw_type = _all_pandas[val][0]
    return f"serial={val}, hw_type={hw_type}"
  return None


@pytest.fixture(name='panda_jungle')
def fixture__panda_jungle(request):
  init_jungle()
  return _panda_jungle

@pytest.fixture(name='p')
def func_fixture_panda(request, module_panda):
  p = module_panda

  # Check if test is applicable to this panda
  mark = request.node.get_closest_marker('test_panda_types')
  if mark:
    assert len(mark.args) > 0, "Missing allowed panda types in mark"
    test_types = mark.args[0]
    if _all_pandas[p.get_usb_serial()] not in test_types:
      pytest.skip(f"Not applicable, {test_types} pandas only")

  # TODO: reset is slow (2+ seconds)
  p.reset()

  # Run test
  yield p

  # Teardown
  if not p.connected:
    p.reconnect()
  if p.bootstub:
    p.reset()

  # TODO: would be nice to make these common checks in the teardown
  # show up as failed tests instead of "errors"

  # Check for faults
  assert p.health()['fault_status'] == 0

  # Check health of each CAN core after test, normal to fail for test_gen2_loopback on OBD bus, so skipping
  mark = request.node.get_closest_marker('panda_expect_can_error')
  expect_can_error = mark is not None
  if not expect_can_error:
    for i in range(3):
      can_health = p.can_health(i)
      assert can_health['bus_off_cnt'] == 0
      assert can_health['receive_error_cnt'] == 0
      assert can_health['transmit_error_cnt'] == 0
      assert can_health['total_rx_lost_cnt'] == 0
      assert can_health['total_tx_lost_cnt'] == 0
      assert can_health['total_error_cnt'] == 0
      assert can_health['total_tx_checksum_error_cnt'] == 0

@pytest.fixture(name='module_panda', params=_all_panda_serials, scope='module')
def fixture_panda_setup(request):
  """
    Clean up all pandas + jungle and return the panda under test.
  """
  panda_serial = request.param

  # Initialize jungle
  init_jungle()

  # wait for all pandas to come up
  for _ in range(50):
    if set(_all_panda_serials).issubset(set(Panda.list())):
      break
    time.sleep(0.1)

  # Connect to pandas
  def cnnct(s):
    if s == panda_serial:
      p = Panda(serial=s)
      p.reset(reconnect=True)

      p.set_can_loopback(False)
      p.set_gmlan(None)
      p.set_esp_power(False)
      p.set_power_save(False)
      for bus, speed in BUS_SPEEDS:
        p.set_can_speed_kbps(bus, speed)
      clear_can_buffers(p)
      p.set_power_save(False)
      return p
    else:
      with Panda(serial=s) as p:
        p.reset(reconnect=False)
    return None

  with concurrent.futures.ThreadPoolExecutor() as exc:
    ps = list(exc.map(cnnct, _all_panda_serials, timeout=20))
    pandas = [p for p in ps if p is not None]

  # run test
  yield pandas[0]

  # Teardown
  for p in pandas:
    p.close()
