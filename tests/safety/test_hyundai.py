#!/usr/bin/env python3
import unittest
import numpy as np
from panda import Panda
from panda.tests.safety import libpandasafety_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda, make_msg
from panda.tests.safety.hyundai_common import HyundaiButtonBase

MAX_ACCEL = 2.0
MIN_ACCEL = -3.5


class Buttons:
  NONE = 0
  RESUME = 1
  SET = 2
  CANCEL = 4


PREV_BUTTON_SAMPLES = 8
ENABLE_BUTTONS = (Buttons.RESUME, Buttons.SET, Buttons.CANCEL)


# 4 bit checkusm used in some hyundai messages
# lives outside the can packer because we never send this msg
def checksum(msg):
  addr, t, dat, bus = msg

  chksum = 0
  if addr == 902:
    for i, b in enumerate(dat):
      for j in range(8):
        # exclude checksum and counter bits
        if (i != 1 or j < 6) and (i != 3 or j < 6) and (i != 5 or j < 6) and (i != 7 or j < 6):
          bit = (b >> j) & 1
        else:
          bit = 0
        chksum += bit
    chksum = (chksum ^ 9) & 0xF
    ret = bytearray(dat)
    ret[5] |= (chksum & 0x3) << 6
    ret[7] |= (chksum & 0xc) << 4
  else:
    for i, b in enumerate(dat):
      if addr in [608, 1057] and i == 7:
        b &= 0x0F if addr == 1057 else 0xF0
      elif addr == 916 and i == 6:
        b &= 0xF0
      elif addr == 916 and i == 7:
        continue
      chksum += sum(divmod(b, 16))
    chksum = (16 - chksum) % 16
    ret = bytearray(dat)
    ret[6 if addr == 916 else 7] |= chksum << (4 if addr == 1057 else 0)

  return addr, t, ret, bus





class TestHyundaiSafety(HyundaiButtonBase, common.PandaSafetyTest, common.DriverTorqueSteeringSafetyTest):
  TX_MSGS = [[832, 0], [1265, 0], [1157, 0]]
  STANDSTILL_THRESHOLD = 30  # ~1kph
  RELAY_MALFUNCTION_ADDR = 832
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {2: [832, 1157]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  MAX_RATE_UP = 3
  MAX_RATE_DOWN = 7
  MAX_TORQUE = 384
  MAX_RT_DELTA = 112
  RT_INTERVAL = 250000
  DRIVER_TORQUE_ALLOWANCE = 50
  DRIVER_TORQUE_FACTOR = 2

  # Safety around steering req bit
  MIN_VALID_STEERING_FRAMES = 89
  MAX_INVALID_STEERING_FRAMES = 2
  MIN_VALID_STEERING_RT_INTERVAL = 810000  # a ~10% buffer, can send steer up to 110Hz

  cnt_gas = 0
  cnt_speed = 0
  cnt_brake = 0
  cnt_cruise = 0
  cnt_button = 0

  def setUp(self):
    self.packer = CANPackerPanda("hyundai_kia_generic")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI, 0)
    self.safety.init_tests()

  def _button_msg(self, buttons, main_button=0, bus=0):
    values = {"CF_Clu_CruiseSwState": buttons, "CF_Clu_CruiseSwMain": main_button, "CF_Clu_AliveCnt1": self.cnt_button}
    self.__class__.cnt_button += 1
    return self.packer.make_can_msg_panda("CLU11", bus, values)

  def _user_gas_msg(self, gas):
    values = {"CF_Ems_AclAct": gas, "AliveCounter": self.cnt_gas % 4}
    self.__class__.cnt_gas += 1
    return self.packer.make_can_msg_panda("EMS16", 0, values, fix_checksum=checksum)

  def _user_brake_msg(self, brake):
    values = {"DriverBraking": brake, "AliveCounterTCS": self.cnt_brake % 8}
    self.__class__.cnt_brake += 1
    return self.packer.make_can_msg_panda("TCS13", 0, values, fix_checksum=checksum)

  def _speed_msg(self, speed):
    # panda safety doesn't scale, so undo the scaling
    values = {"WHL_SPD_%s" % s: speed * 0.03125 for s in ["FL", "FR", "RL", "RR"]}
    values["WHL_SPD_AliveCounter_LSB"] = (self.cnt_speed % 16) & 0x3
    values["WHL_SPD_AliveCounter_MSB"] = (self.cnt_speed % 16) >> 2
    self.__class__.cnt_speed += 1
    return self.packer.make_can_msg_panda("WHL_SPD11", 0, values, fix_checksum=checksum)

  def _pcm_status_msg(self, enable):
    values = {"ACCMode": enable, "CR_VSM_Alive": self.cnt_cruise % 16}
    self.__class__.cnt_cruise += 1
    return self.packer.make_can_msg_panda("SCC12", self.SCC_BUS, values, fix_checksum=checksum)

  # TODO: this is unused
  def _torque_driver_msg(self, torque):
    values = {"CR_Mdps_StrColTq": torque}
    return self.packer.make_can_msg_panda("MDPS12", 0, values)

  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"CR_Lkas_StrToqReq": torque, "CF_Lkas_ActToi": steer_req}
    return self.packer.make_can_msg_panda("LKAS11", 0, values)


class TestHyundaiSafetyCameraSCC(TestHyundaiSafety):
  BUTTONS_BUS = 2  # tx on 2, rx on 0
  SCC_BUS = 2  # rx on 2

  def setUp(self):
    self.packer = CANPackerPanda("hyundai_kia_generic")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI, Panda.FLAG_HYUNDAI_CAMERA_SCC)
    self.safety.init_tests()


