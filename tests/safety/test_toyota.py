#!/usr/bin/env python3
import numpy as np
import random
import unittest
import itertools

from panda import Panda
from panda.tests.libpanda import libpanda_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda, make_msg


def interceptor_msg(gas, addr):
  to_send = make_msg(0, addr, 6)
  to_send[0].data[0] = (gas & 0xFF00) >> 8
  to_send[0].data[1] = gas & 0xFF
  to_send[0].data[2] = (gas & 0xFF00) >> 8
  to_send[0].data[3] = gas & 0xFF
  return to_send


class TestToyotaSafetyBase(common.PandaSafetyTest, common.InterceptorSafetyTest,
                           common.LongitudinalAccelSafetyTest):

  TX_MSGS = [[0x283, 0], [0x2E6, 0], [0x2E7, 0], [0x33E, 0], [0x344, 0], [0x365, 0], [0x366, 0], [0x4CB, 0],  # DSU bus 0
             [0x128, 1], [0x141, 1], [0x160, 1], [0x161, 1], [0x470, 1],  # DSU bus 1
             [0x2E4, 0], [0x191, 0], [0x411, 0], [0x412, 0], [0x343, 0], [0x1D2, 0],  # LKAS + ACC
             [0x200, 0], [0x750, 0]]  # interceptor + blindspot monitor
  STANDSTILL_THRESHOLD = 0  # kph
  RELAY_MALFUNCTION_ADDR = 0x2E4
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {2: [0x2E4, 0x412, 0x191, 0x343]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}
  INTERCEPTOR_THRESHOLD = 805
  EPS_SCALE = 73

  @classmethod
  def setUpClass(cls):
    if cls.__name__.endswith("Base"):
      cls.packer = None
      cls.safety = None
      raise unittest.SkipTest

  def _torque_meas_msg(self, torque):
    values = {"STEER_TORQUE_EPS": (torque / self.EPS_SCALE) * 100.}
    return self.packer.make_can_msg_panda("STEER_TORQUE_SENSOR", 0, values)

  # Both torque and angle safety modes test with each other's steering commands
  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"STEER_TORQUE_CMD": torque, "STEER_REQUEST": steer_req}
    return self.packer.make_can_msg_panda("STEERING_LKA", 0, values)

  def _lta_msg(self, req, req2, angle_cmd, setme_x64=100):
    values = {"STEER_REQUEST": req, "STEER_REQUEST_2": req2, "STEER_ANGLE_CMD": angle_cmd, "SETME_X64": setme_x64}
    return self.packer.make_can_msg_panda("STEERING_LTA", 0, values)

  def _accel_msg(self, accel, cancel_req=0):
    values = {"ACCEL_CMD": accel, "CANCEL_REQ": cancel_req}
    return self.packer.make_can_msg_panda("ACC_CONTROL", 0, values)

  def _speed_msg(self, speed):
    values = {("WHEEL_SPEED_%s" % n): speed for n in ["FR", "FL", "RR", "RL"]}
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

  def _interceptor_gas_cmd(self, gas):
    return interceptor_msg(gas, 0x200)

  def _interceptor_user_gas(self, gas):
    return interceptor_msg(gas, 0x201)

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
    for engaged, req, req2, setme_x64, angle in itertools.product([True, False],
                                                                  [0, 1], [0, 1],
                                                                  [0, 50, 100],
                                                                  np.linspace(-20, 20, 5)):
      self.safety.set_controls_allowed(engaged)

      should_tx = not req and not req2 and angle == 0 and setme_x64 == 0
      self.assertEqual(should_tx, self._tx(self._lta_msg(req, req2, angle, setme_x64)))

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


class TestToyotaSafetyTorque(TestToyotaSafetyBase, common.MotorTorqueSteeringSafetyTest):

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


class TestToyotaSafetyAngle(TestToyotaSafetyBase):

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

      should_tx = not steer_req and torque == 0
      self.assertEqual(should_tx, self._tx(self._torque_cmd_msg(torque, steer_req)))


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
