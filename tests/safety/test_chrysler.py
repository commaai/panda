#!/usr/bin/env python3
import unittest
import numpy as np
from panda import Panda
from panda.tests.safety import libpandasafety_py
from panda.tests.safety.common import PandaSafetyTest, CANPackerPanda, make_msg, UNSAFE_MODE

MAX_RATE_UP = 3
MAX_RATE_DOWN = 3
MAX_STEER = 261

MAX_RT_DELTA = 112
RT_INTERVAL = 250000

MAX_TORQUE_ERROR = 80


def chrysler_checksum(msg, len_msg):
  checksum = 0xFF
  for idx in range(0, len_msg-1):
    curr = (msg.RDLR >> (8*idx)) if idx < 4 else (msg.RDHR >> (8*(idx - 4)))
    curr &= 0xFF
    shift = 0x80
    for i in range(0, 8):
      bit_sum = curr & shift
      temp_chk = checksum & 0x80
      if (bit_sum != 0):
        bit_sum = 0x1C
        if (temp_chk != 0):
          bit_sum = 1
        checksum = checksum << 1
        temp_chk = checksum | 1
        bit_sum ^= temp_chk
      else:
        if (temp_chk != 0):
          bit_sum = 0x1D
        checksum = checksum << 1
        bit_sum ^= checksum
      checksum = bit_sum
      shift = shift >> 1
  return ~checksum & 0xFF

