#!/usr/bin/env python3

import time
from panda import Panda

def get_overshoot_rpm(p, power):
  # make sure the fan is stopped completely
  p.set_fan_power(0)
  while p.get_fan_rpm() > 100:
    time.sleep(0.1)
  time.sleep(1)

  # set it to 30% power to mimic going onroad
  p.set_fan_power(power)
  max_rpm = 0
  for _ in range(70):
    max_rpm = max(max_rpm, p.get_fan_rpm())
    time.sleep(0.1)

  # tolerate 10% overshoot
  expected_rpm = Panda.MAX_FAN_RPMs[bytes(p.get_type())] * power / 100
  overshoot = (max_rpm / expected_rpm) - 1

  return overshoot, max_rpm

if __name__ == "__main__":
  p = Panda()

  for power in range(10, 101, 10):
    overshoot, max_rpm = get_overshoot_rpm(p, power)
    print(f"Fan power {power}% overshoot: {overshoot:.2f} Max RPM: {max_rpm}")
