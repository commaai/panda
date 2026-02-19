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

@pytest.mark.panda_expect_can_error
@pytest.mark.test_panda_types((Panda.HW_TYPE_CUATRO, ))
def test_stop_mode(p, panda_jungle):
  serial = p.get_usb_serial()
  logger.warning(f"=== test_stop_mode start: serial={serial}, type={p.get_type()}, spi={p.spi} ===")
  logger.warning(f"  fw: {p.get_version()}")
  panda_jungle.set_obd(True)

  for orientation in (Panda.HARNESS_STATUS_FLIPPED, Panda.HARNESS_STATUS_NORMAL):
    panda_jungle.set_harness_orientation(orientation)
    time.sleep(0.25)

    # ORIENTATION_1: SBU1=relay, SBU2=ignition (panda FLIPPED)
    # ORIENTATION_2: SBU1=ignition, SBU2=relay (panda NORMAL)
    panda_normal = (orientation == Panda.HARNESS_STATUS_FLIPPED)
    orientation_name = "NORMAL" if panda_normal else "FLIPPED"
    logger.warning(f"=== orientation={orientation} panda={orientation_name} ===")

    # ignition wakeup tests the SBU EXTI line for this orientation:
    #   FLIPPED: ignition on SBU2 (PA1) -> EXTI1
    #   NORMAL:  ignition on SBU1 (PC4) -> EXTI4
    wakeup_sources = ["ign", "0", "1", "2"]

    for wakeup in wakeup_sources:
      # ensure ignition is off before each iteration
      panda_jungle.set_ignition(False)

      logger.warning(f"--- [{orientation_name}] wakeup={wakeup} ---")
      h = p.health()
      logger.warning(f"  before stop: uptime={h['uptime']}s, voltage={h['voltage']}mV, current={h['current']}mA")
      logger.warning(f"  ignition_line={h['ignition_line']}, ignition_can={h['ignition_can']}")
      logger.warning(f"  harness={h['car_harness_status']}, sbu1={h['sbu1_voltage_mV']}mV, sbu2={h['sbu2_voltage_mV']}mV")
      logger.warning(f"  safety_mode={h['safety_mode']}, power_save={h['power_save_enabled']}, faults={h['faults']}")

      # enter stop mode
      p.enter_stop_mode()
      p.close()
      logger.warning("stop mode requested, closed connection")

      # wait for panda to enter stop mode
      time.sleep(1.5)

      # send wakeup stimulus
      t_wake = time.monotonic()
      logger.warning(f"  sending wakeup: {wakeup}")
      if wakeup == "ign":
        panda_jungle.set_ignition(True)
        exti = "EXTI1 (PA1/SBU2)" if not panda_normal else "EXTI4 (PC4/SBU1)"
        logger.warning(f"  ignition -> {exti}")
      else:
        panda_jungle.can_send(0x123, b'\x01\x02', int(wakeup))
        logger.warning(f"  CAN bus {wakeup}")

      # panda should reset and come back
      found = Panda.wait_for_panda(serial, timeout=10)
      t_found = time.monotonic() - t_wake
      logger.warning(f"  wait_for_panda returned {found} after {t_found:.2f}s")
      assert found, f"panda didn't come back for wakeup={wakeup} orientation={orientation_name} after {t_found:.1f}s"

      p.reconnect()
      h = p.health()
      logger.warning(f"  after wakeup: uptime={h['uptime']}s, voltage={h['voltage']}mV, current={h['current']}mA")
      logger.warning(f"  ignition_line={h['ignition_line']}, ignition_can={h['ignition_can']}")
      logger.warning(f"  harness={h['car_harness_status']}, sbu1={h['sbu1_voltage_mV']}mV, sbu2={h['sbu2_voltage_mV']}mV")
      logger.warning(f"  safety_mode={h['safety_mode']}, power_save={h['power_save_enabled']}, faults={h['faults']}")
      assert h['uptime'] < 3, f"uptime {h['uptime']}s too high for wakeup={wakeup} orientation={orientation_name}"
      logger.warning(f"  PASS: {wakeup} uptime={h['uptime']}s")
