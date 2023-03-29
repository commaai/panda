import os
import abc
import unittest
import importlib
import numpy as np
from typing import Dict, List, Optional

from opendbc.can.packer import CANPacker  # pylint: disable=import-error
from panda import ALTERNATIVE_EXPERIENCE
from panda.tests.libpanda import libpanda_py

MAX_WRONG_COUNTERS = 5


def sign_of(a):
  return 1 if a > 0 else -1


def make_msg(bus, addr, length=8):
  return libpanda_py.make_CANPacket(addr, bus, b'\x00' * length)


class CANPackerPanda(CANPacker):
  def make_can_msg_panda(self, name_or_addr, bus, values, fix_checksum=None):
    msg = self.make_can_msg(name_or_addr, bus, values)
    if fix_checksum is not None:
      msg = fix_checksum(msg)
    addr, _, dat, bus = msg
    return libpanda_py.make_CANPacket(addr, bus, dat)


def add_regen_tests(cls):
  """Dynamically adds regen tests for all user brake tests."""

  # only rx/user brake tests, not brake command
  found_tests = [func for func in dir(cls) if func.startswith("test_") and "user_brake" in func]
  assert len(found_tests) >= 3, "Failed to detect known brake tests"

  for test in found_tests:
    def _make_regen_test(brake_func):
      def _regen_test(self):
        # only for safety modes with a regen message
        if self._user_regen_msg(0) is None:
          raise unittest.SkipTest("Safety mode implements no _user_regen_msg")

        getattr(self, brake_func)(self._user_regen_msg, self.safety.get_regen_braking_prev)
      return _regen_test

    setattr(cls, test.replace("brake", "regen"), _make_regen_test(test))

  return cls


class PandaSafetyTestBase(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "PandaSafetyTestBase":
      cls.safety = None
      raise unittest.SkipTest

  def _rx(self, msg):
    return self.safety.safety_rx_hook(msg)

  def _tx(self, msg):
    return self.safety.safety_tx_hook(msg)


class InterceptorSafetyTest(PandaSafetyTestBase):

  INTERCEPTOR_THRESHOLD = 0

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "InterceptorSafetyTest":
      cls.safety = None
      raise unittest.SkipTest

  @abc.abstractmethod
  def _interceptor_gas_cmd(self, gas):
    pass

  @abc.abstractmethod
  def _interceptor_user_gas(self, gas):
    pass

  def test_prev_gas_interceptor(self):
    self._rx(self._interceptor_user_gas(0x0))
    self.assertFalse(self.safety.get_gas_interceptor_prev())
    self._rx(self._interceptor_user_gas(0x1000))
    self.assertTrue(self.safety.get_gas_interceptor_prev())
    self._rx(self._interceptor_user_gas(0x0))
    self.safety.set_gas_interceptor_detected(False)

  def test_disengage_on_gas_interceptor(self):
    for g in range(0, 0x1000):
      self._rx(self._interceptor_user_gas(0))
      self.safety.set_controls_allowed(True)
      self._rx(self._interceptor_user_gas(g))
      remain_enabled = g <= self.INTERCEPTOR_THRESHOLD
      self.assertEqual(remain_enabled, self.safety.get_controls_allowed())
      self._rx(self._interceptor_user_gas(0))
      self.safety.set_gas_interceptor_detected(False)

  def test_alternative_experience_no_disengage_on_gas_interceptor(self):
    self.safety.set_controls_allowed(True)
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.DISABLE_DISENGAGE_ON_GAS)
    for g in range(0, 0x1000):
      self._rx(self._interceptor_user_gas(g))
      # Test we allow lateral, but not longitudinal
      self.assertTrue(self.safety.get_controls_allowed())
      self.assertEqual(g <= self.INTERCEPTOR_THRESHOLD, self.safety.get_longitudinal_allowed())
      # Make sure we can re-gain longitudinal actuation
      self._rx(self._interceptor_user_gas(0))
      self.assertTrue(self.safety.get_longitudinal_allowed())

  def test_allow_engage_with_gas_interceptor_pressed(self):
    self._rx(self._interceptor_user_gas(0x1000))
    self.safety.set_controls_allowed(1)
    self._rx(self._interceptor_user_gas(0x1000))
    self.assertTrue(self.safety.get_controls_allowed())
    self._rx(self._interceptor_user_gas(0))

  def test_gas_interceptor_safety_check(self):
    for gas in np.arange(0, 4000, 100):
      for controls_allowed in [True, False]:
        self.safety.set_controls_allowed(controls_allowed)
        if controls_allowed:
          send = True
        else:
          send = gas == 0
        self.assertEqual(send, self._tx(self._interceptor_gas_cmd(gas)))


