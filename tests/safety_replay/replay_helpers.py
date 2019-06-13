import struct
import panda.tests.safety.libpandasafety_py as libpandasafety_py


def to_signed(d, bits):
  ret = d
  if d >= (1 << (bits - 1)):
    ret = d - (1 << bits)
  return ret

def is_steering_msg(mode, addr):
  if mode == 2:
    return addr == 0x2E4
  return False

def get_steer_torque(mode, to_send):
  ret = 0
  if mode == 2:
    ret = (to_send.RDLR & 0xFF00) | ((to_send.RDLR >> 16) & 0xFF)
    ret = to_signed(ret, 16)
  return ret

def set_desired_torque_last(safety, mode, torque):
  if mode == 2:
    safety.set_toyota_desired_torque_last(torque)

def package_can_msg(msg):
  addr_shift = 3 if msg.address >= 0x800 else 21
  rdlr, rdhr = struct.unpack('II', msg.dat.ljust(8, b'\x00'))

  ret = libpandasafety_py.ffi.new('CAN_FIFOMailBox_TypeDef *')
  ret[0].RIR = msg.address << addr_shift
  ret[0].RDTR = len(msg.dat) | ((msg.src & 0xF) << 4)
  ret[0].RDHR = rdhr
  ret[0].RDLR = rdlr

  return ret

