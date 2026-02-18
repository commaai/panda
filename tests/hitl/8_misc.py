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
    panda_jungle.set_ignition(False)
    panda_jungle.set_harness_orientation(orientation)
    time.sleep(0.25)

    for wakeup in "ign", "can":
      logger.debug(f"--- orientation={orientation} wakeup={wakeup} ---")
      h = p.health()
      logger.debug(f"before stop: uptime={h['uptime']}s, voltage={h['voltage']}mV, current={h['current']}mA")
      logger.debug(f"  ignition_line={h['ignition_line']}, ignition_can={h['ignition_can']}")
      logger.debug(f"  safety_mode={h['safety_mode']}, power_save={h['power_save_enabled']}")
      logger.debug(f"  faults={h['faults']}, fan={h['fan_power']}")

      # enter stop mode
      p.set_safety_mode()
      p.enter_stop_mode()
      p.close()
      logger.debug(f"stop mode requested, closed connection")

      # wait for panda to enter stop mode
      time.sleep(0.5)
      logger.debug(f"waited 0.5s, sending wakeup: {wakeup}")

      # wake via ignition or CAN activity
      t_wake = time.monotonic()
      if wakeup == "ign":
        panda_jungle.set_ignition(True)
      else:
        panda_jungle.can_send(0x123, b'\x01\x02', 0)

      # panda should reset and come back
      assert Panda.wait_for_panda(serial, timeout=10)
      t_found = time.monotonic() - t_wake
      logger.debug(f"panda found after {t_found:.2f}s")

      p.reconnect()
      h = p.health()
      logger.debug(f"after wakeup: uptime={h['uptime']}s, voltage={h['voltage']}mV, current={h['current']}mA")
      logger.debug(f"  ignition_line={h['ignition_line']}, ignition_can={h['ignition_can']}")
      logger.debug(f"  safety_mode={h['safety_mode']}, power_save={h['power_save_enabled']}")
      logger.debug(f"  faults={h['faults']}, fan={h['fan_power']}")
      logger.debug(f"  harness={h['car_harness_status']}, sbu1={h['sbu1_voltage_mV']}mV, sbu2={h['sbu2_voltage_mV']}mV")
      assert h['uptime'] < 3
