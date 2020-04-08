import struct

from opendbc.can.packer import CANPacker
from panda.tests.safety import libpandasafety_py

MAX_WRONG_COUNTERS = 5

class UNSAFE_MODE:
  DEFAULT = 0
  DISABLE_DISENGAGE_ON_GAS = 1
  DISABLE_STOCK_AEB = 2
  RAISE_LONGITUDINAL_LIMITS_TO_ISO_MAX = 8

def make_msg(bus, addr, length=8):
  to_send = libpandasafety_py.ffi.new('CAN_FIFOMailBox_TypeDef *')
  if addr >= 0x800:
    to_send[0].RIR = (addr << 3) | 5
  else:
    to_send[0].RIR = (addr << 21) | 1
  to_send[0].RDTR = length
  to_send[0].RDTR |= bus << 4

  return to_send

def interceptor_msg(gas, addr):
  to_send = make_msg(0, addr, 6)
  gas2 = gas * 2
  to_send[0].RDLR = ((gas & 0xff) << 8) | ((gas & 0xff00) >> 8) | \
                    ((gas2 & 0xff) << 24) | ((gas2 & 0xff00) << 8)
  return to_send

def package_can_msg(msg):
  addr, _, dat, bus = msg
  rdlr, rdhr = struct.unpack('II', dat.ljust(8, b'\x00'))

  ret = libpandasafety_py.ffi.new('CAN_FIFOMailBox_TypeDef *')
  if addr >= 0x800:
    ret[0].RIR = (addr << 3) | 5
  else:
    ret[0].RIR = (addr << 21) | 1
  ret[0].RDTR = len(dat) | ((bus & 0xF) << 4)
  ret[0].RDHR = rdhr
  ret[0].RDLR = rdlr

  return ret

class CANPackerPanda(CANPacker):
  def make_can_msg_panda(self, name_or_addr, bus, values, counter=-1):
    msg = self.make_can_msg(name_or_addr, bus, values, counter=-1)
    return package_can_msg(msg)

class StdTest:
  @staticmethod
  def test_relay_malfunction(test, addr, bus=0):
    # input is a test class and the address that, if seen on specified bus, triggers
    # the relay_malfunction protection logic: both tx_hook and fwd_hook are
    # expected to return failure
    test.assertFalse(test.safety.get_relay_malfunction())
    test.safety.safety_rx_hook(make_msg(bus, addr, 8))
    test.assertTrue(test.safety.get_relay_malfunction())
    for a in range(1, 0x800):
      for b in range(0, 3):
        test.assertFalse(test.safety.safety_tx_hook(make_msg(b, a, 8)))
        test.assertEqual(-1, test.safety.safety_fwd_hook(b, make_msg(b, a, 8)))

  @staticmethod
  def test_manually_enable_controls_allowed(test):
    test.safety.set_controls_allowed(1)
    test.assertTrue(test.safety.get_controls_allowed())
    test.safety.set_controls_allowed(0)
    test.assertFalse(test.safety.get_controls_allowed())

  @staticmethod
  def test_spam_can_buses(test, TX_MSGS):
    for addr in range(1, 0x800):
      for bus in range(0, 4):
        if all(addr != m[0] or bus != m[1] for m in TX_MSGS):
          test.assertFalse(test.safety.safety_tx_hook(make_msg(bus, addr, 8)))

  @staticmethod
  def test_allow_brake_at_zero_speed(test):
    # Brake was already pressed
    test.safety.safety_rx_hook(test._speed_msg(0))
    test.safety.safety_rx_hook(test._brake_msg(1))
    test.safety.set_controls_allowed(1)
    test.safety.safety_rx_hook(test._brake_msg(1))
    test.assertTrue(test.safety.get_controls_allowed())
    test.safety.safety_rx_hook(test._brake_msg(0))
    test.assertTrue(test.safety.get_controls_allowed())
    # rising edge of brake should disengage
    test.safety.safety_rx_hook(test._brake_msg(1))
    test.assertFalse(test.safety.get_controls_allowed())
    test.safety.safety_rx_hook(test._brake_msg(0))  # reset no brakes

  @staticmethod
  def test_not_allow_brake_when_moving(test, standstill_threshold):
    # Brake was already pressed
    test.safety.safety_rx_hook(test._brake_msg(1))
    test.safety.set_controls_allowed(1)
    test.safety.safety_rx_hook(test._speed_msg(standstill_threshold))
    test.safety.safety_rx_hook(test._brake_msg(1))
    test.assertTrue(test.safety.get_controls_allowed())
    test.safety.safety_rx_hook(test._speed_msg(standstill_threshold + 1))
    test.safety.safety_rx_hook(test._brake_msg(1))
    test.assertFalse(test.safety.get_controls_allowed())
    test.safety.safety_rx_hook(test._speed_msg(0))
