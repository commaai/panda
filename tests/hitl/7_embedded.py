import time
import pytest

from panda.tests.hitl.conftest import PandaGroup

@pytest.mark.test_panda_types(PandaGroup.EMBEDDED)
def test_fan_controller(p):
  for power, expected_rpm in [(50, 3250), (100, 6500)]:
    p.set_fan_power(power)
    time.sleep(10)
    assert 0.95 * expected_rpm <= p.get_fan_rpm() <= 1.05 * expected_rpm
  p.set_fan_power(0)

@pytest.mark.test_panda_types(PandaGroup.EMBEDDED)
def test_fan_cooldown(p):
  # if the fan cooldown doesn't work, we get high frequency noise on the tach line 
  # while the rotor spins down. this makes sure it never goes beyond the expected max RPM
  p.set_fan_power(100)
  time.sleep(3)
  p.set_fan_power(0)
  for _ in range(5):
    assert p.get_fan_rpm() <= 7000
    time.sleep(0.5)


