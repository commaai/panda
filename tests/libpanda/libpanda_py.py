import os
from cffi import FFI
from typing import List

from panda import LEN_TO_DLC
from panda.tests.libpanda.safety_helpers import PandaSafety, setup_safety_helpers

libpanda_dir = os.path.dirname(os.path.abspath(__file__))
libpanda_fn = os.path.join(libpanda_dir, "libpanda.so")

ffi = FFI()

ffi.cdef("""
typedef struct {
  unsigned char reserved : 1;
  unsigned char bus : 3;
  unsigned char data_len_code : 4;
  unsigned char rejected : 1;
  unsigned char returned : 1;
  unsigned char extended : 1;
  unsigned int addr : 29;
  unsigned char data[64];
} CANPacket_t;
""", packed=True)

ffi.cdef("""
int safety_rx_hook(CANPacket_t *to_send);
int safety_tx_hook(CANPacket_t *to_push);
int safety_fwd_hook(int bus_num, CANPacket_t *to_fwd);
int set_safety_hooks(uint16_t mode, uint16_t param);
""")

setup_safety_helpers(ffi)

class CANPacket:
  reserved: int
  bus: int
  data_len_code: int
  rejected: int
  returned: int
  extended: int
  addr: int
  data: List[int]

class Panda(PandaSafety):
  # safety
  def safety_rx_hook(self, to_send: CANPacket) -> int: ...
  def safety_tx_hook(self, to_push: CANPacket) -> int: ...
  def safety_fwd_hook(self, bus_num: int, to_fwd: CANPacket) -> int: ...
  def set_safety_hooks(self, mode: int, param: int) -> int: ...


libpanda: Panda = ffi.dlopen(libpanda_fn)


# helpers

def make_CANPacket(addr: int, bus: int, dat):
  ret = ffi.new('CANPacket_t *')
  ret[0].extended = 1 if addr >= 0x800 else 0
  ret[0].addr = addr
  ret[0].data_len_code = LEN_TO_DLC[len(dat)]
  ret[0].bus = bus
  ret[0].data = bytes(dat)

  return ret
