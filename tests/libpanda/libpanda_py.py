import ctypes
import os

from panda import LEN_TO_DLC

libpanda_dir = os.path.dirname(os.path.abspath(__file__))
libpanda_fn = os.path.join(libpanda_dir, "libpanda.so")


class CANPacket(ctypes.Structure):
  _fields_ = [
    ("addr", ctypes.c_uint, 29),
    ("extended", ctypes.c_uint, 1),
    ("returned", ctypes.c_uint, 1),
    ("rejected", ctypes.c_uint, 1),
    ("bus", ctypes.c_uint8),
    ("data_len_code", ctypes.c_uint8),
    ("fd", ctypes.c_uint8),
    ("data", ctypes.c_uint8 * 64),
  ]


class CANRing(ctypes.Structure):
  _fields_ = [
    ("w_ptr", ctypes.c_uint32),
    ("r_ptr", ctypes.c_uint32),
    ("fifo_size", ctypes.c_uint32),
    ("elems", ctypes.POINTER(CANPacket)),
  ]


libpanda = ctypes.CDLL(libpanda_fn)

for name in ("rx_q", "tx1_q", "tx2_q", "tx3_q"):
  setattr(libpanda, name, ctypes.POINTER(CANRing).in_dll(libpanda, name))

for name, argtypes, restype in (
  ("set_safety_hooks", [ctypes.c_uint16, ctypes.c_uint16], ctypes.c_int),
  ("can_pop", [ctypes.POINTER(CANRing), ctypes.POINTER(CANPacket)], ctypes.c_bool),
  ("can_push", [ctypes.POINTER(CANRing), ctypes.POINTER(CANPacket)], ctypes.c_bool),
  ("comms_can_read", [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint32], ctypes.c_uint32),
  ("comms_can_write", [ctypes.c_char_p, ctypes.c_uint32], None),
  ("comms_can_reset", [], None),
  ("can_slots_empty", [ctypes.POINTER(CANRing)], ctypes.c_uint32),
  ("get_safety_tx_blocked", [], ctypes.c_uint32),
  ("get_tx_checksum_error_cnt", [ctypes.c_uint8], ctypes.c_uint32),
):
  func = getattr(libpanda, name)
  func.argtypes = argtypes
  func.restype = restype


def make_CANPacket(addr: int, bus: int, dat) -> CANPacket:
  ret = CANPacket()
  ret.extended = 1 if addr >= 0x800 else 0
  ret.addr = addr
  ret.data_len_code = LEN_TO_DLC[len(dat)]
  ret.bus = bus
  ret.data[:len(dat)] = bytes(dat)
  return ret
