import time

from opendbc.car.structs import CarParams
from panda.tests.hitl.base import PandaTestCase

# keep in sync with PERMANENT_FAULTS, see board/sys/sys.h
PERMANENT_FAULT_BITS = ()
FAULT_SIREN_MALFUNCTION_BIT = 25
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
