#!/usr/bin/env python3
import sys
import numpy as np
import random
import unittest
import itertools

from panda import Panda
from panda.tests.libpanda import libpanda_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda


TOYOTA_COMMON_TX_MSGS = [[0x283, 0], [0x2E6, 0], [0x2E7, 0], [0x33E, 0], [0x344, 0], [0x365, 0], [0x366, 0], [0x4CB, 0],  # DSU bus 0
                         [0x128, 1], [0x141, 1], [0x160, 1], [0x161, 1], [0x470, 1],  # DSU bus 1
                         [0x2E4, 0], [0x191, 0], [0x411, 0], [0x412, 0], [0x343, 0], [0x1D2, 0],  # LKAS + ACC
                         [0x750, 0]]  # blindspot monitor


class TestToyotaSafetyBase(common.PandaCarSafetyTest, common.LongitudinalAccelSafetyTest):

  TX_MSGS = TOYOTA_COMMON_TX_MSGS
  STANDSTILL_THRESHOLD = 0  # kph
  RELAY_MALFUNCTION_ADDRS = {0: (0x2E4,)}
  FWD_BLACKLISTED_ADDRS = {2: [0x2E4, 0x412, 0x191, 0x343]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}
  INTERCEPTOR_THRESHOLD = 805
  EPS_SCALE = 73

  cnt_gas_cmd = 0
  cnt_user_gas = 0

  packer: CANPackerPanda
  safety: libpanda_py.Panda

  @classmethod
  def setUpClass(cls):
    if cls.__name__.endswith("Base"):
      cls.packer = None
      cls.safety = None
      raise unittest.SkipTest

  def _torque_meas_msg(self, torque: int, driver_torque: int | None = None):
    values = {"STEER_TORQUE_EPS": (torque / self.EPS_SCALE) * 100.}
    if driver_torque is not None:
      values["STEER_TORQUE_DRIVER"] = driver_torque
    return self.packer.make_can_msg_panda("STEER_TORQUE_SENSOR", 0, values)

  # Both torque and angle safety modes test with each other's steering commands
  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"STEER_TORQUE_CMD": torque, "STEER_REQUEST": steer_req}
    return self.packer.make_can_msg_panda("STEERING_LKA", 0, values)

  def _angle_meas_msg(self, angle: float, steer_angle_initializing: bool = False):
    # This creates a steering torque angle message. Not set on all platforms,
    # relative to init angle on some older TSS2 platforms. Only to be used with LTA
    values = {"STEER_ANGLE": angle, "STEER_ANGLE_INITIALIZING": int(steer_angle_initializing)}
    return self.packer.make_can_msg_panda("STEER_TORQUE_SENSOR", 0, values)

  def _angle_cmd_msg(self, angle: float, enabled: bool):
    return self._lta_msg(int(enabled), int(enabled), angle, torque_wind_down=100 if enabled else 0)

  def _lta_msg(self, req, req2, angle_cmd, torque_wind_down=100):
    values = {"STEER_REQUEST": req, "STEER_REQUEST_2": req2, "STEER_ANGLE_CMD": angle_cmd, "TORQUE_WIND_DOWN": torque_wind_down}
    return self.packer.make_can_msg_panda("STEERING_LTA", 0, values)

  def _accel_msg(self, accel, cancel_req=0):
    values = {"ACCEL_CMD": accel, "CANCEL_REQ": cancel_req}
    return self.packer.make_can_msg_panda("ACC_CONTROL", 0, values)

  def _speed_msg(self, speed):
    values = {("WHEEL_SPEED_%s" % n): speed * 3.6 for n in ["FR", "FL", "RR", "RL"]}
    return self.packer.make_can_msg_panda("WHEEL_SPEEDS", 0, values)

  def _user_brake_msg(self, brake):
    values = {"BRAKE_PRESSED": brake}
    return self.packer.make_can_msg_panda("BRAKE_MODULE", 0, values)

  def _user_gas_msg(self, gas):
    cruise_active = self.safety.get_controls_allowed()
    values = {"GAS_RELEASED": not gas, "CRUISE_ACTIVE": cruise_active}
    return self.packer.make_can_msg_panda("PCM_CRUISE", 0, values)

  def _pcm_status_msg(self, enable):
    values = {"CRUISE_ACTIVE": enable}
    return self.packer.make_can_msg_panda("PCM_CRUISE", 0, values)

  def test_block_aeb(self):
    for controls_allowed in (True, False):
      for bad in (True, False):
        for _ in range(10):
          self.safety.set_controls_allowed(controls_allowed)
          dat = [random.randint(1, 255) for _ in range(7)]
          if not bad:
            dat = [0]*6 + dat[-1:]
          msg = libpanda_py.make_CANPacket(0x283, 0, bytes(dat))
          self.assertEqual(not bad, self._tx(msg))

  # Only allow LTA msgs with no actuation
  def test_lta_steer_cmd(self):
    for engaged, req, req2, torque_wind_down, angle in itertools.product([True, False],
                                                                  [0, 1], [0, 1],
                                                                  [0, 50, 100],
                                                                  np.linspace(-20, 20, 5)):
      self.safety.set_controls_allowed(engaged)

      should_tx = not req and not req2 and angle == 0 and torque_wind_down == 0
      self.assertEqual(should_tx, self._tx(self._lta_msg(req, req2, angle, torque_wind_down)))

  def test_rx_hook(self):
    # checksum checks
    for msg in ["trq", "pcm"]:
      self.safety.set_controls_allowed(1)
      if msg == "trq":
        to_push = self._torque_meas_msg(0)
      if msg == "pcm":
        to_push = self._pcm_status_msg(True)
      self.assertTrue(self._rx(to_push))
      to_push[0].data[4] = 0
      to_push[0].data[5] = 0
      to_push[0].data[6] = 0
      to_push[0].data[7] = 0
      self.assertFalse(self._rx(to_push))
      self.assertFalse(self.safety.get_controls_allowed())