class TestChryslerSafety(PandaSafetyTest, unittest.TestCase):
  cnt_torque_meas = 0
  cnt_gas = 0
  cnt_cruise = 0
  cnt_brake = 0

  TX_MSGS = [[571, 0], [658, 0], [678, 0]]
  STANDSTILL_THRESHOLD = 0
  RELAY_MALFUNCTION_ADDR = 0x292
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {2: [658, 678]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  @classmethod
  def setUp(cls):
    # TODO: add chrysler checksum to can packer
    cls.packer = CANPackerPanda("chrysler_pacifica_2017_hybrid")
    cls.safety = libpandasafety_py.libpandasafety
    cls.safety.set_safety_hooks(Panda.SAFETY_CHRYSLER, 0)
    cls.safety.init_tests_chrysler()

  def _button_msg(self, cancel=0, flw_dec=0, flw_inc=0, spd_dec=0, spd_inc=0, resume=0):
    values = {"ACC_CANCEL": cancel, "ACC_FOLLOW_DEC": flw_dec,
                "ACC_FOLLOW_INC": flw_inc, "ACC_SPEED_DEC": spd_dec,
                "ACC_SPEED_INC": spd_inc, "ACC_RESUME": resume}
    return self.packer.make_can_msg_panda("WHEEL_BUTTONS", 0, values)

  def _cruise_msg(self, active):
    values = {"ACC_STATUS_2": 0x7 if active else 0, "COUNTER": self.cnt_cruise % 16}
    msg = self.packer.make_can_msg_panda("ACC_2", 0, values)
    values["CHECKSUM"] = chrysler_checksum(msg[0], 8)
    self.__class__.cnt_cruise += 1
    return self.packer.make_can_msg_panda("ACC_2", 0, values)

  def _speed_msg(self, speed):
    # TODO: fix this
    values = {"SPEED_LEFT": speed, "SPEED_RIGHT": speed}
    speed = int(speed / 0.071028)
    to_send = make_msg(0, 514, 4)
    to_send[0].RDLR = ((speed & 0xFF0) >> 4) + ((speed & 0xF) << 12) + \
                      ((speed & 0xFF0) << 12) + ((speed & 0xF) << 28)
    return to_send
    msg = self.packer.make_can_msg_panda("SPEED_1", 0, values)
    print()
    print("rdlr", hex(to_send[0].RDLR), hex(msg[0].RDLR))
    print()
    return self.packer.make_can_msg_panda("SPEED_1", 0, values)

  def _gas_msg(self, gas):
    values = {"ACCEL_134": gas, "COUNTER": self.cnt_gas % 16}
    self.__class__.cnt_gas += 1
    return self.packer.make_can_msg_panda("ACCEL_GAS_134", 0, values)

  def _brake_msg(self, brake):
    values = {"BRAKE_PRESSED_2": 5 if brake else 0, "COUNTER": self.cnt_brake % 16}
    msg = self.packer.make_can_msg_panda("BRAKE_2", 0, values)
    values["CHECKSUM"] = chrysler_checksum(msg[0], 8)
    self.__class__.cnt_brake += 1
    return self.packer.make_can_msg_panda("BRAKE_2", 0, values)

  def _set_prev_torque(self, t):
    self.safety.set_chrysler_desired_torque_last(t)
    self.safety.set_chrysler_rt_torque_last(t)
    self.safety.set_chrysler_torque_meas(t, t)

  def _torque_meas_msg(self, torque):
    values = {"TORQUE_MOTOR": torque, "COUNTER": self.cnt_torque_meas % 16}
    msg = self.packer.make_can_msg_panda("EPS_STATUS", 0, values)
    values["CHECKSUM"] = chrysler_checksum(msg[0], 8)
    self.__class__.cnt_torque_meas += 1
    return self.packer.make_can_msg_panda("EPS_STATUS", 0, values)

  def _torque_msg(self, torque):
    values = {"LKAS_STEERING_TORQUE": torque}
    return self.packer.make_can_msg_panda("LKAS_COMMAND", 0, values)

  def test_steer_safety_check(self):
    for enabled in [0, 1]:
      for t in range(-MAX_STEER*2, MAX_STEER*2):
        self.safety.set_controls_allowed(enabled)
        self._set_prev_torque(t)
        if abs(t) > MAX_STEER or (not enabled and abs(t) > 0):
          self.assertFalse(self._tx(self._torque_msg(t)))
        else:
          self.assertTrue(self._tx(self._torque_msg(t)))

  def test_enable_control_allowed_from_cruise(self):
    to_push = self._cruise_msg(True)
    self._rx(to_push)
    self.assertTrue(self.safety.get_controls_allowed())

  def test_disable_control_allowed_from_cruise(self):
    to_push = self._cruise_msg(False)
    self.safety.set_controls_allowed(1)
    self._rx(to_push)
    self.assertFalse(self.safety.get_controls_allowed())

  def test_gas_disable(self):
    self.safety.set_controls_allowed(1)
    self._rx(self._speed_msg(2.2))
    self._rx(self._gas_msg(1))
    self.assertTrue(self.safety.get_controls_allowed())
    self._rx(self._gas_msg(0))
    self._rx(self._speed_msg(2.3))
    self._rx(self._gas_msg(1))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_unsafe_mode_no_disengage_on_gas(self):
    self._rx(self._gas_msg(0))
    self.safety.set_controls_allowed(1)
    self.safety.set_unsafe_mode(UNSAFE_MODE.DISABLE_DISENGAGE_ON_GAS)
    self._rx(self._gas_msg(1))
    self.assertTrue(self.safety.get_controls_allowed())
    self.safety.set_unsafe_mode(UNSAFE_MODE.DEFAULT)

  def test_non_realtime_limit_up(self):
    self.safety.set_controls_allowed(True)

    self._set_prev_torque(0)
    self.assertTrue(self._tx(self._torque_msg(MAX_RATE_UP)))

    self._set_prev_torque(0)
    self.assertFalse(self._tx(self._torque_msg(MAX_RATE_UP + 1)))

  def test_non_realtime_limit_down(self):
    self.safety.set_controls_allowed(True)

    self.safety.set_chrysler_rt_torque_last(MAX_STEER)
    torque_meas = MAX_STEER - MAX_TORQUE_ERROR - 20
    self.safety.set_chrysler_torque_meas(torque_meas, torque_meas)
    self.safety.set_chrysler_desired_torque_last(MAX_STEER)
    self.assertTrue(self._tx(self._torque_msg(MAX_STEER - MAX_RATE_DOWN)))

    self.safety.set_chrysler_rt_torque_last(MAX_STEER)
    self.safety.set_chrysler_torque_meas(torque_meas, torque_meas)
    self.safety.set_chrysler_desired_torque_last(MAX_STEER)
    self.assertFalse(self._tx(self._torque_msg(MAX_STEER - MAX_RATE_DOWN + 1)))

  def test_exceed_torque_sensor(self):
    self.safety.set_controls_allowed(True)

    for sign in [-1, 1]:
      self._set_prev_torque(0)
      for t in np.arange(0, MAX_TORQUE_ERROR + 2, 2):  # step needs to be smaller than MAX_TORQUE_ERROR
        t *= sign
        self.assertTrue(self._tx(self._torque_msg(t)))

      self.assertFalse(self._tx(self._torque_msg(sign * (MAX_TORQUE_ERROR + 2))))

  def test_realtime_limit_up(self):
    self.safety.set_controls_allowed(True)

    for sign in [-1, 1]:
      self.safety.init_tests_chrysler()
      self._set_prev_torque(0)
      for t in np.arange(0, MAX_RT_DELTA+1, 1):
        t *= sign
        self.safety.set_chrysler_torque_meas(t, t)
        self.assertTrue(self._tx(self._torque_msg(t)))
      self.assertFalse(self._tx(self._torque_msg(sign * (MAX_RT_DELTA + 1))))

      self._set_prev_torque(0)
      for t in np.arange(0, MAX_RT_DELTA+1, 1):
        t *= sign
        self.safety.set_chrysler_torque_meas(t, t)
        self.assertTrue(self._tx(self._torque_msg(t)))

      # Increase timer to update rt_torque_last
      self.safety.set_timer(RT_INTERVAL + 1)
      self.assertTrue(self._tx(self._torque_msg(sign * MAX_RT_DELTA)))
      self.assertTrue(self._tx(self._torque_msg(sign * (MAX_RT_DELTA + 1))))

  def test_torque_measurements(self):
    self._rx(self._torque_meas_msg(50))
    self._rx(self._torque_meas_msg(-50))
    self._rx(self._torque_meas_msg(0))
    self._rx(self._torque_meas_msg(0))
    self._rx(self._torque_meas_msg(0))
    self._rx(self._torque_meas_msg(0))

    self.assertEqual(-50, self.safety.get_chrysler_torque_meas_min())
    self.assertEqual(50, self.safety.get_chrysler_torque_meas_max())

    self._rx(self._torque_meas_msg(0))
    self.assertEqual(0, self.safety.get_chrysler_torque_meas_max())
    self.assertEqual(-50, self.safety.get_chrysler_torque_meas_min())

    self._rx(self._torque_meas_msg(0))
    self.assertEqual(0, self.safety.get_chrysler_torque_meas_max())
    self.assertEqual(0, self.safety.get_chrysler_torque_meas_min())

  # only allow cancel button press to be spoofed
  def test_button_spoof(self):
    n_btns = 6
    for b in range(0, 2**n_btns + 1):
      btns = [(b >> i) & 1 for i in range(n_btns)]
      btns[2] = 0 # TODO: delete this line. fix is in separate PR
      # cancel is first arg in _button_msg
      should_tx = btns[0] and not any(btns[1:])
      self.assertEqual(should_tx, self._tx(self._button_msg(*btns)))


if __name__ == "__main__":
  unittest.main()