class LongitudinalAccelSafetyTest(PandaSafetyTestBase, abc.ABC):

  MAX_ACCEL: float = 2.0
  MIN_ACCEL: float = -3.5
  INACTIVE_ACCEL: float = 0.0

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "LongitudinalAccelSafetyTest":
      cls.safety = None
      raise unittest.SkipTest

  @abc.abstractmethod
  def _accel_msg(self, accel: float):
    pass

  def test_accel_limits_correct(self):
    self.assertGreater(self.MAX_ACCEL, 0)
    self.assertLess(self.MIN_ACCEL, 0)

  def test_accel_actuation_limits(self, stock_longitudinal=False):
    limits = ((self.MIN_ACCEL, self.MAX_ACCEL, ALTERNATIVE_EXPERIENCE.DEFAULT),
              (self.MIN_ACCEL, self.MAX_ACCEL, ALTERNATIVE_EXPERIENCE.RAISE_LONGITUDINAL_LIMITS_TO_ISO_MAX))

    for min_accel, max_accel, alternative_experience in limits:
      for accel in np.arange(min_accel - 1, max_accel + 1, 0.05):
        for controls_allowed in [True, False]:
          self.safety.set_controls_allowed(controls_allowed)
          self.safety.set_alternative_experience(alternative_experience)
          if stock_longitudinal:
            should_tx = False
          elif controls_allowed:
            should_tx = int(min_accel * 1000) <= int(accel * 1000) <= int(max_accel * 1000)
          else:
            should_tx = np.isclose(accel, self.INACTIVE_ACCEL, atol=0.0001)
          self.assertEqual(should_tx, self._tx(self._accel_msg(accel)))


