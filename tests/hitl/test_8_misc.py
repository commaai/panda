import os
import random
import re
import statistics
import time

from opendbc.car.structs import CarParams
from panda.tests.hitl.base import PandaTestCase, panda_test
from panda import Panda

# EXTI lines that must wake the panda from stop mode, by orientation
EXTI_SBU2, EXTI_SBU1 = 1 << 1, 1 << 4
EXTI_FDCAN2_RX, EXTI_FDCAN1_RX, EXTI_FDCAN3_RX = 1 << 5, 1 << 8, 1 << 12
WAKEUPS = {
  "ign": lambda orientation: EXTI_SBU1 if orientation == Panda.HARNESS_STATUS_NORMAL else EXTI_SBU2,
  # UP buses, which follow the orientation, see LANES in ultimate_provisioning
  "0": lambda orientation: EXTI_FDCAN1_RX,
  "1": lambda orientation: EXTI_FDCAN2_RX,
  "2": lambda orientation: EXTI_FDCAN3_RX,
}

def stop_mode_wakeup(p):
  m = re.search(rb"stop mode wakeup EXTI 0x([0-9a-f]{8})", p.debug_read(4096))
  assert m is not None, "no stop mode wakeup logged"
  return int(m.group(1), 16)

class TestMisc(PandaTestCase):
  def test_boot_time(self):
    p = self.p
    # boot time should be instant
    st = time.monotonic()
    p.reset(reconnect=False)
    assert Panda.wait_for_panda(p.get_usb_serial(), timeout=3.0)

    # USB enumeration is slow, so SPI is faster
    assert time.monotonic() - st < (1.0 if p.spi else 5.0)

  @panda_test(expect_can_error=True)
  @panda_test(panda_types=(Panda.HW_TYPE_CUATRO, ))
  def test_stop_mode(self):
    p = self.p
    up = self.up
    serial = p.get_usb_serial()
    seed = int(os.environ.get("STOP_MODE_SEED", random.randrange(2**32)))
    print(f"STOP_MODE_SEED={seed}")
    rng = random.Random(seed)

    # every wakeup in both orientations, in a random order, each after random stimuli that mustn't wake it.
    # anything only the SOM drives (SPI, USB, I2S) may wake it, the stop mode is useless with the SOM up anyway
    rounds = [(o, w) for o in (Panda.HARNESS_STATUS_NORMAL, Panda.HARNESS_STATUS_FLIPPED) for w in WAKEUPS]
    rng.shuffle(rounds)
    savings = []
    for orientation, wakeup in rounds:
      up.set_harness_orientation(orientation)
      while p.health()['car_harness_status'] != orientation:
        time.sleep(0.01)

      awake = up.get_power(0.25)
      p.enter_stop_mode()
      p.close()
      while serial in Panda.list():
        time.sleep(0.01)

      # the OBD pair isn't routed to the panda's CAN RX pins in either orientation
      for _ in range(rng.randrange(4)):
        up.can_send(rng.randrange(0x800), rng.randbytes(rng.randrange(9)), 3)
      # a wakeup brings the panda back in 0.3s
      savings.append(awake - up.get_power(0.25))
      time.sleep(0.1)
      if serial in Panda.list():
        p.reconnect()
        raise AssertionError(f"woke up before {wakeup}: {stop_mode_wakeup(p):#x}")

      print(f"orientation={orientation} wakeup={wakeup} saved {savings[-1] * 1000:.0f} mW")
      if wakeup == "ign":
        up.set_ignition(True)
      else:
        up.can_send(0x123, b'\x01\x02', int(wakeup))
      assert Panda.wait_for_panda(serial, timeout=1)
      p.reconnect()
      up.set_ignition(False)
      # ACK what the UP is still retrying, or it wakes the next stop mode: leave SILENT for the wakeup frame,
      # and route the OBD pair for the others. those swap with bus 1 in the other orientation, see LANES
      p.set_safety_mode(CarParams.SafetyModel.noOutput)
      p.set_obd(True)

      assert stop_mode_wakeup(p) == WAKEUPS[wakeup](orientation)
      assert p.health()['uptime'] < 3

    # stop mode saves ~0.2 W, on top of the SOM's noisy ~1.5 W
    assert statistics.median(savings) > 0.1, savings
