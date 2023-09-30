import time
import pytest

from panda import Panda
from panda import PandaJungle
from panda.tests.hitl.conftest import PandaGroup


def test_ignition(p, panda_jungle):
  # Set harness orientation to #2, since the ignition line is on the wrong SBU bus :/
  panda_jungle.set_harness_orientation(PandaJungle.HARNESS_ORIENTATION_2)
  p.reset()

  for ign in (True, False):
    panda_jungle.set_ignition(ign)
    time.sleep(0.1)
    assert p.health()['ignition_line'] == ign


@pytest.mark.test_panda_types(PandaGroup.GEN2)
def test_harness_status(p, panda_jungle):
  flipped = None
  for ignition in [True, False]:
    for orientation in [Panda.HARNESS_STATUS_NC, Panda.HARNESS_STATUS_NORMAL, Panda.HARNESS_STATUS_FLIPPED]:
      panda_jungle.set_harness_orientation(orientation)
      panda_jungle.set_ignition(ignition)
      time.sleep(1)

      health = p.health()
      detected_orientation = health['car_harness_status']
      print(f"set: {orientation} detected: {detected_orientation}")

      # Orientation
      if orientation == Panda.HARNESS_STATUS_NC:
        assert detected_orientation == Panda.HARNESS_STATUS_NC
      else:
        if flipped is None:
          flipped = (detected_orientation != orientation)

        if orientation == Panda.HARNESS_STATUS_NORMAL:
          assert detected_orientation == (Panda.HARNESS_STATUS_FLIPPED if flipped else Panda.HARNESS_STATUS_NORMAL)
        else:
          assert detected_orientation == (Panda.HARNESS_STATUS_NORMAL if flipped else Panda.HARNESS_STATUS_FLIPPED)

      # Line ignition
      assert health['ignition_line'] == (False if orientation == Panda.HARNESS_STATUS_NC else ignition)

      # SBU voltages
      supply_voltage_mV = 1800 if p.get_type() in [Panda.HW_TYPE_TRES, ] else 3300

      if orientation == Panda.HARNESS_STATUS_NC:
        assert health['sbu1_voltage_mV'] > 0.9 * supply_voltage_mV
        assert health['sbu2_voltage_mV'] > 0.9 * supply_voltage_mV
      else:
        relay_line = 'sbu1_voltage_mV' if (detected_orientation == Panda.HARNESS_STATUS_FLIPPED) else 'sbu2_voltage_mV'
        ignition_line = 'sbu2_voltage_mV' if (detected_orientation == Panda.HARNESS_STATUS_FLIPPED) else 'sbu1_voltage_mV'

        assert health[relay_line] < 0.1 * supply_voltage_mV
        assert health[ignition_line] > health[relay_line]
        if ignition:
          assert health[ignition_line] < 0.3 * supply_voltage_mV
        else:
          assert health[ignition_line] > 0.9 * supply_voltage_mV



@pytest.mark.skip_panda_types((Panda.HW_TYPE_DOS, ))
def test_voltage(p):
  for _ in range(10):
    voltage = p.health()['voltage']
    assert ((voltage > 11000) and (voltage < 13000))
    time.sleep(0.1)

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

def test_heartbeat(p, panda_jungle):
  panda_jungle.set_ignition(True)
  # TODO: add more cases here once the tests aren't super slow
  p.set_safety_mode(mode=Panda.SAFETY_HYUNDAI, param=Panda.FLAG_HYUNDAI_LONG)
  p.send_heartbeat()
  assert p.health()['safety_mode'] == Panda.SAFETY_HYUNDAI
  assert p.health()['safety_param'] == Panda.FLAG_HYUNDAI_LONG

  # shouldn't do anything once we're in a car safety mode
  p.set_heartbeat_disabled()

  time.sleep(6.)

  h = p.health()
  assert h['heartbeat_lost']
  assert h['safety_mode'] == Panda.SAFETY_SILENT
  assert h['safety_param'] == 0
  assert h['controls_allowed'] == 0

def test_microsecond_timer(p):
  start_time = p.get_microsecond_timer()
  time.sleep(1)
  end_time = p.get_microsecond_timer()

  # account for uint32 overflow
  if end_time < start_time:
    end_time += 2**32

  time_diff = (end_time - start_time) / 1e6
  assert 0.98 < time_diff  < 1.02, f"Timer not running at the correct speed! (got {time_diff:.2f}s instead of 1.0s)"