class TorqueSteeringSafetyTestBase(PandaSafetyTestBase, abc.ABC):

  MAX_RATE_UP = 0
  MAX_RATE_DOWN = 0
  MAX_TORQUE = 0
  MAX_RT_DELTA = 0
  RT_INTERVAL = 0

  # Safety around steering req bit
  MIN_VALID_STEERING_FRAMES = 0
  MAX_INVALID_STEERING_FRAMES = 0
  MIN_VALID_STEERING_RT_INTERVAL = 0

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "TorqueSteeringSafetyTestBase":
      cls.safety = None
      raise unittest.SkipTest

  @abc.abstractmethod
  def _torque_cmd_msg(self, torque, steer_req=1):
    pass

  def _set_prev_torque(self, t):
    self.safety.set_desired_torque_last(t)
    self.safety.set_rt_torque_last(t)

  def test_steer_safety_check(self):
    for enabled in [0, 1]:
      for t in range(-self.MAX_TORQUE * 2, self.MAX_TORQUE * 2):
        self.safety.set_controls_allowed(enabled)
        self._set_prev_torque(t)
        if abs(t) > self.MAX_TORQUE or (not enabled and abs(t) > 0):
          self.assertFalse(self._tx(self._torque_cmd_msg(t)))
        else:
          self.assertTrue(self._tx(self._torque_cmd_msg(t)))

  def test_non_realtime_limit_up(self):
    self.safety.set_controls_allowed(True)

    self._set_prev_torque(0)
    self.assertTrue(self._tx(self._torque_cmd_msg(self.MAX_RATE_UP)))
    self._set_prev_torque(0)
    self.assertTrue(self._tx(self._torque_cmd_msg(-self.MAX_RATE_UP)))

    self._set_prev_torque(0)
    self.assertFalse(self._tx(self._torque_cmd_msg(self.MAX_RATE_UP + 1)))
    self.safety.set_controls_allowed(True)
    self._set_prev_torque(0)
    self.assertFalse(self._tx(self._torque_cmd_msg(-self.MAX_RATE_UP - 1)))

  def test_steer_req_bit_frames(self):
    """
      Certain safety modes implement some tolerance on their steer request bits matching the
      requested torque to avoid a steering fault or lockout and maintain torque. This tests:
        - We can't cut torque for more than one frame
        - We can't cut torque until at least the minimum number of matching steer_req messages
        - We can always recover from violations if steer_req=1
    """

    for min_valid_steer_frames in range(self.MIN_VALID_STEERING_FRAMES * 2):
      # Reset match count and rt timer to allow cut (valid_steer_req_count, ts_steer_req_mismatch_last)
      self.safety.init_tests()
      self.safety.set_timer(self.MIN_VALID_STEERING_RT_INTERVAL)

      # Allow torque cut
      self.safety.set_controls_allowed(True)
      self._set_prev_torque(self.MAX_TORQUE)
      for _ in range(min_valid_steer_frames):
        self.assertTrue(self._tx(self._torque_cmd_msg(self.MAX_TORQUE, steer_req=1)))

      # should tx if we've sent enough valid frames, and we're not cutting torque for too many frames consecutively
      should_tx = min_valid_steer_frames >= self.MIN_VALID_STEERING_FRAMES
      for idx in range(self.MAX_INVALID_STEERING_FRAMES * 2):
        tx = self._tx(self._torque_cmd_msg(self.MAX_TORQUE, steer_req=0))
        self.assertEqual(should_tx and idx < self.MAX_INVALID_STEERING_FRAMES, tx)

      # Keep blocking after one steer_req mismatch
      for _ in range(100):
        self.assertFalse(self._tx(self._torque_cmd_msg(self.MAX_TORQUE, steer_req=0)))

      # Make sure we can recover
      self.assertTrue(self._tx(self._torque_cmd_msg(0, steer_req=1)))
      self._set_prev_torque(self.MAX_TORQUE)
      self.assertTrue(self._tx(self._torque_cmd_msg(self.MAX_TORQUE, steer_req=1)))

  def test_steer_req_bit_multi_invalid(self):
    """
      For safety modes allowing multiple consecutive invalid frames, this ensures that once a valid frame
      is sent after an invalid frame (even without sending the max number of allowed invalid frames),
      all counters are reset.
    """
    # TODO: Add safety around steer request bits for all safety modes and remove exception
    if self.MIN_VALID_STEERING_FRAMES == 0:
      raise unittest.SkipTest("Safety mode does not implement tolerance for steer request bit safety")

    for max_invalid_steer_frames in range(1, self.MAX_INVALID_STEERING_FRAMES * 2):
      self.safety.init_tests()
      self.safety.set_timer(self.MIN_VALID_STEERING_RT_INTERVAL)

      # Allow torque cut
      self.safety.set_controls_allowed(True)
      self._set_prev_torque(self.MAX_TORQUE)
      for _ in range(self.MIN_VALID_STEERING_FRAMES):
        self.assertTrue(self._tx(self._torque_cmd_msg(self.MAX_TORQUE, steer_req=1)))

      # Send partial amount of allowed invalid frames
      for idx in range(max_invalid_steer_frames):
        should_tx = idx < self.MAX_INVALID_STEERING_FRAMES
        self.assertEqual(should_tx, self._tx(self._torque_cmd_msg(self.MAX_TORQUE, steer_req=0)))

      # Send one valid frame, and subsequent invalid should now be blocked
      self._set_prev_torque(self.MAX_TORQUE)
      self.assertTrue(self._tx(self._torque_cmd_msg(self.MAX_TORQUE, steer_req=1)))
      for _ in range(self.MIN_VALID_STEERING_FRAMES + 1):
        self.assertFalse(self._tx(self._torque_cmd_msg(self.MAX_TORQUE, steer_req=0)))

  def test_steer_req_bit_realtime(self):
    """
      Realtime safety for cutting steer request bit. This tests:
        - That we allow messages with mismatching steer request bit if time from last is >= MIN_VALID_STEERING_RT_INTERVAL
        - That frame mismatch safety does not interfere with this test
    """
    # TODO: Add safety around steer request bits for all safety modes and remove exception
    if self.MIN_VALID_STEERING_RT_INTERVAL == 0:
      raise unittest.SkipTest("Safety mode does not implement tolerance for steer request bit safety")

    for rt_us in np.arange(self.MIN_VALID_STEERING_RT_INTERVAL - 50000, self.MIN_VALID_STEERING_RT_INTERVAL + 50000, 10000):
      # Reset match count and rt timer (valid_steer_req_count, ts_steer_req_mismatch_last)
      self.safety.init_tests()

      # Make sure valid_steer_req_count doesn't affect this test
      self.safety.set_controls_allowed(True)
      self._set_prev_torque(self.MAX_TORQUE)
      for _ in range(self.MIN_VALID_STEERING_FRAMES):
        self.assertTrue(self._tx(self._torque_cmd_msg(self.MAX_TORQUE, steer_req=1)))

      # Normally, sending MIN_VALID_STEERING_FRAMES valid frames should always allow
      self.safety.set_timer(max(rt_us, 0))
      should_tx = rt_us >= self.MIN_VALID_STEERING_RT_INTERVAL
      for _ in range(self.MAX_INVALID_STEERING_FRAMES):
        self.assertEqual(should_tx, self._tx(self._torque_cmd_msg(self.MAX_TORQUE, steer_req=0)))

      # Keep blocking after one steer_req mismatch
      for _ in range(100):
        self.assertFalse(self._tx(self._torque_cmd_msg(self.MAX_TORQUE, steer_req=0)))

      # Make sure we can recover
      self.assertTrue(self._tx(self._torque_cmd_msg(0, steer_req=1)))
      self._set_prev_torque(self.MAX_TORQUE)
      self.assertTrue(self._tx(self._torque_cmd_msg(self.MAX_TORQUE, steer_req=1)))


