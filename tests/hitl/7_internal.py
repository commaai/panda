import time
import pytest

from panda import Panda

pytestmark = [
  pytest.mark.skip_panda_types(Panda.HW_TYPE_UNO),
  pytest.mark.test_panda_types(Panda.INTERNAL_DEVICES)
]

def test_fan_controller(p):
  for power in [50, 100]:
    p.set_fan_power(power)
    time.sleep(10)

    expected_rpm = Panda.MAX_FAN_RPMs[bytes(p.get_type())] * power / 100
    assert 0.95 * expected_rpm <= p.get_fan_rpm() <= 1.05 * expected_rpm

def test_fan_cooldown(p):
  # if the fan cooldown doesn't work, we get high frequency noise on the tach line 
  # while the rotor spins down. this makes sure it never goes beyond the expected max RPM
  p.set_fan_power(100)
  time.sleep(3)
  p.set_fan_power(0)
  for _ in range(5):
    assert p.get_fan_rpm() <= 7000
    time.sleep(0.5)

def test_fan_overshoot(p):
  # make sure it's stopped completely
  p.set_fan_power(0)
  while p.get_fan_rpm() > 100:
    time.sleep(0.1)

  # set it to 30% power to mimic going onroad
  p.set_fan_power(30)
  max_rpm = 0
  for _ in range(50):
    max_rpm = max(max_rpm, p.get_fan_rpm())
    time.sleep(0.1)

  # tolerate 10% overshoot
  expected_rpm = Panda.MAX_FAN_RPMs[bytes(p.get_type())] * 30 / 100
  assert max_rpm <= 1.1 * expected_rpm, f"Fan overshoot: {(max_rpm / expected_rpm * 100) - 100:.1f}%"
