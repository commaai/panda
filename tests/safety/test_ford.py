#!/usr/bin/env python3
import unittest

import panda.tests.safety.common as common

from panda import Panda
from panda.tests.safety import libpandasafety_py
from panda.tests.safety.common import CANPackerPanda

MSG_ENG_BRAKE_DATA = 0x165            # RX from PCM, for driver brake pedal and cruise state
MSG_ENG_VEHICLE_SP_THROTTLE2 = 0x202  # RX from PCM, for vehicle speed
MSG_ENG_VEHICLE_SP_THROTTLE = 0x204   # RX from PCM, for driver throttle input
MSG_STEERING_DATA_FD1 = 0x083         # TX by OP, ACC control buttons for cancel
MSG_LANE_ASSIST_DATA1 = 0x3CA         # TX by OP, Lane Keeping Assist
MSG_LATERAL_MOTION_CONTROL = 0x3D3    # TX by OP, Lane Centering Assist
MSG_IPMA_DATA = 0x3D8                 # TX by OP, IPMA HUD user interface


class TestFordSafety(common.PandaSafetyTest):
  STANDSTILL_THRESHOLD = 1
  RELAY_MALFUNCTION_ADDR = MSG_IPMA_DATA
  RELAY_MALFUNCTION_BUS = 0

  TX_MSGS = [[MSG_STEERING_DATA_FD1, 0], [MSG_LANE_ASSIST_DATA1, 0], [MSG_LATERAL_MOTION_CONTROL, 0], [MSG_IPMA_DATA, 0]]
  FWD_BLACKLISTED_ADDRS = {2: [MSG_LANE_ASSIST_DATA1, MSG_LATERAL_MOTION_CONTROL, MSG_IPMA_DATA]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  def setUp(self):
    self.packer = CANPackerPanda("ford_lincoln_base_pt")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_FORD, 0)
    self.safety.init_tests()

  # Driver brake pedal
  def _user_brake_msg(self, brake: bool):
    # brake pedal and cruise state share same message, so we have to send
    # the other signal too
    enable = self.safety.get_controls_allowed()
    values = {
      "BpedDrvAppl_D_Actl": 2 if brake else 1,
      "CcStat_D_Actl": 5 if enable else 0,
    }
    return self.packer.make_can_msg_panda("EngBrakeData", 0, values)

  # Standstill state
  def _speed_msg(self, speed: float):
    values = {"VehStop_D_Stat": 1 if speed <= self.STANDSTILL_THRESHOLD else 0}
    return self.packer.make_can_msg_panda("DesiredTorqBrk", 0, values)

  # Drive throttle input
  def _user_gas_msg(self, gas: float):
    values = {"ApedPos_Pc_ActlArb": gas}
    return self.packer.make_can_msg_panda("EngVehicleSpThrottle", 0, values)

  # Cruise status
  def _pcm_status_msg(self, enable: bool):
    # brake pedal and cruise state share same message, so we have to send
    # the other signal too
    brake = self.safety.get_brake_pressed_prev()
    values = {
      "BpedDrvAppl_D_Actl": 2 if brake else 1,
      "CcStat_D_Actl": 5 if enable else 0,
    }
    return self.packer.make_can_msg_panda("EngBrakeData", 0, values)

  # LKAS command
  def _lkas_command_msg(self, action: int):
    values = {
      "LkaActvStats_D2_Req": action,
    }
    return self.packer.make_can_msg_panda("Lane_Assist_Data1", 0, values)

  # TJA command
  def _tja_command_msg(self, enabled: bool):
    values = {
      "LatCtl_D_Rq": 1 if enabled else 0,
    }
    return self.packer.make_can_msg_panda("LateralMotionControl", 0, values)

  # Cruise control buttons
  def _acc_button_msg(self, cancel=0, resume=0, _set=0):
    values = {
      "CcAslButtnCnclPress": cancel,
      "CcAsllButtnResPress": resume,
      "CcAslButtnSetPress": _set,
    }
    return self.packer.make_can_msg_panda("Steering_Data_FD1", 0, values)

  def test_steer_allowed(self):
    self.safety.set_controls_allowed(1)
    self.assertTrue(self._tx(self._tja_command_msg(1)))
    self.assertTrue(self.safety.get_controls_allowed())

    self.safety.set_controls_allowed(0)
    self.assertFalse(self._tx(self._tja_command_msg(1)))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_prevent_lkas_action(self):
    self.safety.set_controls_allowed(1)
    self.assertFalse(self._tx(self._lkas_command_msg(1)))

    self.safety.set_controls_allowed(0)
    self.assertFalse(self._tx(self._lkas_command_msg(1)))

  def test_spam_cancel_safety_check(self):
    for allowed in (0, 1):
      self.safety.set_controls_allowed(allowed)
      self.assertTrue(self._tx(self._acc_button_msg(cancel=1)))
      self.assertFalse(self._tx(self._acc_button_msg(resume=1)))
      self.assertFalse(self._tx(self._acc_button_msg(_set=1)))


if __name__ == "__main__":
  unittest.main()
