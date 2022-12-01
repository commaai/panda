import os

from typing import List
from cffi import FFI

can_dir = os.path.dirname(os.path.abspath(__file__))
libpandaprotocol_fn = os.path.join(can_dir, "libpandaprotocol.so")

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
typedef struct {
  volatile uint32_t w_ptr;
  volatile uint32_t r_ptr;
  uint32_t fifo_size;
  CANPacket_t *elems;
} can_ring;

extern can_ring *rx_q;
extern can_ring *txgmlan_q;
extern can_ring *tx1_q;
extern can_ring *tx2_q;
extern can_ring *tx3_q;

bool can_pop(can_ring *q, CANPacket_t *elem);
bool can_push(can_ring *q, CANPacket_t *elem);
int comms_can_read(uint8_t *data, uint32_t max_len);
void comms_can_write(uint8_t *data, uint32_t len);
uint32_t can_slots_empty(can_ring *q);
""")

class CANPacket:
  reserved: int
  bus: int
  data_len_code: int
  rejected: int
  returned: int
  extended: int
  addr: int
  data: List[int]

libpandaprotocol = ffi.dlopen(libpandaprotocol_fn)