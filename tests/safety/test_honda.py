#!/usr/bin/env python3
import unittest
import numpy as np
from panda import Panda
from panda.tests.safety import libpandasafety_py
import panda.tests.safety.common as common
from panda.tests.safety.common import make_msg, MAX_WRONG_COUNTERS, UNSAFE_MODE

MAX_BRAKE = 255

INTERCEPTOR_THRESHOLD = 344

class Btn:
  CANCEL = 2
  SET = 3
  RESUME = 4

HONDA_N_HW = 0
HONDA_BG_HW = 1
HONDA_BH_HW = 2

# Honda gas gains are the different
def honda_interceptor_msg(gas, addr):
  to_send = make_msg(0, addr, 6)
  gas2 = gas * 2
  to_send[0].RDLR = ((gas & 0xff) << 8) | ((gas & 0xff00) >> 8) | \
                    ((gas2 & 0xff) << 24) | ((gas2 & 0xff00) << 8)
  return to_send

def honda_checksum(msg, addr, len_msg):
  checksum = 0
  while addr > 0:
    checksum += addr
    addr >>= 4
  for i in range (0, 2*len_msg):
    if i < 8:
      checksum += (msg.RDLR >> (4 * i))
    else:
      checksum += (msg.RDHR >> (4 * (i - 8)))
  return (8 - checksum) & 0xF