class DriverTorqueSteeringSafetyTest(TorqueSteeringSafetyTestBase, abc.ABC):

  DRIVER_TORQUE_ALLOWANCE = 0
  DRIVER_TORQUE_FACTOR = 0

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "DriverTorqueSteeringSafetyTest":
      cls.safety = None
      raise unittest.SkipTest

  def test_non_realtime_limit_up(self):
    self.safety.set_torque_driver(0, 0)
    super().test_non_realtime_limit_up()

  def test_against_torque_driver(self):
    # Tests down limits and driver torque blending
    self.safety.set_controls_allowed(True)

    for sign in [-1, 1]:
      for t in np.arange(0, self.DRIVER_TORQUE_ALLOWANCE + 1, 1):
        t *= -sign
        self.safety.set_torque_driver(t, t)
        self._set_prev_torque(self.MAX_TORQUE * sign)
        self.assertTrue(self._tx(self._torque_cmd_msg(self.MAX_TORQUE * sign)))

      self.safety.set_torque_driver(self.DRIVER_TORQUE_ALLOWANCE + 1, self.DRIVER_TORQUE_ALLOWANCE + 1)
      self.assertFalse(self._tx(self._torque_cmd_msg(-self.MAX_TORQUE)))

    # arbitrary high driver torque to ensure max steer torque is allowed
    max_driver_torque = int(self.MAX_TORQUE / self.DRIVER_TORQUE_FACTOR + self.DRIVER_TORQUE_ALLOWANCE + 1)

    # spot check some individual cases
    for sign in [-1, 1]:
      driver_torque = (self.DRIVER_TORQUE_ALLOWANCE + 10) * sign
      torque_desired = (self.MAX_TORQUE - 10 * self.DRIVER_TORQUE_FACTOR) * sign
      delta = 1 * sign
      self._set_prev_torque(torque_desired)
      self.safety.set_torque_driver(-driver_torque, -driver_torque)
      self.assertTrue(self._tx(self._torque_cmd_msg(torque_desired)))
      self._set_prev_torque(torque_desired + delta)
      self.safety.set_torque_driver(-driver_torque, -driver_torque)
      self.assertFalse(self._tx(self._torque_cmd_msg(torque_desired + delta)))

      self._set_prev_torque(self.MAX_TORQUE * sign)
      self.safety.set_torque_driver(-max_driver_torque * sign, -max_driver_torque * sign)
      self.assertTrue(self._tx(self._torque_cmd_msg((self.MAX_TORQUE - self.MAX_RATE_DOWN) * sign)))
      self._set_prev_torque(self.MAX_TORQUE * sign)
      self.safety.set_torque_driver(-max_driver_torque * sign, -max_driver_torque * sign)
      self.assertTrue(self._tx(self._torque_cmd_msg(0)))
      self._set_prev_torque(self.MAX_TORQUE * sign)
      self.safety.set_torque_driver(-max_driver_torque * sign, -max_driver_torque * sign)
      self.assertFalse(self._tx(self._torque_cmd_msg((self.MAX_TORQUE - self.MAX_RATE_DOWN + 1) * sign)))

  def test_realtime_limits(self):
    self.safety.set_controls_allowed(True)

    for sign in [-1, 1]:
      self.safety.init_tests()
      self._set_prev_torque(0)
      self.safety.set_torque_driver(0, 0)
      for t in np.arange(0, self.MAX_RT_DELTA, 1):
        t *= sign
        self.assertTrue(self._tx(self._torque_cmd_msg(t)))
      self.assertFalse(self._tx(self._torque_cmd_msg(sign * (self.MAX_RT_DELTA + 1))))

      self._set_prev_torque(0)
      for t in np.arange(0, self.MAX_RT_DELTA, 1):
        t *= sign
        self.assertTrue(self._tx(self._torque_cmd_msg(t)))

      # Increase timer to update rt_torque_last
      self.safety.set_timer(self.RT_INTERVAL + 1)
      self.assertTrue(self._tx(self._torque_cmd_msg(sign * (self.MAX_RT_DELTA - 1))))
      self.assertTrue(self._tx(self._torque_cmd_msg(sign * (self.MAX_RT_DELTA + 1))))


