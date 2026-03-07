import time
import pytest

from opendbc.car.structs import CarParams
from panda import Panda
from panda.python.spi import PandaSpiException
from panda.tests.hitl.helpers import clear_can_buffers

SPEED_NORMAL = 500
SPEED_MISMATCH = 250


def _reset_speeds(p, panda_jungle):
  """Restore matching CAN speeds and clear buffers."""
  for bus in range(3):
    panda_jungle.set_can_speed_kbps(bus, SPEED_NORMAL)
  try:
    p.reset(reconnect=True)
    for bus in range(3):
      p.set_can_speed_kbps(bus, SPEED_NORMAL)
    clear_can_buffers(p)
  except Exception:
    p.reconnect()


def _health_check(p):
  """Get health, return None if SPI fails (lockup detected)."""
  try:
    return p.health()
  except PandaSpiException:
    return None


def _poll_health_during_errors(p, panda_jungle, duration, send_from_panda=False):
  """Send CAN at mismatched speed while polling health. Returns stats."""
  msg = b"\xaa" * 8
  start = time.monotonic()
  health_count = 0
  spi_failures = 0
  gaps = []
  last_response = start

  while time.monotonic() - start < duration:
    for bus in range(3):
      if send_from_panda:
        try:
          p.can_send(0x456, msg, bus)
        except PandaSpiException:
          spi_failures += 1
          continue
      else:
        panda_jungle.can_send(0x123, msg, bus)

    h = _health_check(p)
    now = time.monotonic()
    gap = now - last_response
    gaps.append(gap)
    last_response = now

    if h is not None:
      health_count += 1
    else:
      spi_failures += 1

  max_gap = max(gaps) if gaps else 0
  avg_gap = (sum(gaps) / len(gaps)) if gaps else 0
  p95_idx = int(len(gaps) * 0.95) if gaps else 0
  p95_gap = sorted(gaps)[p95_idx] if gaps else 0

  return {
    'health_count': health_count,
    'spi_failures': spi_failures,
    'max_gap': max_gap,
    'avg_gap': avg_gap,
    'p95_gap': p95_gap,
    'total_polls': health_count + spi_failures,
  }


def _print_stats(stats):
  mg = stats['max_gap'] * 1000
  ag = stats['avg_gap'] * 1000
  p95 = stats['p95_gap'] * 1000
  print(f"health={stats['health_count']} spi_fail={stats['spi_failures']} max_gap={mg:.1f}ms avg={ag:.1f}ms p95={p95:.1f}ms")


def _print_can_health(p):
  for bus in range(3):
    ch = p.can_health(bus)
    print(f"  bus {bus}: errs={ch['total_error_cnt']} irq0={ch['irq0_call_rate']} busoff={ch['bus_off_cnt']} resets={ch['can_core_reset_count']}")


@pytest.mark.panda_expect_can_error
@pytest.mark.timeout(60)
class TestCanErrorResilience:
  """Verify panda stays responsive over SPI during CAN error conditions."""

  def test_spi_responsive_during_can_errors(self, p, panda_jungle):
    """Speed mismatch causes CAN error interrupts; SPI must stay responsive."""
    p.set_safety_mode(CarParams.SafetyModel.allOutput)

    for bus in range(3):
      p.set_can_speed_kbps(bus, SPEED_NORMAL)
      panda_jungle.set_can_speed_kbps(bus, SPEED_MISMATCH)

    stats = _poll_health_during_errors(p, panda_jungle, duration=8.0)
    _print_stats(stats)

    if stats['spi_failures'] == 0:
      _print_can_health(p)

    assert stats['spi_failures'] == 0, f"SPI failed {stats['spi_failures']} times (panda locked up)"
    assert stats['max_gap'] < 0.250, f"SPI gap too large: {stats['max_gap']*1000:.1f}ms"

  def test_spi_responsive_during_bus_off(self, p, panda_jungle):
    """TX with no ACK -> bus-off -> must not block SPI."""
    p.set_safety_mode(CarParams.SafetyModel.allOutput)

    for bus in range(3):
      p.set_can_speed_kbps(bus, SPEED_NORMAL)
      panda_jungle.set_can_speed_kbps(bus, SPEED_MISMATCH)

    stats = _poll_health_during_errors(p, panda_jungle, duration=5.0, send_from_panda=True)
    _print_stats(stats)

    if stats['spi_failures'] == 0:
      _print_can_health(p)

    assert stats['spi_failures'] == 0, f"SPI failed {stats['spi_failures']} times (panda locked up)"
    assert stats['max_gap'] < 0.250, f"SPI gap too large: {stats['max_gap']*1000:.1f}ms"

  def test_sustained_error_storm(self, p, panda_jungle):
    """Sustained CAN errors for 15s must not degrade SPI responsiveness."""
    p.set_safety_mode(CarParams.SafetyModel.allOutput)

    for bus in range(3):
      p.set_can_speed_kbps(bus, SPEED_NORMAL)
      panda_jungle.set_can_speed_kbps(bus, SPEED_MISMATCH)

    stats = _poll_health_during_errors(p, panda_jungle, duration=15.0)
    _print_stats(stats)

    assert stats['spi_failures'] == 0, f"SPI failed {stats['spi_failures']} times"
    assert stats['max_gap'] < 0.250, f"SPI max gap: {stats['max_gap']*1000:.1f}ms"
    assert stats['health_count'] > 100, f"Too few responses: {stats['health_count']}"

  def test_can_recovery_after_errors(self, p, panda_jungle):
    """After CAN errors, normal communication must resume."""
    p.set_safety_mode(CarParams.SafetyModel.allOutput)

    for bus in range(3):
      p.set_can_speed_kbps(bus, SPEED_NORMAL)
      panda_jungle.set_can_speed_kbps(bus, SPEED_MISMATCH)

    msg = b"\xdd" * 8
    for _ in range(100):
      for bus in range(3):
        panda_jungle.can_send(0xabc, msg, bus)
    time.sleep(1.0)

    _reset_speeds(p, panda_jungle)
    time.sleep(0.5)

    test_msg = b"\xee" * 8
    for bus in range(3):
      panda_jungle.can_send(0x100, test_msg, bus)
    time.sleep(0.5)

    msgs = p.can_recv()
    buses_received = {m[2] for m in msgs if m[0] == 0x100}
    print(f"Received on buses: {buses_received}")
    assert len(buses_received) == 3, f"CAN didn't recover on all buses, only got: {buses_received}"

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

    time.sleep(2.0)

    h = _health_check(p)
    assert h is not None, "Panda unresponsive after CAN errors"
    print(f"faults: 0x{h['faults']:x}, interrupt_load: {h['interrupt_load']}")
    for bus in range(3):
      ch = p.can_health(bus)
      print(f"  bus {bus}: irq0_rate={ch['irq0_call_rate']}, irq1_rate={ch['irq1_call_rate']}")

    assert h['faults'] == 0, f"Faults during CAN errors: 0x{h['faults']:x}"
