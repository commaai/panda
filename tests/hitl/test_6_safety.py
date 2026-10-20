import time

from opendbc.car.structs import CarParams
from panda.tests.hitl.base import PandaTestCase, panda_test
from panda.tests.hitl.helpers import clear_can_buffers

# board/sys/sys.h PERMANENT_FAULTS
PERMANENT_FAULT_BITS = (1, 2, 3, 4, 7, 8, 9, 10, 11, 12, 15, 16, 17, 21)
# board/drivers/self_test.h injection index: fault bit
SELF_TEST_FAULT_BITS = {0: 9, 1: 10, 2: 11}
FAULT_CLOCK_BIT = 16
FAULT_INTERRUPT_MISSING_BIT = 17
FAULT_SIREN_MALFUNCTION_BIT = 25
FAULT_INTERRUPT_RATE_CAN_1_BIT = 2


class TestSafety(PandaTestCase):
  def test_safety_nooutput(self):
    p = self.p
    p.set_safety_mode(CarParams.SafetyModel.silent)
    p.set_can_loopback(True)

    # send a message on bus 0
    p.can_send(0x1aa, b"message", 0)

    # confirm receive nothing
    time.sleep(0.05)
    r = p.can_recv()
    # bus 192 is messages blocked by TX safety hook on bus 0
    assert len([x for x in r if x[2] != 192]) == 0
    assert len([x for x in r if x[2] == 192]) == 1

  def test_fault_safe_state(self):
    p = self.p
    for fault_bit in PERMANENT_FAULT_BITS:
      p.reset()
      p.set_safety_mode(CarParams.SafetyModel.allOutput)
      assert p.health()['safety_mode'] == CarParams.SafetyModel.allOutput

      t = time.monotonic()
      p.inject_fault(fault_bit)
      h = p.health()
      print(f"fault bit {fault_bit}: safe state after {(time.monotonic() - t) * 1e3:.1f} ms")
      assert h['faults'] & (1 << fault_bit)
      assert h['fault_status'] == 2
      assert h['safety_mode'] == CarParams.SafetyModel.silent
      assert not h['controls_allowed']

      p.set_safety_mode(CarParams.SafetyModel.allOutput)
      assert p.health()['safety_mode'] == CarParams.SafetyModel.silent
    p.reset()

  def test_temporary_fault_no_safe_state(self):
    p = self.p
    p.set_safety_mode(CarParams.SafetyModel.allOutput)
    p.inject_fault(FAULT_SIREN_MALFUNCTION_BIT)
    h = p.health()
    assert h['fault_status'] == 1
    assert h['safety_mode'] == CarParams.SafetyModel.allOutput
    p.reset()

  def test_hardfault_resets_to_silent(self):
    p = self.p
    p.set_safety_mode(CarParams.SafetyModel.allOutput)
    p.inject_hardfault()
    p.reconnect()
    h = p.health()
    assert h['hardfault_reset']
    assert h['safety_mode'] == CarParams.SafetyModel.silent
    assert not h['controls_allowed']

  def test_mpu_fault_resets_to_silent(self):
    p = self.p
    p.set_safety_mode(CarParams.SafetyModel.allOutput)
    p.inject_mpu_fault()
    p.reconnect()
    h = p.health()
    assert h['hardfault_reset']
    assert h['safety_mode'] == CarParams.SafetyModel.silent
    assert not h['controls_allowed']

  def test_stack_overflow_resets_to_silent(self):
    p = self.p
    p.set_safety_mode(CarParams.SafetyModel.allOutput)
    p.inject_stack_overflow()
    p.reconnect()
    h = p.health()
    assert h['hardfault_reset']
    assert h['safety_mode'] == CarParams.SafetyModel.silent
    assert not h['controls_allowed']

  def test_nmi_resets_to_silent(self):
    p = self.p
    p.set_safety_mode(CarParams.SafetyModel.allOutput)
    p.inject_nmi()
    p.reconnect()
    h = p.health()
    assert h['nmi_reset']
    assert h['safety_mode'] == CarParams.SafetyModel.silent
    assert not h['controls_allowed']

  @panda_test(needs_jungle=True)
  def test_fault_stops_tx(self):
    p = self.p
    jungle = self.panda_jungle
    p.set_safety_mode(CarParams.SafetyModel.allOutput)
    clear_can_buffers(p)
    clear_can_buffers(jungle)

    p.can_send(0x1aa, b"before", 0)
    time.sleep(0.05)
    assert (0x1aa, b"before", 0) in jungle.can_recv()

    p.inject_fault(FAULT_INTERRUPT_RATE_CAN_1_BIT)
    p.can_send(0x1aa, b"after", 0)
    time.sleep(0.05)
    assert all(addr != 0x1aa for addr, _, _ in jungle.can_recv())
    p.reset()

  @panda_test(timeout=2*60)
  def test_self_test_failure(self):
    p = self.p
    for test, fault_bit in SELF_TEST_FAULT_BITS.items():
      p.reset()
      p.set_safety_mode(CarParams.SafetyModel.allOutput)
      p.inject_self_test_failure(test)

      # heartbeat keeps allOutput, so only the fault can set SILENT
      t = time.monotonic()
      while not (p.health()['faults'] & (1 << fault_bit)) and (time.monotonic() - t) < 15:
        p.send_heartbeat(engaged=False)
        time.sleep(0.1)
      print(f"self-test {test}: fault after {time.monotonic() - t:.1f} s")
      h = p.health()
      assert h['faults'] & (1 << fault_bit)
      assert h['fault_status'] == 2
      assert h['safety_mode'] == CarParams.SafetyModel.silent
    p.reset()

  def test_self_test_no_fault(self):
    p = self.p
    p.set_safety_mode(CarParams.SafetyModel.allOutput)
    for _ in range(60):
      p.send_heartbeat(engaged=False)
      time.sleep(0.2)
    h = p.health()
    assert h['faults'] == 0, f"faults {h['faults']:#x}"
    assert h['safety_mode'] == CarParams.SafetyModel.allOutput

  def test_tick_stopped(self):
    p = self.p
    p.set_safety_mode(CarParams.SafetyModel.allOutput)
    time.sleep(2.5)
    p.stop_tick()
    t = time.monotonic()
    while not ((h := p.health())['faults'] & (1 << FAULT_INTERRUPT_MISSING_BIT)) and (time.monotonic() - t) < 3:
      time.sleep(0.05)
    print(f"tick stopped: fault after {(time.monotonic() - t) * 1e3:.0f} ms")
    assert h['faults'] & (1 << FAULT_INTERRUPT_MISSING_BIT)
    assert h['safety_mode'] == CarParams.SafetyModel.silent
    p.reset()

  def test_clock_check(self):
    p = self.p
    p.set_safety_mode(CarParams.SafetyModel.allOutput)
    p.set_clock_check_prescaler(1)
    t = time.monotonic()
    while not ((h := p.health())['faults'] & (1 << FAULT_CLOCK_BIT)) and (time.monotonic() - t) < 3:
      time.sleep(0.05)
    assert h['faults'] & (1 << FAULT_CLOCK_BIT)
    assert h['safety_mode'] == CarParams.SafetyModel.silent
    p.reset()
