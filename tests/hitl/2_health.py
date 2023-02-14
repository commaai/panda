import time

from panda import Panda
from panda_jungle import PandaJungle  # pylint: disable=import-error
from .helpers import panda_jungle, test_all_pandas, test_all_gen2_pandas, panda_connect_and_init


@test_all_pandas
@panda_connect_and_init
def test_ignition(p):
  # Set harness orientation to #2, since the ignition line is on the wrong SBU bus :/
  panda_jungle.set_harness_orientation(PandaJungle.HARNESS_ORIENTATION_2)
  p.reset()

  for ign in (True, False):
    panda_jungle.set_ignition(ign)
    time.sleep(2)
    assert p.health()['ignition_line'] == ign


@test_all_gen2_pandas
@panda_connect_and_init
def test_orientation_detection(p):
  seen_orientations = []
  for i in range(3):
    panda_jungle.set_harness_orientation(i)
    time.sleep(0.25)
    p.reset()
    time.sleep(0.25)

    detected_harness_orientation = p.health()['car_harness_status']
    print(f"Detected orientation: {detected_harness_orientation}")
    if (i == 0 and detected_harness_orientation != 0) or detected_harness_orientation in seen_orientations:
      assert False
    seen_orientations.append(detected_harness_orientation)

@test_all_pandas
@panda_connect_and_init
def test_voltage(p):
  for _ in range(10):
    voltage = p.health()['voltage']
    assert ((voltage > 10000) and (voltage < 14000))
    time.sleep(0.1)

@test_all_pandas
@panda_connect_and_init
def test_hw_type(p):
  """
    hw type should be same in bootstub as application
  """

  hw_type = p.get_type()
  mcu_type = p.get_mcu_type()
  assert mcu_type is not None

  app_uid =  p.get_uid()
  usb_serial = p.get_usb_serial()
  assert app_uid == usb_serial

  p.reset(enter_bootstub=True, reconnect=True)
  p.close()
  time.sleep(3)
  with Panda(p.get_usb_serial()) as pp:
    assert pp.bootstub
    assert pp.get_type() == hw_type, "Bootstub and app hw type mismatch"
    assert pp.get_mcu_type() == mcu_type, "Bootstub and app MCU type mismatch"
    assert pp.get_uid() == app_uid


@test_all_pandas
@panda_connect_and_init
def test_heartbeat(p):
  # TODO: add more cases here once the tests aren't super slow
  p.set_safety_mode(mode=Panda.SAFETY_HYUNDAI, param=Panda.FLAG_HYUNDAI_LONG)
  p.send_heartbeat()
  assert p.health()['safety_mode'] == Panda.SAFETY_HYUNDAI
  assert p.health()['safety_param'] == Panda.FLAG_HYUNDAI_LONG

  # shouldn't do anything once we're in a car safety mode
  p.set_heartbeat_disabled()

  time.sleep(6)

  h = p.health()
  assert h['heartbeat_lost']
  assert h['safety_mode'] == Panda.SAFETY_SILENT
  assert h['safety_param'] == 0
  assert h['controls_allowed'] == 0

@test_all_pandas
@panda_connect_and_init
def test_microsecond_timer(p):
  start_time = p.get_microsecond_timer()
  time.sleep(1)
  end_time = p.get_microsecond_timer()

  # account for uint32 overflow
  if end_time < start_time:
    end_time += 2**32

  time_diff = (end_time - start_time) / 1e6
  assert 0.98 < time_diff  < 1.02, f"Timer not running at the correct speed! (got {time_diff:.2f}s instead of 1.0s)"