class TestToyotaSafetyInterceptorBase(TestToyotaSafetyBase, common.InterceptorSafetyTest):

  TX_MSGS = TOYOTA_COMMON_TX_MSGS + [[0x200, 0]]

  # Skip non-interceptor user gas tests
  def test_prev_gas(self):
    pass

  def test_disengage_on_gas(self):
    pass

  def test_alternative_experience_no_disengage_on_gas(self):
    pass


# def create_interceptor_test(base_class):
#   print('base_class', base_class)
#   """Creates a version of the base class testing the interceptor"""
#   name = f"{base_class.__name__}Interceptor"
#   def newSetUp(self):
#     print('self', self)
#     base_class.setUp(self)
#     self.safety.set_safety_hooks(Panda.SAFETY_TOYOTA, self.safety.get_current_safety_param() |
#                                  Panda.FLAG_TOYOTA_GAS_INTERCEPTOR)
#     self.safety.init_tests()
#
#   new_class = type(name, (base_class, TestToyotaSafetyInterceptorBase), {})
#   new_class.setUp = newSetUp
#   globals()[f"{__name__}.{name}"] = new_class
#   return base_class


def create_interceptor_test_new(safety_param, safety_test):
  """
  A new unittest.TestCase class is derived from the wrapped class and the passed in safety_test class.
  The safety_param argument is logical or'd with the existing safety params.
  """

  def wrapper(base_class):
    print('base_class', base_class)
    """Creates a version of the base class testing the interceptor"""
    name = f"{base_class.__name__}Interceptor"

    def newSetUp(self):
      base_class.setUp(self)
      self.safety.set_safety_hooks(self.safety.get_current_safety_mode(),
                                   self.safety.get_current_safety_param() | safety_param)
      self.safety.init_tests()

    new_class = type(name, (base_class, safety_test), {})
    new_class.setUp = newSetUp
    globals()[f"{__name__}.{name}"] = new_class
    return base_class

  return wrapper


  # print(base_class)
  # """Creates a version of the base class testing the interceptor"""
  # name = f"{base_class.__name__}Interceptor"
  # def newSetUp(thing, self):
  #   print('newSetUp', thing)
  #   base_class.setUp(self)
  #   self.safety.set_safety_hooks(Panda.SAFETY_TOYOTA, self.safety.get_current_safety_param() |
  #                                Panda.FLAG_TOYOTA_GAS_INTERCEPTOR)
  #   self.safety.init_tests()
  #
  # new_class = type(name, (base_class, TestToyotaSafetyInterceptorBase), {})
  # new_class.setUp = newSetUp
  # globals()[f"{__name__}.{name}"] = new_class
  # return base_class