class MotorTorqueSteeringSafetyTest(TorqueSteeringSafetyTestBase, abc.ABC):

  MAX_TORQUE_ERROR = 0
  TORQUE_MEAS_TOLERANCE = 0

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "MotorTorqueSteeringSafetyTest":
      cls.safety = None
      raise unittest.SkipTest

  @abc.abstractmethod
  def _torque_meas_msg(self, torque):
    pass

  def _set_prev_torque(self, t):
    super()._set_prev_torque(t)
    self.safety.set_torque_meas(t, t)

  def test_torque_absolute_limits(self):
    for controls_allowed in [True, False]:
      for torque in np.arange(-self.MAX_TORQUE - 1000, self.MAX_TORQUE + 1000, self.MAX_RATE_UP):
        self.safety.set_controls_allowed(controls_allowed)
        self.safety.set_rt_torque_last(torque)
        self.safety.set_torque_meas(torque, torque)
        self.safety.set_desired_torque_last(torque - self.MAX_RATE_UP)

        if controls_allowed:
          send = (-self.MAX_TORQUE <= torque <= self.MAX_TORQUE)
        else:
          send = torque == 0

        self.assertEqual(send, self._tx(self._torque_cmd_msg(torque)))

  def test_non_realtime_limit_down(self):
    self.safety.set_controls_allowed(True)

    torque_meas = self.MAX_TORQUE - self.MAX_TORQUE_ERROR - 50

    self.safety.set_rt_torque_last(self.MAX_TORQUE)
    self.safety.set_torque_meas(torque_meas, torque_meas)
    self.safety.set_desired_torque_last(self.MAX_TORQUE)
    self.assertTrue(self._tx(self._torque_cmd_msg(self.MAX_TORQUE - self.MAX_RATE_DOWN)))

    self.safety.set_rt_torque_last(self.MAX_TORQUE)
    self.safety.set_torque_meas(torque_meas, torque_meas)
    self.safety.set_desired_torque_last(self.MAX_TORQUE)
    self.assertFalse(self._tx(self._torque_cmd_msg(self.MAX_TORQUE - self.MAX_RATE_DOWN + 1)))

  def test_exceed_torque_sensor(self):
    self.safety.set_controls_allowed(True)

    for sign in [-1, 1]:
      self._set_prev_torque(0)
      for t in np.arange(0, self.MAX_TORQUE_ERROR + 2, 2):  # step needs to be smaller than MAX_TORQUE_ERROR
        t *= sign
        self.assertTrue(self._tx(self._torque_cmd_msg(t)))

      self.assertFalse(self._tx(self._torque_cmd_msg(sign * (self.MAX_TORQUE_ERROR + 2))))

  def test_realtime_limit_up(self):
    self.safety.set_controls_allowed(True)

    for sign in [-1, 1]:
      self.safety.init_tests()
      self._set_prev_torque(0)
      for t in np.arange(0, self.MAX_RT_DELTA + 1, 1):
        t *= sign
        self.safety.set_torque_meas(t, t)
        self.assertTrue(self._tx(self._torque_cmd_msg(t)))
      self.assertFalse(self._tx(self._torque_cmd_msg(sign * (self.MAX_RT_DELTA + 1))))

      self._set_prev_torque(0)
      for t in np.arange(0, self.MAX_RT_DELTA + 1, 1):
        t *= sign
        self.safety.set_torque_meas(t, t)
        self.assertTrue(self._tx(self._torque_cmd_msg(t)))

      # Increase timer to update rt_torque_last
      self.safety.set_timer(self.RT_INTERVAL + 1)
      self.assertTrue(self._tx(self._torque_cmd_msg(sign * self.MAX_RT_DELTA)))
      self.assertTrue(self._tx(self._torque_cmd_msg(sign * (self.MAX_RT_DELTA + 1))))

  def test_torque_measurements(self):
    trq = 50
    for t in [trq, -trq, 0, 0, 0, 0]:
      self._rx(self._torque_meas_msg(t))

    max_range = range(trq, trq + self.TORQUE_MEAS_TOLERANCE + 1)
    min_range = range(-(trq + self.TORQUE_MEAS_TOLERANCE), -trq + 1)
    self.assertTrue(self.safety.get_torque_meas_min() in min_range)
    self.assertTrue(self.safety.get_torque_meas_max() in max_range)

    max_range = range(0, self.TORQUE_MEAS_TOLERANCE + 1)
    min_range = range(-(trq + self.TORQUE_MEAS_TOLERANCE), -trq + 1)
    self._rx(self._torque_meas_msg(0))
    self.assertTrue(self.safety.get_torque_meas_min() in min_range)
    self.assertTrue(self.safety.get_torque_meas_max() in max_range)

    max_range = range(0, self.TORQUE_MEAS_TOLERANCE + 1)
    min_range = range(-self.TORQUE_MEAS_TOLERANCE, 0 + 1)
    self._rx(self._torque_meas_msg(0))
    self.assertTrue(self.safety.get_torque_meas_min() in min_range)
    self.assertTrue(self.safety.get_torque_meas_max() in max_range)


