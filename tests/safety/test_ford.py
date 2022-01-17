#!/usr/bin/env python3
import unittest
import numpy as np

import panda.tests.safety.common as common

from panda import Panda
from panda.tests.safety import libpandasafety_py
from panda.tests.safety.common import CANPackerPanda

DEG_TO_RAD = np.pi / 180.

# Signal: LatCtlPath_An_Actl
# Factor: 0.0005
FORD_RAD_TO_CAN = 2000.0
FORD_DEG_TO_CAN = DEG_TO_RAD * FORD_RAD_TO_CAN

ANGLE_DELTA_BP = [2., 7., 17.]
ANGLE_DELTA_V = [5., .8, .25]     # windup limit
ANGLE_DELTA_VU = [5., 3.5, .8]   # unwind limit

MSG_STEERING_PINION_DATA = 0x07E      # RX from PSCM, for steering pinion angle
MSG_ENG_BRAKE_DATA = 0x165            # RX from PCM, for driver brake pedal and cruise state
MSG_ENG_VEHICLE_SP_THROTTLE2 = 0x202  # RX from PCM, for vehicle speed
MSG_ENG_VEHICLE_SP_THROTTLE = 0x204   # RX from PCM, for driver throttle input
MSG_STEERING_DATA_FD1 = 0x083         # TX by OP, ACC control buttons for cancel
MSG_LANE_ASSIST_DATA1 = 0x3CA         # TX by OP, Lane Keeping Assist
MSG_LATERAL_MOTION_CONTROL = 0x3D3    # TX by OP, Lane Centering Assist
MSG_IPMA_DATA = 0x3D8                 # TX by OP, IPMA HUD user interface


def sign(a):
  return 1 if a > 0 else -1


def clamp(n, low, high):
  return max(low, min(n, high))


class TestFordSafety(common.PandaSafetyTest):
  STANDSTILL_THRESHOLD = 1
  GAS_PRESSED_THRESHOLD = 3
  RELAY_MALFUNCTION_BUS = 0
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  def setUp(self):
    self.packer = None
    raise unittest.SkipTest

  # Driver brake pedal
  def _brake_msg(self, brake):
    # brake pedal and cruise state share same message, so we have to send
    # the other signal too (use prev value)
    enable = self.safety.get_controls_allowed()
    values = {
      "BpedDrvAppl_D_Actl": 2 if brake else 1,
      "CcStat_D_Actl": 5 if enable else 0,
    }
    return self.packer.make_can_msg_panda("EngBrakeData", 0, values)

  # Vehicle speed
  def _speed_msg(self, speed):
    values = {"Veh_V_ActlEng": speed}
    return self.packer.make_can_msg_panda("EngVehicleSpThrottle2", 0, values)

  # Drive throttle input
  def _gas_msg(self, gas):
    values = {"ApedPos_Pc_ActlArb": gas}
    return self.packer.make_can_msg_panda("EngVehicleSpThrottle", 0, values)

  # Cruise status
  def _pcm_status_msg(self, enable):
    # brake pedal and cruise state share same message, so we have to send
    # the other signal too (use prev value)
    brake = self.safety.get_brake_pressed_prev()
    values = {
      "BpedDrvAppl_D_Actl": 2 if brake else 1,
      "CcStat_D_Actl": 5 if enable else 0,
    }
    return self.packer.make_can_msg_panda("EngBrakeData", 0, values)

  # Steering wheel angle
  def _angle_meas_msg(self, angle):
    values = {"StePinComp_An_Est": angle}
    return self.packer.make_can_msg_panda("SteeringPinion_Data", 0, values)

  def _set_prev_angle(self, t):
    self.safety.set_desired_angle_last(int(t * FORD_DEG_TO_CAN))

  def _angle_meas_msg_array(self, angle):
    for _ in range(6):
      self._rx(self._angle_meas_msg(angle))

  # Lane Centering Assist command
  def _lkas_control_msg(self, angle, enabled):
    values = {
      "LatCtl_D_Rq": 1 if enabled else 0,
      "LatCtlPath_An_Actl": clamp(angle * DEG_TO_RAD, -0.5, 0.5),  # FIXME if DBC changes
    }
    return self.packer.make_can_msg_panda("LateralMotionControl", 0, values)

  def _acc_button_msg(self, cancel=0, resume=0, _set=0):
    values = {
      "CcAslButtnCnclPress": cancel,
      "CcAsllButtnResPress": resume,
      "CcAslButtnSetPress": _set,
    }
    return self.packer.make_can_msg_panda("Steering_Data_FD1", 0, values)