class TestHondaSafety(common.PandaSafetyTest):
  cnt_speed = 0
  cnt_gas = 0
  cnt_button = 0

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "TestHondaSafety":
      cls.packer = None
      cls.safety = None
      raise unittest.SkipTest

  # override these inherited tests. honda doesn't use pcm enable
  def test_disable_control_allowed_from_cruise(self): pass
  def test_enable_control_allowed_from_cruise(self): pass

  def _speed_msg(self, speed):
    bus = 1 if self.safety.get_honda_hw() == HONDA_BH_HW else 0
    to_send = make_msg(bus, 0x158)
    to_send[0].RDLR = speed
    to_send[0].RDHR |= (self.cnt_speed % 4) << 28
    to_send[0].RDHR |= honda_checksum(to_send[0], 0x158, 8) << 24
    self.__class__.cnt_speed += 1
    return to_send

  def _button_msg(self, buttons, addr):
    bus = 1 if self.safety.get_honda_hw() == HONDA_BH_HW else 0
    to_send = make_msg(bus, addr)
    to_send[0].RDLR = buttons << 5
    to_send[0].RDHR |= (self.cnt_button % 4) << 28
    to_send[0].RDHR |= honda_checksum(to_send[0], addr, 8) << 24
    self.__class__.cnt_button += 1
    return to_send

  def _brake_msg(self, brake):
    bus = 1 if self.safety.get_honda_hw() == HONDA_BH_HW else 0
    to_send = make_msg(bus, 0x17C)
    to_send[0].RDHR = 0x200000 if brake else 0
    to_send[0].RDHR |= (self.cnt_gas % 4) << 28
    to_send[0].RDHR |= honda_checksum(to_send[0], 0x17C, 8) << 24
    self.__class__.cnt_gas += 1
    return to_send

  def _alt_brake_msg(self, brake):
    to_send = make_msg(0, 0x1BE)
    to_send[0].RDLR = 0x10 if brake else 0
    return to_send

  def _gas_msg(self, gas):
    bus = 1 if self.safety.get_honda_hw() == HONDA_BH_HW else 0
    to_send = make_msg(bus, 0x17C)
    to_send[0].RDLR = 1 if gas else 0
    to_send[0].RDHR |= (self.cnt_gas % 4) << 28
    to_send[0].RDHR |= honda_checksum(to_send[0], 0x17C, 8) << 24
    self.__class__.cnt_gas += 1
    return to_send

  def _send_brake_msg(self, brake):
    to_send = make_msg(0, 0x1FA)
    to_send[0].RDLR = ((brake & 0x3) << 14) | ((brake & 0x3FF) >> 2)
    return to_send

  def _send_steer_msg(self, steer):
    bus = 2 if self.safety.get_honda_hw() == HONDA_BG_HW else 0
    to_send = make_msg(bus, 0xE4, 6)
    to_send[0].RDLR = steer
    return to_send

  def test_resume_button(self):
    self.safety.set_controls_allowed(0)
    self.safety.safety_rx_hook(self._button_msg(Btn.RESUME, 0x296))
    self.assertTrue(self.safety.get_controls_allowed())

  def test_set_button(self):
    self.safety.set_controls_allowed(0)
    self.safety.safety_rx_hook(self._button_msg(Btn.SET, 0x296))
    self.assertTrue(self.safety.get_controls_allowed())

  def test_cancel_button(self):
    self.safety.set_controls_allowed(1)
    self.safety.safety_rx_hook(self._button_msg(Btn.CANCEL, 0x296))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_sample_speed(self):
    self.assertEqual(0, self.safety.get_honda_moving())
    self.safety.safety_rx_hook(self._speed_msg(100))
    self.assertEqual(1, self.safety.get_honda_moving())

  def test_prev_brake(self):
    self.assertFalse(self.safety.get_brake_pressed_prev())
    self.safety.safety_rx_hook(self._brake_msg(True))
    self.assertTrue(self.safety.get_brake_pressed_prev())

  def test_disengage_on_brake(self):
    self.safety.set_controls_allowed(1)
    self.safety.safety_rx_hook(self._brake_msg(1))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_alt_disengage_on_brake(self):
    self.safety.set_honda_alt_brake_msg(1)
    self.safety.set_controls_allowed(1)
    self.safety.safety_rx_hook(self._alt_brake_msg(1))
    self.assertFalse(self.safety.get_controls_allowed())

    self.safety.set_honda_alt_brake_msg(0)
    self.safety.set_controls_allowed(1)
    self.safety.safety_rx_hook(self._alt_brake_msg(1))
    self.assertTrue(self.safety.get_controls_allowed())

  def test_prev_gas(self):
    self.safety.safety_rx_hook(self._gas_msg(False))
    self.assertFalse(self.safety.get_gas_pressed_prev())
    self.safety.safety_rx_hook(self._gas_msg(True))
    self.assertTrue(self.safety.get_gas_pressed_prev())

  def test_prev_gas_interceptor(self):
    self.safety.safety_rx_hook(honda_interceptor_msg(0x0, 0x201))
    self.assertFalse(self.safety.get_gas_interceptor_prev())
    self.safety.safety_rx_hook(honda_interceptor_msg(0x1000, 0x201))
    self.assertTrue(self.safety.get_gas_interceptor_prev())
    self.safety.safety_rx_hook(honda_interceptor_msg(0x0, 0x201))
    self.safety.set_gas_interceptor_detected(False)

  def test_disengage_on_gas(self):
    self.safety.safety_rx_hook(self._gas_msg(0))
    self.safety.set_controls_allowed(1)
    self.safety.safety_rx_hook(self._gas_msg(1))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_unsafe_mode_no_disengage_on_gas(self):
    self.safety.safety_rx_hook(self._gas_msg(0))
    self.safety.set_controls_allowed(1)
    self.safety.set_unsafe_mode(UNSAFE_MODE.DISABLE_DISENGAGE_ON_GAS)
    self.safety.safety_rx_hook(self._gas_msg(1))
    self.assertTrue(self.safety.get_controls_allowed())
    self.safety.set_unsafe_mode(UNSAFE_MODE.DEFAULT)

  def test_allow_engage_with_gas_pressed(self):
    self.safety.safety_rx_hook(self._gas_msg(1))
    self.safety.set_controls_allowed(1)
    self.safety.safety_rx_hook(self._gas_msg(1))
    self.assertTrue(self.safety.get_controls_allowed())

  def test_disengage_on_gas_interceptor(self):
    for g in range(0, 0x1000):
      self.safety.safety_rx_hook(honda_interceptor_msg(0, 0x201))
      self.safety.set_controls_allowed(True)
      self.safety.safety_rx_hook(honda_interceptor_msg(g, 0x201))
      remain_enabled = g <= INTERCEPTOR_THRESHOLD
      self.assertEqual(remain_enabled, self.safety.get_controls_allowed())
      self.safety.safety_rx_hook(honda_interceptor_msg(0, 0x201))
      self.safety.set_gas_interceptor_detected(False)

  def test_unsafe_mode_no_disengage_on_gas_interceptor(self):
    self.safety.set_controls_allowed(True)
    self.safety.set_unsafe_mode(UNSAFE_MODE.DISABLE_DISENGAGE_ON_GAS)
    for g in range(0, 0x1000):
      self.safety.safety_rx_hook(honda_interceptor_msg(g, 0x201))
      self.assertTrue(self.safety.get_controls_allowed())
      self.safety.safety_rx_hook(honda_interceptor_msg(0, 0x201))
      self.safety.set_gas_interceptor_detected(False)
    self.safety.set_unsafe_mode(UNSAFE_MODE.DEFAULT)
    self.safety.set_controls_allowed(False)

  def test_allow_engage_with_gas_interceptor_pressed(self):
    self.safety.safety_rx_hook(honda_interceptor_msg(0x1000, 0x201))
    self.safety.set_controls_allowed(1)
    self.safety.safety_rx_hook(honda_interceptor_msg(0x1000, 0x201))
    self.assertTrue(self.safety.get_controls_allowed())
    self.safety.safety_rx_hook(honda_interceptor_msg(0, 0x201))
    self.safety.set_gas_interceptor_detected(False)

  def test_brake_safety_check(self):
    hw = self.safety.get_honda_hw()
    if hw == HONDA_N_HW:
      for fwd_brake in [False, True]:
        self.safety.set_honda_fwd_brake(fwd_brake)
        for brake in np.arange(0, MAX_BRAKE + 10, 1):
          for controls_allowed in [True, False]:
            self.safety.set_controls_allowed(controls_allowed)
            if fwd_brake:
              send = False  # block openpilot brake msg when fwd'ing stock msg
            elif controls_allowed:
              send = MAX_BRAKE >= brake >= 0
            else:
              send = brake == 0
            self.assertEqual(send, self.safety.safety_tx_hook(self._send_brake_msg(brake)))
      self.safety.set_honda_fwd_brake(False)

  def test_gas_interceptor_safety_check(self):
    if self.safety.get_honda_hw() == HONDA_N_HW:
      for gas in np.arange(0, 4000, 100):
        for controls_allowed in [True, False]:
          self.safety.set_controls_allowed(controls_allowed)
          if controls_allowed:
            send = True
          else:
            send = gas == 0
          self.assertEqual(send, self.safety.safety_tx_hook(honda_interceptor_msg(gas, 0x200)))

  def test_steer_safety_check(self):
    self.safety.set_controls_allowed(0)
    self.assertTrue(self.safety.safety_tx_hook(self._send_steer_msg(0x0000)))
    self.assertFalse(self.safety.safety_tx_hook(self._send_steer_msg(0x1000)))

  def test_spam_cancel_safety_check(self):
    hw = self.safety.get_honda_hw()
    if hw != HONDA_N_HW:
      BUTTON_MSG = 0x296
      self.safety.set_controls_allowed(0)
      self.assertTrue(self.safety.safety_tx_hook(self._button_msg(Btn.CANCEL, BUTTON_MSG)))
      self.assertFalse(self.safety.safety_tx_hook(self._button_msg(Btn.RESUME, BUTTON_MSG)))
      self.assertFalse(self.safety.safety_tx_hook(self._button_msg(Btn.SET, BUTTON_MSG)))
      # do not block resume if we are engaged already
      self.safety.set_controls_allowed(1)
      self.assertTrue(self.safety.safety_tx_hook(self._button_msg(Btn.RESUME, BUTTON_MSG)))

  def test_rx_hook(self):

    # checksum checks
    for msg in ["btn1", "btn2", "gas", "speed"]:
      self.safety.set_controls_allowed(1)
      if msg == "btn1":
        if self.safety.get_honda_hw() == HONDA_N_HW:
          to_push = self._button_msg(Btn.SET, 0x1A6)  # only in Honda_NIDEC
        else:
          continue
      if msg == "btn2":
        to_push = self._button_msg(Btn.SET, 0x296)
      if msg == "gas":
        to_push = self._gas_msg(0)
      if msg == "speed":
        to_push = self._speed_msg(0)
      self.assertTrue(self.safety.safety_rx_hook(to_push))
      to_push[0].RDHR = 0  # invalidate checksum
      self.assertFalse(self.safety.safety_rx_hook(to_push))
      self.assertFalse(self.safety.get_controls_allowed())

    # counter
    # reset wrong_counters to zero by sending valid messages
    for i in range(MAX_WRONG_COUNTERS + 1):
      self.__class__.cnt_speed += 1
      self.__class__.cnt_gas += 1
      self.__class__.cnt_button += 1
      if i < MAX_WRONG_COUNTERS:
        self.safety.set_controls_allowed(1)
        self.safety.safety_rx_hook(self._button_msg(Btn.SET, 0x296))
        self.safety.safety_rx_hook(self._speed_msg(0))
        self.safety.safety_rx_hook(self._gas_msg(0))
      else:
        self.assertFalse(self.safety.safety_rx_hook(self._button_msg(Btn.SET, 0x296)))
        self.assertFalse(self.safety.safety_rx_hook(self._speed_msg(0)))
        self.assertFalse(self.safety.safety_rx_hook(self._gas_msg(0)))
        self.assertFalse(self.safety.get_controls_allowed())

    # restore counters for future tests with a couple of good messages
    for i in range(2):
      self.safety.set_controls_allowed(1)
      self.safety.safety_rx_hook(self._button_msg(Btn.SET, 0x296))
      self.safety.safety_rx_hook(self._speed_msg(0))
      self.safety.safety_rx_hook(self._gas_msg(0))
    self.safety.safety_rx_hook(self._button_msg(Btn.SET, 0x296))
    self.assertTrue(self.safety.get_controls_allowed())


  def test_tx_hook_on_pedal_pressed(self):
    for pedal in ['brake', 'gas', 'interceptor']:
      if pedal == 'brake':
        # brake_pressed_prev and honda_moving
        self.safety.safety_rx_hook(self._speed_msg(100))
        self.safety.safety_rx_hook(self._brake_msg(1))
      elif pedal == 'gas':
        # gas_pressed_prev
        self.safety.safety_rx_hook(self._gas_msg(1))
      elif pedal == 'interceptor':
        # gas_interceptor_prev > INTERCEPTOR_THRESHOLD
        self.safety.safety_rx_hook(honda_interceptor_msg(INTERCEPTOR_THRESHOLD+1, 0x201))
        self.safety.safety_rx_hook(honda_interceptor_msg(INTERCEPTOR_THRESHOLD+1, 0x201))

      self.safety.set_controls_allowed(1)
      hw = self.safety.get_honda_hw()
      if hw == HONDA_N_HW:
        self.safety.set_honda_fwd_brake(False)
        self.assertFalse(self.safety.safety_tx_hook(self._send_brake_msg(MAX_BRAKE)))
        self.assertFalse(self.safety.safety_tx_hook(honda_interceptor_msg(INTERCEPTOR_THRESHOLD, 0x200)))
      self.assertFalse(self.safety.safety_tx_hook(self._send_steer_msg(0x1000)))

      # reset status
      self.safety.set_controls_allowed(0)
      self.safety.safety_tx_hook(self._send_brake_msg(0))
      self.safety.safety_tx_hook(self._send_steer_msg(0))
      self.safety.safety_tx_hook(honda_interceptor_msg(0, 0x200))
      if pedal == 'brake':
        self.safety.safety_rx_hook(self._speed_msg(0))
        self.safety.safety_rx_hook(self._brake_msg(0))
      elif pedal == 'gas':
        self.safety.safety_rx_hook(self._gas_msg(0))
      elif pedal == 'interceptor':
        self.safety.set_gas_interceptor_detected(False)

  def test_tx_hook_on_pedal_pressed_on_unsafe_gas_mode(self):
    for pedal in ['brake', 'gas', 'interceptor']:
      self.safety.set_unsafe_mode(UNSAFE_MODE.DISABLE_DISENGAGE_ON_GAS)
      if pedal == 'brake':
        # brake_pressed_prev and honda_moving
        self.safety.safety_rx_hook(self._speed_msg(100))
        self.safety.safety_rx_hook(self._brake_msg(1))
        allow_ctrl = False
      elif pedal == 'gas':
        # gas_pressed_prev
        self.safety.safety_rx_hook(self._gas_msg(1))
        allow_ctrl = True
      elif pedal == 'interceptor':
        # gas_interceptor_prev > INTERCEPTOR_THRESHOLD
        self.safety.safety_rx_hook(honda_interceptor_msg(INTERCEPTOR_THRESHOLD+1, 0x201))
        self.safety.safety_rx_hook(honda_interceptor_msg(INTERCEPTOR_THRESHOLD+1, 0x201))
        allow_ctrl = True

      self.safety.set_controls_allowed(1)
      hw = self.safety.get_honda_hw()
      if hw == HONDA_N_HW:
        self.safety.set_honda_fwd_brake(False)
        self.assertEqual(allow_ctrl, self.safety.safety_tx_hook(self._send_brake_msg(MAX_BRAKE)))
        self.assertEqual(allow_ctrl, self.safety.safety_tx_hook(honda_interceptor_msg(INTERCEPTOR_THRESHOLD, 0x200)))
      self.assertEqual(allow_ctrl, self.safety.safety_tx_hook(self._send_steer_msg(0x1000)))
      # reset status
      self.safety.set_controls_allowed(0)
      self.safety.set_unsafe_mode(UNSAFE_MODE.DEFAULT)
      self.safety.safety_tx_hook(self._send_brake_msg(0))
      self.safety.safety_tx_hook(self._send_steer_msg(0))
      self.safety.safety_tx_hook(honda_interceptor_msg(0, 0x200))
      if pedal == 'brake':
        self.safety.safety_rx_hook(self._speed_msg(0))
        self.safety.safety_rx_hook(self._brake_msg(0))
      elif pedal == 'gas':
        self.safety.safety_rx_hook(self._gas_msg(0))
      elif pedal == 'interceptor':
        self.safety.set_gas_interceptor_detected(False)

