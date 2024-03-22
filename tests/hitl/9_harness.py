import time
import pytest

from panda import Panda
from panda import PandaJungle
from panda.tests.hitl.conftest import PandaGroup


@pytest.mark.parametrize("ignition", [True, False])
@pytest.mark.parametrize("orientation", [Panda.HARNESS_STATUS_NC, Panda.HARNESS_STATUS_NORMAL, Panda.HARNESS_STATUS_FLIPPED])
@pytest.mark.test_panda_types(PandaGroup.GEN2)
def test_harness_status(p, panda_jungle, ignition, orientation):
  flipped = None
  panda_jungle.set_harness_orientation(orientation)
  panda_jungle.set_ignition(ignition)
  time.sleep(0.25)  # updated at 8Hz

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
