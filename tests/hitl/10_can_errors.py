import time
import pytest

from opendbc.car.structs import CarParams
from panda.tests.hitl.helpers import clear_can_buffers

SPEED_NORMAL = 500
SPEED_MISMATCH = 250


def _assert_spi_responsive(p, duration=5.0, interval=0.05):
  """Poll health over SPI for `duration` seconds, assert we get responses."""
  start = time.monotonic()
  count = 0
  max_gap = 0.0
  last_response = start
  while time.monotonic() - start < duration:
    h = p.health()
    assert h['uptime'] > 0
    now = time.monotonic()
    gap = now - last_response
    if gap > max_gap:
      max_gap = gap
    last_response = now
    count += 1
    time.sleep(interval)
  expected = int(duration / interval)
  return count, max_gap, expected


def _flood_jungle_can(panda_jungle, duration=3.0, buses=(0, 1, 2)):
  """Send CAN messages from jungle continuously for `duration` seconds."""
  msg = b"\xaa" * 8
  start = time.monotonic()
  sent = 0
  while time.monotonic() - start < duration:
    for bus in buses:
      panda_jungle.can_send(0x123, msg, bus)
      sent += 1
  return sent


@pytest.mark.panda_expect_can_error
@pytest.mark.timeout(120)
class TestCanErrorResilience:
  """Verify panda stays responsive over SPI during CAN error conditions."""

  def test_spi_responsive_during_can_errors(self, p, panda_jungle):
    """Speed mismatch causes CAN error interrupts; SPI must stay responsive."""
    p.set_safety_mode(CarParams.SafetyModel.allOutput)

    # Set panda to 500kbps, jungle to 250kbps -> every frame is a CAN error
    for bus in range(3):
      p.set_can_speed_kbps(bus, SPEED_NORMAL)
      panda_jungle.set_can_speed_kbps(bus, SPEED_MISMATCH)

    # Flood from jungle while polling panda health
    msg = b"\xaa" * 8
    start = time.monotonic()
    health_count = 0
    max_gap = 0.0
    last_response = start

    while time.monotonic() - start < 8.0:
      # send burst from jungle
      for bus in range(3):
        panda_jungle.can_send(0x123, msg, bus)

      # check panda responsiveness
      h = p.health()
      assert h['uptime'] > 0
      now = time.monotonic()
      gap = now - last_response
      if gap > max_gap:
        max_gap = gap
      last_response = now
      health_count += 1

    print(f"health responses: {health_count}, max gap: {max_gap*1000:.1f}ms")

    # verify errors actually occurred
    for bus in range(3):
      ch = p.can_health(bus)
      print(f"  bus {bus}: total_error_cnt={ch['total_error_cnt']}, "
            f"irq0_rate={ch['irq0_call_rate']}, "
            f"bus_off_cnt={ch['bus_off_cnt']}, "
            f"core_resets={ch['can_core_reset_count']}")
      assert ch['total_error_cnt'] > 0, f"Bus {bus}: expected CAN errors"

    # SPI should not have had long gaps (>250ms would indicate lockup)
    assert max_gap < 0.250, f"SPI gap too large: {max_gap*1000:.1f}ms (lockup?)"

  def test_spi_responsive_during_bus_off(self, p, panda_jungle):
    """TX with no ACK -> bus-off -> must not block SPI."""
    p.set_safety_mode(CarParams.SafetyModel.allOutput)

    # Mismatch speeds so jungle can't ACK panda's TX
    for bus in range(3):
      p.set_can_speed_kbps(bus, SPEED_NORMAL)
      panda_jungle.set_can_speed_kbps(bus, SPEED_MISMATCH)

    # Send from panda into the void -- TEC rises, eventually bus-off
    msg = b"\xbb" * 8
    for _ in range(300):
      for bus in range(3):
        p.can_send(0x456, msg, bus)
      time.sleep(0.001)

    # Panda must still respond
    count, max_gap, expected = _assert_spi_responsive(p, duration=5.0)
    print(f"health responses: {count}/{expected}, max gap: {max_gap*1000:.1f}ms")

    for bus in range(3):
      ch = p.can_health(bus)
      print(f"  bus {bus}: bus_off_cnt={ch['bus_off_cnt']}, "
            f"tec={ch['transmit_error_cnt']}, "
            f"core_resets={ch['can_core_reset_count']}")

    assert max_gap < 0.250, f"SPI gap too large: {max_gap*1000:.1f}ms (lockup?)"

  def test_sustained_error_storm(self, p, panda_jungle):
    """Sustained CAN errors for 15s must not degrade SPI responsiveness."""
    p.set_safety_mode(CarParams.SafetyModel.allOutput)

    for bus in range(3):
      p.set_can_speed_kbps(bus, SPEED_NORMAL)
      panda_jungle.set_can_speed_kbps(bus, SPEED_MISMATCH)

    msg = b"\xcc" * 8
    start = time.monotonic()
    health_count = 0
    gaps = []
    last_response = start

    while time.monotonic() - start < 15.0:
      for bus in range(3):
        panda_jungle.can_send(0x789, msg, bus)

      h = p.health()
      assert h['uptime'] > 0
      now = time.monotonic()
      gaps.append(now - last_response)
      last_response = now
      health_count += 1

    max_gap = max(gaps)
    avg_gap = sum(gaps) / len(gaps)
    p95_gap = sorted(gaps)[int(len(gaps) * 0.95)]
    print(f"health responses: {health_count}, "
          f"avg gap: {avg_gap*1000:.1f}ms, "
          f"p95 gap: {p95_gap*1000:.1f}ms, "
          f"max gap: {max_gap*1000:.1f}ms")

    assert max_gap < 0.250, f"SPI max gap: {max_gap*1000:.1f}ms (lockup?)"
    assert health_count > 100, f"Too few responses: {health_count}"

  def test_can_recovery_after_errors(self, p, panda_jungle):
    """After CAN errors, normal communication must resume."""
    p.set_safety_mode(CarParams.SafetyModel.allOutput)

    # Phase 1: induce errors
    for bus in range(3):
      p.set_can_speed_kbps(bus, SPEED_NORMAL)
      panda_jungle.set_can_speed_kbps(bus, SPEED_MISMATCH)

    msg = b"\xdd" * 8
    for _ in range(100):
      for bus in range(3):
        panda_jungle.can_send(0xabc, msg, bus)
    time.sleep(1.0)

    # Phase 2: fix speeds, verify normal CAN works
    clear_can_buffers(p, speed=SPEED_NORMAL)
    clear_can_buffers(panda_jungle, speed=SPEED_NORMAL)
    time.sleep(0.5)

    test_msg = b"\xee" * 8
    for bus in range(3):
      panda_jungle.can_send(0x100, test_msg, bus)
    time.sleep(0.5)

    msgs = p.can_recv()
    buses_received = {m[2] for m in msgs if m[0] == 0x100}
    print(f"Received on buses: {buses_received}")
    assert len(buses_received) == 3, \
      f"CAN didn't recover on all buses, only got: {buses_received}"

  def test_no_faults_during_errors(self, p, panda_jungle):
    """CAN errors should not trigger interrupt rate faults."""
    p.set_safety_mode(CarParams.SafetyModel.allOutput)

    for bus in range(3):
      p.set_can_speed_kbps(bus, SPEED_NORMAL)
      panda_jungle.set_can_speed_kbps(bus, SPEED_MISMATCH)

    msg = b"\xff" * 8
    for _ in range(200):
      for bus in range(3):
        panda_jungle.can_send(0x555, msg, bus)
      time.sleep(0.001)

    # wait for rate counters to update (1s timer)
    time.sleep(2.0)

    h = p.health()
    print(f"faults: 0x{h['faults']:x}, interrupt_load: {h['interrupt_load']}")
    for bus in range(3):
      ch = p.can_health(bus)
      print(f"  bus {bus}: irq0_rate={ch['irq0_call_rate']}, "
            f"irq1_rate={ch['irq1_call_rate']}")

    assert h['faults'] == 0, f"Faults during CAN errors: 0x{h['faults']:x}"
