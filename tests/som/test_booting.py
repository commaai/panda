import time
import pytest
import numpy as np
from collections import deque

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
  STANDBY_FOR_BOOTKICK = 5


@pytest.fixture(autouse=True, scope="function")
def pj():
  jungle = PandaJungle(JUNGLE_SERIAL)
  jungle.flash()

  jungle.set_ignition(False)

  yield jungle

  #jungle.set_panda_power(False)

@pytest.fixture(scope="function")
def p(pj):
  pj.set_panda_power(True)
  assert Panda.wait_for_panda(PANDA_SERIAL, 10)
  p = Panda(PANDA_SERIAL)
  p.flash()
  p.reset()
  return p

def setup_state(panda, jungle, state):
  jungle.set_panda_power(0)

  if state == State.OFF:
    wait_for_full_poweroff(jungle)
  elif state == State.NORMAL_BOOT:
    jungle.set_panda_individual_power(OBDC_PORT, 1)
  elif state == State.QDL:
    jungle.set_panda_individual_power(AUX_PORT, 1)
    time.sleep(0.5)
    jungle.set_panda_individual_power(OBDC_PORT, 1)
  elif state == State.STANDBY_FOR_BOOTKICK:
    wait_for_full_poweroff(jungle)
    jungle.set_panda_individual_power(OBDC_PORT, 1)
    wait_for_boot(panda, jungle)
    panda.set_safety_mode(Panda.SAFETY_SILENT)
    panda.send_heartbeat()
    wait_for_som_shutdown(panda, jungle)
  else:
    raise ValueError(f"unkown state: {state}")


def wait_for_som_shutdown(panda, jungle):
  # jungle power measurement isn't very accurate

  pwrs = deque([], maxlen=15)
  st = time.monotonic()
  while time.monotonic() - st < 30 and (len(pwrs) < pwrs.maxlen or np.mean(pwrs) > 2.):
    pwrs.append(jungle.health()[f"ch{OBDC_PORT}_power"])
    time.sleep(0.5)
  dt = time.monotonic() - st
  print(f"took {dt:.2f}s for SOM to shutdown", pwrs, np.mean(pwrs))

  assert len(pwrs) == pwrs.maxlen
  assert np.mean(pwrs) < 2.1

def wait_for_full_poweroff(jungle, timeout=30):
  st = time.monotonic()

  time.sleep(15)

  while PANDA_SERIAL in Panda.list():
    if time.monotonic() - st > timeout:
      raise Exception("took too long for device to turn off")

  health = jungle.health()
  assert all(health[f"ch{i}_power"] < 0.1 for i in range(1, 7))

def check_som_boot_flag(panda):
  h = panda.health()
  return h['safety_mode'] == Panda.SAFETY_ELM327 and h['safety_param'] == 30

def wait_for_boot(panda, jungle, bootkick=False, timeout=45):
  st = time.monotonic()

  Panda.wait_for_panda(PANDA_SERIAL, timeout)
  panda.reconnect()
  if bootkick:
    assert panda.health()['uptime'] > 30
  else:
    assert panda.health()['uptime'] < 3

  for i in range(3):
    assert not check_som_boot_flag(panda)
    time.sleep(1)

  # wait for SOM to bootup
  while not check_som_boot_flag(panda):
    if time.monotonic() - st > timeout:
      raise Exception("SOM didn't boot in time")
    time.sleep(1.0)


def test_cold_boot(p, pj):
  setup_state(p, pj, State.OFF)
  setup_state(p, pj, State.NORMAL_BOOT)
  wait_for_boot(p, pj)

def test_bootkick_ignition_line(p, pj):
  setup_state(p, pj, State.STANDBY_FOR_BOOTKICK)
  pj.set_ignition(True)
  wait_for_boot(p, pj, bootkick=True)

def test_bootkick_can_ignition(p, pj):
  setup_state(p, pj, State.STANDBY_FOR_BOOTKICK)
  for _ in range(10):
    # Mazda ignition signal
    pj.can_send(0x9E, b'\xc0\x00\x00\x00\x00\x00\x00\x00', 0)
    time.sleep(0.5)
  wait_for_boot(p, pj, bootkick=True)