class TestHondaNidecSafety(TestHondaSafety):

  TX_MSGS = [[0xE4, 0], [0x194, 0], [0x1FA, 0], [0x200, 0], [0x30C, 0], [0x33D, 0]]
  STANDSTILL_THRESHOLD = 0
  RELAY_MALFUNCTION_ADDR = 0xE4
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {2: [0xE4, 0x194, 0x33D, 0x30C]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  def setUp(self):
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_HONDA_NIDEC, 0)
    self.safety.init_tests_honda()

  def test_fwd_hook(self):
    # normal operation, not forwarding AEB
    self.FWD_BLACKLISTED_ADDRS[2].append(0x1FA)
    self.safety.set_honda_fwd_brake(False)
    super().test_fwd_hook()

    # TODO: test latching until AEB event is over?
    # forwarding AEB brake signal
    self.FWD_BLACKLISTED_ADDRS = {2: [0xE4, 0x194, 0x33D, 0x30C]}
    self.safety.set_honda_fwd_brake(True)
    super().test_fwd_hook()


class TestHondaBoschHarnessSafety(TestHondaSafety):
  TX_MSGS = [[0xE4, 0], [0x296, 1], [0x33D, 0]]  # Bosch Harness
  STANDSTILL_THRESHOLD = 0
  RELAY_MALFUNCTION_ADDR = 0xE4
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {2: [0xE4, 0x33D]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  def setUp(self):
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_HONDA_BOSCH_HARNESS, 0)
    self.safety.init_tests_honda()


class TestHondaBoschGiraffeSafety(TestHondaBoschHarnessSafety):
  TX_MSGS = [[0xE4, 2], [0x296, 0], [0x33D, 2]]  # Bosch Giraffe
  STANDSTILL_THRESHOLD = 0
  RELAY_MALFUNCTION_ADDR = 0xE4
  RELAY_MALFUNCTION_BUS = 2
  FWD_BLACKLISTED_ADDRS = {1: [0xE4, 0x33D]}
  FWD_BUS_LOOKUP = {1: 2, 2: 1}

  def setUp(self):
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_HONDA_BOSCH_GIRAFFE, 0)
    self.safety.init_tests_honda()


if __name__ == "__main__":
  unittest.main()
