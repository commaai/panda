import logging
import time
import pytest

from panda import Panda

logger = logging.getLogger("stop_mode")

def test_boot_time(p):
  # boot time should be instant
  st = time.monotonic()
  p.reset(reconnect=False)
  assert Panda.wait_for_panda(p.get_usb_serial(), timeout=3.0)

  # USB enumeration is slow, so SPI is faster
  assert time.monotonic() - st < (1.0 if p.spi else 5.0)

@pytest.mark.test_panda_types((Panda.HW_TYPE_CUATRO, ))
def test_stop_mode(p, panda_jungle):
  serial = p.get_usb_serial()

  for orientation in (Panda.HARNESS_STATUS_NORMAL, Panda.HARNESS_STATUS_FLIPPED):
    panda_jungle.set_harness_orientation(orientation)
    time.sleep(0.5)

    for wakeup in ("ign", "can0", "can2"):
      panda_jungle.set_ignition(False)
      time.sleep(0.25)

      h = p.health()
      logger.warning(f"\n--- orientation={orientation} wakeup={wakeup} ---")
      logger.warning(f"  harness={h['car_harness_status']} sbu1={h['sbu1_voltage_mV']}mV sbu2={h['sbu2_voltage_mV']}mV ign={h['ignition_line']} faults={h['faults']}")

      assert serial in Panda.list()

      p.set_safety_mode()
      p.enter_stop_mode()
      p.close()

      time.sleep(1)

      assert serial not in Panda.list()

      t_wake = time.monotonic()
      if wakeup == "ign":
        panda_jungle.set_ignition(True)
      elif wakeup == "can0":
        panda_jungle.can_send(0x123, b'\x01\x02', 0)
      elif wakeup == "can2":
        panda_jungle.can_send(0x123, b'\x01\x02', 2)

      p.reconnect()
      t_found = time.monotonic() - t_wake

      h = p.health()
      logger.warning(f"  woke in {t_found:.2f}s uptime={h['uptime']}s faults={h['faults']}")
      assert h['uptime'] <= 2
