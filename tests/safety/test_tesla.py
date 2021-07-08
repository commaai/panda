#!/usr/bin/env python3
import unittest
import numpy as np
from panda import Panda
from panda.tests.safety import libpandasafety_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda

ANGLE_DELTA_BP = [0., 5., 15.]
ANGLE_DELTA_V = [5., .8, .15]     # windup limit
ANGLE_DELTA_VU = [5., 3.5, 0.4]   # unwind limit

class CONTROL_LEVER_STATE:
  DN_1ST = 32
  UP_1ST = 16
  DN_2ND = 8
  UP_2ND = 4
  RWD = 2
  FWD = 1
  IDLE = 0

def sign(a):
  return 1 if a > 0 else -1

class TestTeslaSafety(common.PandaSafetyTest):
  TX_MSGS = [[0x488, 0], [0x45, 0], [0x45, 2]]
  STANDSTILL_THRESHOLD = 0
  GAS_PRESSED_THRESHOLD = 3
  RELAY_MALFUNCTION_ADDR = 0x488
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {2: [0x488]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  def setUp(self):
    self.packer = CANPackerPanda("tesla_can")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_TESLA, 0)
    self.safety.init_tests()

  def _angle_meas_msg(self, angle):
    values = {"EPAS_internalSAS": angle}
    return self.packer.make_can_msg_panda("EPAS_sysStatus", 0, values)

  def _set_prev_angle(self, t):
    t = int(t * 10)
    self.safety.set_desired_angle_last(t)

  def _angle_meas_msg_array(self, angle):
    for _ in range(6):
      self._rx(self._angle_meas_msg(angle))

  def _pcm_status_msg(self, enable):
    values = {"DI_cruiseState": 2 if enable else 0}
    return self.packer.make_can_msg_panda("DI_state", 0, values)

  def _lkas_control_msg(self, angle, enabled):
    values = {"DAS_steeringAngleRequest": angle, "DAS_steeringControlType": 1 if enabled else 0}
    return self.packer.make_can_msg_panda("DAS_steeringControl", 0, values)

  def _speed_msg(self, speed):
    values = {"ESP_vehicleSpeed": speed * 3.6}
    return self.packer.make_can_msg_panda("ESP_B", 0, values)

  def _brake_msg(self, brake):
    values = {"driverBrakeStatus": 2 if brake else 1}
    return self.packer.make_can_msg_panda("BrakeMessage", 0, values)

  def _gas_msg(self, gas):
    values = {"DI_pedalPos": gas}
    return self.packer.make_can_msg_panda("DI_torque1", 0, values)

  def _control_lever_cmd(self, command):
    values = {"SpdCtrlLvr_Stat": command}
    return self.packer.make_can_msg_panda("STW_ACTN_RQ", 0, values)

  def _autopilot_status_msg(self, status):
    values = {"autopilotStatus": status}
    return self.packer.make_can_msg_panda("AutopilotStatus", 2, values)

  def test_angle_cmd_when_enabled(self):
    # when controls are allowed, angle cmd rate limit is enforced
    speeds = [0., 1., 5., 10., 15., 50.]
    angles = [-300, -100, -10, 0, 10, 100, 300]
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
        # Up
        self.assertEqual(True, self._tx(self._lkas_control_msg(a + sign(a) * max_delta_up, 1)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Don't change
        self.assertEqual(True, self._tx(self._lkas_control_msg(a, 1)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Down
        self.assertEqual(True, self._tx(self._lkas_control_msg(a - sign(a) * max_delta_down, 1)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Inject too high rates
        # Up
        self.assertEqual(False, self._tx(self._lkas_control_msg(a + sign(a) * (max_delta_up + 1.1), 1)))
        self.assertFalse(self.safety.get_controls_allowed())

        # Don't change
        self.safety.set_controls_allowed(1)
        self._set_prev_angle(a)
        self.assertTrue(self.safety.get_controls_allowed())
        self.assertEqual(True, self._tx(self._lkas_control_msg(a, 1)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Down
        self.assertEqual(False, self._tx(self._lkas_control_msg(a - sign(a) * (max_delta_down + 1.1), 1)))
        self.assertFalse(self.safety.get_controls_allowed())

        # Check desired steer should be the same as steer angle when controls are off
        self.safety.set_controls_allowed(0)
        self.assertEqual(True, self._tx(self._lkas_control_msg(a, 0)))

  def test_angle_cmd_when_disabled(self):
    self.safety.set_controls_allowed(0)

    self._set_prev_angle(0)
    self.assertFalse(self._tx(self._lkas_control_msg(0, 1)))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_acc_buttons(self):
    self.safety.set_controls_allowed(1)
    self._tx(self._control_lever_cmd(CONTROL_LEVER_STATE.FWD))
    self.assertTrue(self.safety.get_controls_allowed())
    self._tx(self._control_lever_cmd(CONTROL_LEVER_STATE.RWD))
    self.assertFalse(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(1)
    self._tx(self._control_lever_cmd(CONTROL_LEVER_STATE.UP_1ST))
    self.assertFalse(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(1)
    self._tx(self._control_lever_cmd(CONTROL_LEVER_STATE.UP_2ND))
    self.assertFalse(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(1)
    self._tx(self._control_lever_cmd(CONTROL_LEVER_STATE.DN_1ST))
    self.assertFalse(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(1)
    self._tx(self._control_lever_cmd(CONTROL_LEVER_STATE.DN_2ND))
    self.assertFalse(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(1)
    self._tx(self._control_lever_cmd(CONTROL_LEVER_STATE.IDLE))
    self.assertTrue(self.safety.get_controls_allowed())

  def test_autopilot_passthrough(self):
    for ap_status in range(16):
      self.safety.set_controls_allowed(1)
      self._rx(self._autopilot_status_msg(ap_status))
      self.assertEqual(self.safety.get_controls_allowed(), (ap_status not in [3, 4, 5]))

if __name__ == "__main__":
  unittest.main()