class TestHyundaiLegacySafety(TestHyundaiSafety):
  def setUp(self):
    self.packer = CANPackerPanda("hyundai_kia_generic")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI_LEGACY, 0)
    self.safety.init_tests()


class TestHyundaiLegacySafetyEV(TestHyundaiSafety):
  def setUp(self):
    self.packer = CANPackerPanda("hyundai_kia_generic")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI_LEGACY, 1)
    self.safety.init_tests()

  def _user_gas_msg(self, gas):
    values = {"Accel_Pedal_Pos": gas}
    return self.packer.make_can_msg_panda("E_EMS11", 0, values, fix_checksum=checksum)


class TestHyundaiLegacySafetyHEV(TestHyundaiSafety):
  def setUp(self):
    self.packer = CANPackerPanda("hyundai_kia_generic")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI_LEGACY, 2)
    self.safety.init_tests()

  def _user_gas_msg(self, gas):
    values = {"CR_Vcu_AccPedDep_Pos": gas}
    return self.packer.make_can_msg_panda("E_EMS11", 0, values, fix_checksum=checksum)

class TestHyundaiLongitudinalSafety(TestHyundaiSafety):
  TX_MSGS = [[832, 0], [1265, 0], [1157, 0], [1056, 0], [1057, 0], [1290, 0], [905, 0], [1186, 0], [909, 0], [1155, 0], [2000, 0]]

  def setUp(self):
    self.packer = CANPackerPanda("hyundai_kia_generic")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI, Panda.FLAG_HYUNDAI_LONG)
    self.safety.init_tests()

  # override these tests from PandaSafetyTest, hyundai longitudinal uses button enable
  def test_disable_control_allowed_from_cruise(self):
    pass

  def test_enable_control_allowed_from_cruise(self):
    pass

  def test_sampling_cruise_buttons(self):
    pass

  def test_cruise_engaged_prev(self):
    pass

  def test_button_sends(self):
    pass

  def _pcm_status_msg(self, enable):
    raise NotImplementedError

  def _send_accel_msg(self, accel, aeb_req=False, aeb_decel=0):
    values = {
      "aReqRaw": accel,
      "aReqValue": accel,
      "AEB_CmdAct": int(aeb_req),
      "CR_VSM_DecCmd": aeb_decel,
    }
    return self.packer.make_can_msg_panda("SCC12", self.SCC_BUS, values)

  def _send_fca11_msg(self, idx=0, vsm_aeb_req=False, fca_aeb_req=False, aeb_decel=0):
    values = {
      "CR_FCA_Alive": ((-((idx % 0xF) + 2) % 4) << 2) + 1,
      "Supplemental_Counter": idx % 0xF,
      "FCA_Status": 2,
      "CR_VSM_DecCmd": aeb_decel,
      "CF_VSM_DecCmdAct": int(vsm_aeb_req),
      "FCA_CmdAct": int(fca_aeb_req),
    }
    return self.packer.make_can_msg_panda("FCA11", 0, values)

  def test_no_aeb_fca11(self):
    self.assertTrue(self._tx(self._send_fca11_msg()))
    self.assertFalse(self._tx(self._send_fca11_msg(vsm_aeb_req=True)))
    self.assertFalse(self._tx(self._send_fca11_msg(fca_aeb_req=True)))
    self.assertFalse(self._tx(self._send_fca11_msg(aeb_decel=1.0)))

  def test_no_aeb_scc12(self):
    self.assertTrue(self._tx(self._send_accel_msg(0)))
    self.assertFalse(self._tx(self._send_accel_msg(0, aeb_req=True)))
    self.assertFalse(self._tx(self._send_accel_msg(0, aeb_decel=1.0)))

  def test_set_resume_buttons(self):
    """
      SET and RESUME enter controls allowed on their falling edge.
    """
    for btn in range(8):
      self.safety.set_controls_allowed(0)
      for _ in range(10):
        self._rx(self._button_msg(btn))
        self.assertFalse(self.safety.get_controls_allowed())

      # should enter controls allowed on falling edge
      if btn in (Buttons.RESUME, Buttons.SET):
        self._rx(self._button_msg(Buttons.NONE))
        self.assertTrue(self.safety.get_controls_allowed())

  def test_cancel_button(self):
    self.safety.set_controls_allowed(1)
    self._rx(self._button_msg(Buttons.CANCEL))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_accel_safety_check(self):
    for controls_allowed in [True, False]:
      for accel in np.arange(MIN_ACCEL - 1, MAX_ACCEL + 1, 0.01):
        accel = round(accel, 2) # floats might not hit exact boundary conditions without rounding
        self.safety.set_controls_allowed(controls_allowed)
        send = MIN_ACCEL <= accel <= MAX_ACCEL if controls_allowed else accel == 0
        self.assertEqual(send, self._tx(self._send_accel_msg(accel)), (controls_allowed, accel))

  def test_diagnostics(self):
    tester_present = common.package_can_msg((0x7d0, 0, b"\x02\x3E\x80\x00\x00\x00\x00\x00", 0))
    self.assertTrue(self.safety.safety_tx_hook(tester_present))

    not_tester_present = common.package_can_msg((0x7d0, 0, b"\x03\xAA\xAA\x00\x00\x00\x00\x00", 0))
    self.assertFalse(self.safety.safety_tx_hook(not_tester_present))

  def test_radar_alive(self):
    # If the radar knockout failed, make sure the relay malfunction is shown
    self.assertFalse(self.safety.get_relay_malfunction())
    self._rx(make_msg(0, 1057, 8))
    self.assertTrue(self.safety.get_relay_malfunction())


if __name__ == "__main__":
  unittest.main()