class TestFordSteeringSafety(TestFordSafety):
  TX_MSGS = [[MSG_STEERING_DATA_FD1, 0], [MSG_LANE_ASSIST_DATA1, 0], [MSG_LATERAL_MOTION_CONTROL, 0], [MSG_IPMA_DATA, 0]]
  RELAY_MALFUNCTION_ADDR = MSG_LANE_ASSIST_DATA1
  FWD_BLACKLISTED_ADDRS = {2: [MSG_LANE_ASSIST_DATA1, MSG_LATERAL_MOTION_CONTROL, MSG_IPMA_DATA]}

  def setUp(self):
    self.packer = CANPackerPanda("ford_lincoln_base_pt")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_FORD, 0)
    self.safety.init_tests()

  def test_angle_cmd_when_enabled(self):
    # when controls are allowed, angle cmd rate limit is enforced
    speeds = [0., 1., 5., 10., 15., 50.]
    # angles = [-300, -100, -10, 0, 10, 100, 300]
    angles = [-23, -10, 0, 10, 23]
    for a in angles:
      for s in speeds:
        max_delta_up = np.interp(s, ANGLE_DELTA_BP, ANGLE_DELTA_V)
        max_delta_down = np.interp(s, ANGLE_DELTA_BP, ANGLE_DELTA_VU)

        # first test against false positives
        self._angle_meas_msg_array(a)
        self._rx(self._speed_msg(s))

        self._set_prev_angle(a)
        self.safety.set_controls_allowed(1)

        # Stay within limits
        # Don't change
        self.assertTrue(self._tx(self._lkas_control_msg(a, 1)))

        # Up
        self.assertTrue(self._tx(self._lkas_control_msg(a + sign(a) * max_delta_up, 1)))

        # Down
        self._set_prev_angle(a)
        self.assertTrue(self._tx(self._lkas_control_msg(a - sign(a) * max_delta_down, 1)))

        # Inject too high rates
        # Up
        self._set_prev_angle(a)
        self.assertFalse(self._tx(self._lkas_control_msg(a + sign(a) * (max_delta_up + 1.1), 1)))

        # Don't change
        self._set_prev_angle(a)
        self.assertEqual(True, self._tx(self._lkas_control_msg(a, 1)))

        # Down
        self.assertEqual(False, self._tx(self._lkas_control_msg(a - sign(a) * (max_delta_down + 1.1), 1)))

        # TODO: ford safety doesn't check for this yet
        # # Check desired steer should be the same as steer angle when controls are off
        # self.safety.set_controls_allowed(0)
        # self.assertEqual(True, self._tx(self._lkas_control_msg(a, 0)))

  def test_steer_when_disabled(self):
    self.safety.set_controls_allowed(0)

    self._set_prev_angle(0)
    self.assertFalse(self._tx(self._lkas_control_msg(0, 1)))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_spam_cancel_safety_check(self):
    self.safety.set_controls_allowed(1)
    self._tx(self._acc_button_msg(cancel=1))
    self.assertTrue(self.safety.get_controls_allowed())
    self._tx(self._acc_button_msg(resume=1))
    self.assertFalse(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(1)
    self._tx(self._acc_button_msg(_set=1))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_rx_hook(self):
    # TODO: test_rx_hook
    pass


if __name__ == "__main__":
  unittest.main()