class AngleSteeringSafetyTest(PandaSafetyTestBase):

  DEG_TO_CAN: int

  ANGLE_DELTA_BP: List[float]
  ANGLE_DELTA_V: List[float]  # windup limit
  ANGLE_DELTA_VU: List[float]  # unwind limit

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "AngleSteeringSafetyTest":
      cls.safety = None
      raise unittest.SkipTest

  @abc.abstractmethod
  def _angle_cmd_msg(self, angle: float, enabled: bool):
    pass

  @abc.abstractmethod
  def _angle_meas_msg(self, angle: float):
    pass

  def _set_prev_desired_angle(self, t):
    t = int(t * self.DEG_TO_CAN)
    self.safety.set_desired_angle_last(t)

  def _angle_meas_msg_array(self, angle):
    for _ in range(6):
      self._rx(self._angle_meas_msg(angle))

  def test_angle_cmd_when_enabled(self):
    # when controls are allowed, angle cmd rate limit is enforced
    speeds = [0., 1., 5., 10., 15., 50.]
    angles = [-300, -100, -10, 0, 10, 100, 300]
    for a in angles:
      for s in speeds:
        max_delta_up = np.interp(s, self.ANGLE_DELTA_BP, self.ANGLE_DELTA_V)
        max_delta_down = np.interp(s, self.ANGLE_DELTA_BP, self.ANGLE_DELTA_VU)

        # first test against false positives
        self._angle_meas_msg_array(a)
        self._rx(self._speed_msg(s))  # pylint: disable=no-member

        self._set_prev_desired_angle(a)
        self.safety.set_controls_allowed(1)

        # Stay within limits
        # Up
        self.assertTrue(self._tx(self._angle_cmd_msg(a + sign_of(a) * max_delta_up, True)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Don't change
        self.assertTrue(self._tx(self._angle_cmd_msg(a, True)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Down
        self.assertTrue(self._tx(self._angle_cmd_msg(a - sign_of(a) * max_delta_down, True)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Inject too high rates
        # Up
        self.assertFalse(self._tx(self._angle_cmd_msg(a + sign_of(a) * (max_delta_up + 1.1), True)))

        # Don't change
        self.safety.set_controls_allowed(1)
        self._set_prev_desired_angle(a)
        self.assertTrue(self.safety.get_controls_allowed())
        self.assertTrue(self._tx(self._angle_cmd_msg(a, True)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Down
        self.assertFalse(self._tx(self._angle_cmd_msg(a - sign_of(a) * (max_delta_down + 1.1), True)))

        # Check desired steer should be the same as steer angle when controls are off
        self.safety.set_controls_allowed(0)
        self.assertTrue(self._tx(self._angle_cmd_msg(a, False)))

  def test_angle_cmd_when_disabled(self):
    self.safety.set_controls_allowed(0)

    self._set_prev_desired_angle(0)
    self.assertFalse(self._tx(self._angle_cmd_msg(0, True)))
    self.assertFalse(self.safety.get_controls_allowed())


@add_regen_tests
class PandaSafetyTest(PandaSafetyTestBase):
  TX_MSGS: Optional[List[List[int]]] = None
  SCANNED_ADDRS = [*range(0x0, 0x800),                      # Entire 11-bit CAN address space
                   *range(0x18DA00F1, 0x18DB00F1, 0x100),   # 29-bit UDS physical addressing
                   *range(0x18DB00F1, 0x18DC00F1, 0x100),   # 29-bit UDS functional addressing
                   *range(0x3300, 0x3400),                  # Honda
                   0x10400060, 0x104c006c]                  # GMLAN (exceptions, range/format unclear)
  STANDSTILL_THRESHOLD: Optional[float] = None
  GAS_PRESSED_THRESHOLD = 0
  RELAY_MALFUNCTION_ADDR: Optional[int] = None
  RELAY_MALFUNCTION_BUS: Optional[int] = None
  FWD_BLACKLISTED_ADDRS: Dict[int, List[int]] = {}  # {bus: [addr]}
  FWD_BUS_LOOKUP: Dict[int, int] = {}

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "PandaSafetyTest" or cls.__name__.endswith('Base'):
      cls.safety = None
      raise unittest.SkipTest

  @abc.abstractmethod
  def _user_brake_msg(self, brake):
    pass

  def _user_regen_msg(self, regen):
    pass

  @abc.abstractmethod
  def _speed_msg(self, speed):
    pass

  # Safety modes can override if vehicle_moving is driven by a different message
  def _vehicle_moving_msg(self, speed: float):
    return self._speed_msg(speed)

  @abc.abstractmethod
  def _user_gas_msg(self, gas):
    pass

  @abc.abstractmethod
  def _pcm_status_msg(self, enable):
    pass

  # ***** standard tests for all safety modes *****

  def test_tx_msg_in_scanned_range(self):
    # the relay malfunction, fwd hook, and spam can tests don't exhaustively
    # scan the entire 29-bit address space, only some known important ranges
    # make sure SCANNED_ADDRS stays up to date with car port TX_MSGS; new
    # model ports should expand the range if needed
    for msg in self.TX_MSGS:
      self.assertTrue(msg[0] in self.SCANNED_ADDRS, f"{msg[0]=:#x}")

  def test_relay_malfunction(self):
    # each car has an addr that is used to detect relay malfunction
    # if that addr is seen on specified bus, triggers the relay malfunction
    # protection logic: both tx_hook and fwd_hook are expected to return failure
    self.assertFalse(self.safety.get_relay_malfunction())
    self._rx(make_msg(self.RELAY_MALFUNCTION_BUS, self.RELAY_MALFUNCTION_ADDR, 8))
    self.assertTrue(self.safety.get_relay_malfunction())
    for bus in range(0, 3):
      for addr in self.SCANNED_ADDRS:
        self.assertEqual(-1, self._tx(make_msg(bus, addr, 8)))
        self.assertEqual(-1, self.safety.safety_fwd_hook(bus, make_msg(bus, addr, 8)))

  def test_fwd_hook(self):
    # some safety modes don't forward anything, while others blacklist msgs
    for bus in range(0, 3):
      for addr in self.SCANNED_ADDRS:
        # assume len 8
        msg = make_msg(bus, addr, 8)
        fwd_bus = self.FWD_BUS_LOOKUP.get(bus, -1)
        if bus in self.FWD_BLACKLISTED_ADDRS and addr in self.FWD_BLACKLISTED_ADDRS[bus]:
          fwd_bus = -1
        self.assertEqual(fwd_bus, self.safety.safety_fwd_hook(bus, msg), f"{addr=:#x} from {bus=} to {fwd_bus=}")

  def test_spam_can_buses(self):
    for bus in range(0, 4):
      for addr in self.SCANNED_ADDRS:
        if all(addr != m[0] or bus != m[1] for m in self.TX_MSGS):
          self.assertFalse(self._tx(make_msg(bus, addr, 8)), f"allowed TX {addr=} {bus=}")

  def test_default_controls_not_allowed(self):
    self.assertFalse(self.safety.get_controls_allowed())

  def test_manually_enable_controls_allowed(self):
    self.safety.set_controls_allowed(1)
    self.assertTrue(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(0)
    self.assertFalse(self.safety.get_controls_allowed())

  def test_prev_gas(self):
    self.assertFalse(self.safety.get_gas_pressed_prev())
    for pressed in [self.GAS_PRESSED_THRESHOLD + 1, 0]:
      self._rx(self._user_gas_msg(pressed))
      self.assertEqual(bool(pressed), self.safety.get_gas_pressed_prev())

  def test_allow_engage_with_gas_pressed(self):
    self._rx(self._user_gas_msg(1))
    self.safety.set_controls_allowed(True)
    self._rx(self._user_gas_msg(1))
    self.assertTrue(self.safety.get_controls_allowed())
    self._rx(self._user_gas_msg(1))
    self.assertTrue(self.safety.get_controls_allowed())

  def test_disengage_on_gas(self):
    self._rx(self._user_gas_msg(0))
    self.safety.set_controls_allowed(True)
    self._rx(self._user_gas_msg(self.GAS_PRESSED_THRESHOLD + 1))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_alternative_experience_no_disengage_on_gas(self):
    self._rx(self._user_gas_msg(0))
    self.safety.set_controls_allowed(True)
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.DISABLE_DISENGAGE_ON_GAS)
    self._rx(self._user_gas_msg(self.GAS_PRESSED_THRESHOLD + 1))
    # Test we allow lateral, but not longitudinal
    self.assertTrue(self.safety.get_controls_allowed())
    self.assertFalse(self.safety.get_longitudinal_allowed())
    # Make sure we can re-gain longitudinal actuation
    self._rx(self._user_gas_msg(0))
    self.assertTrue(self.safety.get_longitudinal_allowed())

  def test_prev_user_brake(self, _user_brake_msg=None, get_brake_pressed_prev=None):
    if _user_brake_msg is None:
      _user_brake_msg = self._user_brake_msg
      get_brake_pressed_prev = self.safety.get_brake_pressed_prev

    self.assertFalse(get_brake_pressed_prev())
    for pressed in [True, False]:
      self._rx(_user_brake_msg(not pressed))
      self.assertEqual(not pressed, get_brake_pressed_prev())
      self._rx(_user_brake_msg(pressed))
      self.assertEqual(pressed, get_brake_pressed_prev())

  def test_enable_control_allowed_from_cruise(self):
    self._rx(self._pcm_status_msg(False))
    self.assertFalse(self.safety.get_controls_allowed())
    self._rx(self._pcm_status_msg(True))
    self.assertTrue(self.safety.get_controls_allowed())

  def test_disable_control_allowed_from_cruise(self):
    self.safety.set_controls_allowed(1)
    self._rx(self._pcm_status_msg(False))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_cruise_engaged_prev(self):
    for engaged in [True, False]:
      self._rx(self._pcm_status_msg(engaged))
      self.assertEqual(engaged, self.safety.get_cruise_engaged_prev())
      self._rx(self._pcm_status_msg(not engaged))
      self.assertEqual(not engaged, self.safety.get_cruise_engaged_prev())

  def test_allow_user_brake_at_zero_speed(self, _user_brake_msg=None, get_brake_pressed_prev=None):
    if _user_brake_msg is None:
      _user_brake_msg = self._user_brake_msg

    # Brake was already pressed
    self._rx(self._vehicle_moving_msg(0))
    self._rx(_user_brake_msg(1))
    self.safety.set_controls_allowed(1)
    self._rx(_user_brake_msg(1))
    self.assertTrue(self.safety.get_controls_allowed())
    self.assertTrue(self.safety.get_longitudinal_allowed())
    self._rx(_user_brake_msg(0))
    self.assertTrue(self.safety.get_controls_allowed())
    self.assertTrue(self.safety.get_longitudinal_allowed())
    # rising edge of brake should disengage
    self._rx(_user_brake_msg(1))
    self.assertFalse(self.safety.get_controls_allowed())
    self.assertFalse(self.safety.get_longitudinal_allowed())
    self._rx(_user_brake_msg(0))  # reset no brakes

  def test_not_allow_user_brake_when_moving(self, _user_brake_msg=None, get_brake_pressed_prev=None):
    if _user_brake_msg is None:
      _user_brake_msg = self._user_brake_msg

    # Brake was already pressed
    self._rx(_user_brake_msg(1))
    self.safety.set_controls_allowed(1)
    self._rx(self._vehicle_moving_msg(self.STANDSTILL_THRESHOLD))
    self._rx(_user_brake_msg(1))
    self.assertTrue(self.safety.get_controls_allowed())
    self.assertTrue(self.safety.get_longitudinal_allowed())
    self._rx(self._vehicle_moving_msg(self.STANDSTILL_THRESHOLD + 1))
    self._rx(_user_brake_msg(1))
    self.assertFalse(self.safety.get_controls_allowed())
    self.assertFalse(self.safety.get_longitudinal_allowed())
    self._rx(self._vehicle_moving_msg(0))

  def test_vehicle_moving(self):
    self.assertFalse(self.safety.get_vehicle_moving())

    # not moving
    self.safety.safety_rx_hook(self._vehicle_moving_msg(0))
    self.assertFalse(self.safety.get_vehicle_moving())

    # speed is at threshold
    self.safety.safety_rx_hook(self._vehicle_moving_msg(self.STANDSTILL_THRESHOLD))
    self.assertFalse(self.safety.get_vehicle_moving())

    # past threshold
    self.safety.safety_rx_hook(self._vehicle_moving_msg(self.STANDSTILL_THRESHOLD + 1))
    self.assertTrue(self.safety.get_vehicle_moving())

  def test_tx_hook_on_wrong_safety_mode(self):
    files = os.listdir(os.path.dirname(os.path.realpath(__file__)))
    test_files = [f for f in files if f.startswith("test_") and f.endswith(".py")]

    current_test = self.__class__.__name__

    all_tx = []
    for tf in test_files:
      test = importlib.import_module("panda.tests.safety."+tf[:-3])
      for attr in dir(test):
        if attr.startswith("Test") and attr != current_test:
          tx = getattr(getattr(test, attr), "TX_MSGS")
          if tx is not None and not attr.endswith('Base'):
            # No point in comparing different Tesla safety modes
            if 'Tesla' in attr and 'Tesla' in current_test:
              continue
            if attr.startswith('TestToyota') and current_test.startswith('TestToyota'):
              continue
            if {attr, current_test}.issubset({'TestSubaruSafety', 'TestSubaruGen2Safety'}):
              continue
            if {attr, current_test}.issubset({'TestVolkswagenPqSafety', 'TestVolkswagenPqStockSafety', 'TestVolkswagenPqLongSafety'}):
              continue
            if {attr, current_test}.issubset({'TestGmCameraSafety', 'TestGmCameraLongitudinalSafety'}):
              continue
            if attr.startswith('TestHyundaiCanfd') and current_test.startswith('TestHyundaiCanfd'):
              continue
            if {attr, current_test}.issubset({'TestVolkswagenMqbSafety', 'TestVolkswagenMqbStockSafety', 'TestVolkswagenMqbLongSafety'}):
              continue

            # overlapping TX addrs, but they're not actuating messages for either car
            if attr == 'TestHyundaiCanfdHDA2LongEV' and current_test.startswith('TestToyota'):
              tx = list(filter(lambda m: m[0] not in [0x160, ], tx))

            # Volkswagen MQB longitudinal actuating message overlaps with the Subaru lateral actuating message
            if attr == 'TestVolkswagenMqbLongSafety' and current_test.startswith('TestSubaru'):
              tx = list(filter(lambda m: m[0] not in [0x122, ], tx))

            # Volkswagen MQB and Honda Nidec ACC HUD messages overlap
            if attr == 'TestVolkswagenMqbLongSafety' and current_test.startswith('TestHondaNidec'):
              tx = list(filter(lambda m: m[0] not in [0x30c, ], tx))

            # TODO: Temporary, should be fixed in panda firmware, safety_honda.h
            if attr.startswith('TestHonda'):
              # exceptions for common msgs across different hondas
              tx = list(filter(lambda m: m[0] not in [0x1FA, 0x30C, 0x33D], tx))
            all_tx.append(list([m[0], m[1], attr] for m in tx))

    # make sure we got all the msgs
    self.assertTrue(len(all_tx) >= len(test_files)-1)

    for tx_msgs in all_tx:
      for addr, bus, test_name in tx_msgs:
        msg = make_msg(bus, addr)
        self.safety.set_controls_allowed(1)
        # TODO: this should be blocked
        if current_test in ["TestNissanSafety", "TestNissanLeafSafety"] and [addr, bus] in self.TX_MSGS:
          continue
        self.assertFalse(self._tx(msg), f"transmit of {addr=:#x} {bus=} from {test_name} during {current_test} was allowed")