# @create_interceptor_test
@create_interceptor_test_new(Panda.FLAG_TOYOTA_GAS_INTERCEPTOR, TestToyotaSafetyInterceptorBase)
class TestToyotaSafetyTorque(TestToyotaSafetyBase, common.MotorTorqueSteeringSafetyTest, common.SteerRequestCutSafetyTest):

  MAX_RATE_UP = 15
  MAX_RATE_DOWN = 25
  MAX_TORQUE = 1500
  MAX_RT_DELTA = 450
  RT_INTERVAL = 250000
  MAX_TORQUE_ERROR = 350
  TORQUE_MEAS_TOLERANCE = 1  # toyota safety adds one to be conservative for rounding

  # Safety around steering req bit
  MIN_VALID_STEERING_FRAMES = 18
  MAX_INVALID_STEERING_FRAMES = 1
  MIN_VALID_STEERING_RT_INTERVAL = 170000  # a ~10% buffer, can send steer up to 110Hz

  def setUp(self):
    self.packer = CANPackerPanda("toyota_nodsu_pt_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_TOYOTA, self.EPS_SCALE)
    self.safety.init_tests()


@create_interceptor_test
class TestToyotaSafetyAngle(TestToyotaSafetyBase, common.AngleSteeringSafetyTest):

  # Angle control limits
  DEG_TO_CAN = 17.452007  # 1 / 0.0573 deg to can

  ANGLE_RATE_BP = [5., 25., 25.]
  ANGLE_RATE_UP = [0.3, 0.15, 0.15]  # windup limit
  ANGLE_RATE_DOWN = [0.36, 0.26, 0.26]  # unwind limit

  MAX_LTA_ANGLE = 94.9461  # PCS faults if commanding above this, deg
  MAX_MEAS_TORQUE = 1500  # max allowed measured EPS torque before wind down
  MAX_LTA_DRIVER_TORQUE = 150  # max allowed driver torque before wind down

  def setUp(self):
    self.packer = CANPackerPanda("toyota_nodsu_pt_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_TOYOTA, self.EPS_SCALE | Panda.FLAG_TOYOTA_LTA)
    self.safety.init_tests()

  # Only allow LKA msgs with no actuation
  def test_lka_steer_cmd(self):
    for engaged, steer_req, torque in itertools.product([True, False],
                                                        [0, 1],
                                                        np.linspace(-1500, 1500, 7)):
      self.safety.set_controls_allowed(engaged)
      torque = int(torque)
      self.safety.set_rt_torque_last(torque)
      self.safety.set_torque_meas(torque, torque)
      self.safety.set_desired_torque_last(torque)

      should_tx = not steer_req and torque == 0
      self.assertEqual(should_tx, self._tx(self._torque_cmd_msg(torque, steer_req)))

  def test_lta_steer_cmd(self):
    """
    Tests the LTA steering command message
    controls_allowed:
    * STEER_REQUEST and STEER_REQUEST_2 do not mismatch
    * TORQUE_WIND_DOWN is only set to 0 or 100 when STEER_REQUEST and STEER_REQUEST_2 are both 1
    * Full torque messages are blocked if either EPS torque or driver torque is above the threshold

    not controls_allowed:
    * STEER_REQUEST, STEER_REQUEST_2, and TORQUE_WIND_DOWN are all 0
    """
    for controls_allowed in (True, False):
      for angle in np.arange(-90, 90, 1):
        self.safety.set_controls_allowed(controls_allowed)
        self._reset_angle_measurement(angle)
        self._set_prev_desired_angle(angle)

        self.assertTrue(self._tx(self._lta_msg(0, 0, angle, 0)))
        if controls_allowed:
          # Test the two steer request bits and TORQUE_WIND_DOWN torque wind down signal
          for req, req2, torque_wind_down in itertools.product([0, 1], [0, 1], [0, 50, 100]):
            mismatch = not (req or req2) and torque_wind_down != 0
            should_tx = req == req2 and (torque_wind_down in (0, 100)) and not mismatch
            self.assertEqual(should_tx, self._tx(self._lta_msg(req, req2, angle, torque_wind_down)))

          # Test max EPS torque and driver override thresholds
          cases = itertools.product(
            (0, self.MAX_MEAS_TORQUE - 1, self.MAX_MEAS_TORQUE, self.MAX_MEAS_TORQUE + 1, self.MAX_MEAS_TORQUE * 2),
            (0, self.MAX_LTA_DRIVER_TORQUE - 1, self.MAX_LTA_DRIVER_TORQUE, self.MAX_LTA_DRIVER_TORQUE + 1, self.MAX_LTA_DRIVER_TORQUE * 2)
          )

          for eps_torque, driver_torque in cases:
            for sign in (-1, 1):
              for _ in range(6):
                self._rx(self._torque_meas_msg(sign * eps_torque, sign * driver_torque))

              # Toyota adds 1 to EPS torque since it is rounded after EPS factor
              should_tx = (eps_torque - 1) <= self.MAX_MEAS_TORQUE and driver_torque <= self.MAX_LTA_DRIVER_TORQUE
              self.assertEqual(should_tx, self._tx(self._lta_msg(1, 1, angle, 100)))
              self.assertTrue(self._tx(self._lta_msg(1, 1, angle, 0)))  # should tx if we wind down torque

        else:
          # Controls not allowed
          for req, req2, torque_wind_down in itertools.product([0, 1], [0, 1], [0, 50, 100]):
            should_tx = not (req or req2) and torque_wind_down == 0
            self.assertEqual(should_tx, self._tx(self._lta_msg(req, req2, angle, torque_wind_down)))

  def test_steering_angle_measurements(self, max_angle=None):
    # Measurement test tests max angle + 0.5 which will fail
    super().test_steering_angle_measurements(max_angle=self.MAX_LTA_ANGLE - 0.5)

  def test_angle_cmd_when_enabled(self, max_angle=None):
    super().test_angle_cmd_when_enabled(max_angle=self.MAX_LTA_ANGLE)

  def test_angle_measurements(self):
    """
    * Tests angle meas quality flag dictates whether angle measurement is parsed, and if rx is valid
    * Tests rx hook correctly clips the angle measurement, since it is to be compared to LTA cmd when inactive
    """
    for steer_angle_initializing in (True, False):
      for angle in np.arange(0, self.MAX_LTA_ANGLE * 2, 1):
        # If init flag is set, do not rx or parse any angle measurements
        for a in (angle, -angle, 0, 0, 0, 0):
          self.assertEqual(not steer_angle_initializing,
                           self._rx(self._angle_meas_msg(a, steer_angle_initializing)))

        final_angle = (0 if steer_angle_initializing else
                       round(min(angle, self.MAX_LTA_ANGLE) * self.DEG_TO_CAN))
        self.assertEqual(self.safety.get_angle_meas_min(), -final_angle)
        self.assertEqual(self.safety.get_angle_meas_max(), final_angle)

        self._rx(self._angle_meas_msg(0))
        self.assertEqual(self.safety.get_angle_meas_min(), -final_angle)
        self.assertEqual(self.safety.get_angle_meas_max(), 0)

        self._rx(self._angle_meas_msg(0))
        self.assertEqual(self.safety.get_angle_meas_min(), 0)
        self.assertEqual(self.safety.get_angle_meas_max(), 0)


@create_interceptor_test
class TestToyotaAltBrakeSafety(TestToyotaSafetyTorque):

  def setUp(self):
    self.packer = CANPackerPanda("toyota_new_mc_pt_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_TOYOTA, self.EPS_SCALE | Panda.FLAG_TOYOTA_ALT_BRAKE)
    self.safety.init_tests()

  def _user_brake_msg(self, brake):
    values = {"BRAKE_PRESSED": brake}
    return self.packer.make_can_msg_panda("BRAKE_MODULE", 0, values)

  # No LTA message in the DBC
  def test_lta_steer_cmd(self):
    pass


class TestToyotaStockLongitudinalBase(TestToyotaSafetyBase):

  # Base fwd addresses minus ACC_CONTROL (0x343)
  FWD_BLACKLISTED_ADDRS = {2: [0x2E4, 0x412, 0x191]}

  def test_accel_actuation_limits(self, stock_longitudinal=True):
    super().test_accel_actuation_limits(stock_longitudinal=stock_longitudinal)

  def test_acc_cancel(self):
    """
      Regardless of controls allowed, never allow ACC_CONTROL if cancel bit isn't set
    """
    for controls_allowed in [True, False]:
      self.safety.set_controls_allowed(controls_allowed)
      for accel in np.arange(self.MIN_ACCEL - 1, self.MAX_ACCEL + 1, 0.1):
        self.assertFalse(self._tx(self._accel_msg(accel)))
        should_tx = np.isclose(accel, 0, atol=0.0001)
        self.assertEqual(should_tx, self._tx(self._accel_msg(accel, cancel_req=1)))


class TestToyotaStockLongitudinalTorque(TestToyotaStockLongitudinalBase, TestToyotaSafetyTorque):

  def setUp(self):
    self.packer = CANPackerPanda("toyota_nodsu_pt_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_TOYOTA, self.EPS_SCALE | Panda.FLAG_TOYOTA_STOCK_LONGITUDINAL)
    self.safety.init_tests()


class TestToyotaStockLongitudinalAngle(TestToyotaStockLongitudinalBase, TestToyotaSafetyAngle):

  def setUp(self):
    self.packer = CANPackerPanda("toyota_nodsu_pt_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_TOYOTA, self.EPS_SCALE | Panda.FLAG_TOYOTA_STOCK_LONGITUDINAL | Panda.FLAG_TOYOTA_LTA)
    self.safety.init_tests()


if __name__ == "__main__":
  unittest.main()
