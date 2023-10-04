#!/usr/bin/env python3
import time
import pytest

from panda import Panda, PandaJungle

PANDA_SERIAL = "28002d000451323431333839"
JUNGLE_SERIAL = "26001c001451313236343430"

AUX_PORT = 6
OBDC_PORT = 1

class State:
  OFF = 1
  QDL = 2
  FASTBOOT = 3
  NORMAL_BOOT = 4


@pytest.fixture(scope="module")
def pj():
  jungle = PandaJungle(JUNGLE_SERIAL)

  # TODO: do the flashing here
  #jungle.set_panda_power(0)
  #time.sleep(5)
  #jungle.set_panda_power(1)
  #p = Panda(PANDA_SERIAL)
  #p.flash()

  return jungle


def setup_state(jungle, state):
  jungle.set_panda_power(0)

  if state == State.OFF:
    wait_for_shutdown(jungle)
  elif state == State.NORMAL_BOOT:
    jungle.set_panda_individual_power(OBDC_PORT, 1)
  else:
    raise ValueError(f"unkown state: {state}")


def wait_for_shutdown(jungle, timeout=30):
  time.sleep(5)

  st = time.monotonic()
  while PANDA_SERIAL in Panda.list():
    if time.monotonic() - st > timeout:
      raise Exception("took too long for device to turn off")
  print(f"took {time.monotonic() - st:.2f}s to shut off")

  health = jungle.health()
  assert all(health[f"ch{i}_power"] < 0.1 for i in range(1, 7))


def wait_for_boot(timeout):
  st = time.monotonic()

  Panda.wait_for_panda(PANDA_SERIAL, timeout)
  p = Panda(PANDA_SERIAL)

  assert p.health()['uptime'] < 5

  #while time.monotonic() - st < timeout:
  #  # check for 


  health = jungle.health()
  pwrs = [health[f"ch{i}_power"] < 0.1 for i in range(1, 7)]
  assert len(pwrs) == 1

def test_cold_boot(pj):
  setup_state(pj, State.OFF)
  setup_state(pj, State.NORMAL_BOOT)
  wait_for_boot(10)


def test_bootkick_ignition_line():
  pass

def test_bootkick_can_ignition():
  pass


if __name__ == "__main__":
  test_cold_boot()
