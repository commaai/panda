import time

from opendbc.car.structs import CarParams
from panda.tests.hitl.base import PandaTestCase, panda_test

# keep in sync with PERMANENT_FAULTS, see board/sys/sys.h
PERMANENT_FAULT_BITS = ()
# board/drivers/self_test.h injection index: fault bit
SELF_TEST_FAULT_BITS = {0: 9, 1: 10, 2: 11, 3: 12, 4: 12}
# board/main_comms.h 0xef injection index: fault bit
INTERRUPT_FAULT_BITS = {0: 17, 1: 17, 2: 1, 3: 16}
FAULT_SIREN_MALFUNCTION_BIT = 25
# one runtime RAM pass: DTCM, AXI SRAM and ITCM in 248 B steps at 8 Hz, see board/drivers/self_test.h
RAM_TEST_PASS_S = 0x80000 / 248 / 8
FAULT_MPU_BIT = 28


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

  def test_mpu_fault(self):
    p = self.p
    p.set_safety_mode(CarParams.SafetyModel.allOutput)
    p.inject_mpu_fault()
    if FAULT_MPU_BIT in PERMANENT_FAULT_BITS:
      p.reconnect()
      h = p.health()
      assert h['hardfault_reset']
      assert h['safety_mode'] == CarParams.SafetyModel.silent
    else:
      # flagged at the next safety hook, raised by mpu_check() at 1 Hz
      time.sleep(2.5)
      h = p.health()
      assert h['faults'] & (1 << FAULT_MPU_BIT)
      assert h['safety_mode'] == CarParams.SafetyModel.allOutput
    p.reset()

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

  @panda_test(timeout=2*60)
  def test_self_test_failure(self):
    p = self.p
    for test, fault_bit in SELF_TEST_FAULT_BITS.items():
      p.reset()
      p.set_safety_mode(CarParams.SafetyModel.allOutput)
      assert not (p.health()['faults'] & (1 << fault_bit))
      p.inject_self_test_failure(test)

      # heartbeat keeps allOutput, so only the fault can set SILENT
      t = time.monotonic()
      while not (p.health()['faults'] & (1 << fault_bit)) and (time.monotonic() - t) < 30:
        p.send_heartbeat(engaged=False)
        time.sleep(0.1)
      print(f"self-test {test}: fault after {time.monotonic() - t:.1f} s")
      h = p.health()
      assert h['faults'] & (1 << fault_bit)
      if fault_bit in PERMANENT_FAULT_BITS:
        assert h['safety_mode'] == CarParams.SafetyModel.silent
      else:
        assert h['safety_mode'] == CarParams.SafetyModel.allOutput
    p.reset()

  @panda_test(timeout=6*60)
  def test_self_test_no_fault(self):
    p = self.p
    p.set_safety_mode(CarParams.SafetyModel.allOutput)
    t = time.monotonic()
    while (time.monotonic() - t) < (RAM_TEST_PASS_S + 5):
      p.send_heartbeat(engaged=False)
      time.sleep(0.2)
    h = p.health()
    assert h['faults'] == 0, f"faults {h['faults']:#x}"
    assert h['safety_mode'] == CarParams.SafetyModel.allOutput

  @panda_test(timeout=2*60)
  def test_interrupt_fault(self):
    p = self.p
    for test, fault_bit in INTERRUPT_FAULT_BITS.items():
      p.reset()
      p.set_safety_mode(CarParams.SafetyModel.allOutput)
      # the missing interrupt check skips the first window
      time.sleep(2.5)
      h = p.health()
      assert h['faults'] == 0, f"faults {h['faults']:#x}"
      p.inject_interrupt_fault(test)

      t = time.monotonic()
      while not ((h := p.health())['faults'] & (1 << fault_bit)) and (time.monotonic() - t) < 5:
        time.sleep(0.05)
      print(f"interrupt fault {test}: fault after {time.monotonic() - t:.1f} s")
      assert h['faults'] & (1 << fault_bit)
      if fault_bit in PERMANENT_FAULT_BITS:
        assert h['safety_mode'] == CarParams.SafetyModel.silent
      else:
        assert h['safety_mode'] == CarParams.SafetyModel.allOutput
    p.reset()
