import os
import sys
from cffi import FFI
from typing import Any, Protocol

# Add python directory to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'python'))

try:
    from __init__ import LEN_TO_DLC
except ImportError:
    # Fallback for local testing
    LEN_TO_DLC = {0: 0, 1: 1, 2: 2, 3: 3, 4: 4, 5: 5, 6: 6, 7: 7, 8: 8, 12: 9, 16: 10, 20: 11, 24: 12, 32: 13, 48: 14, 64: 15}

libpanda_dir = os.path.dirname(os.path.abspath(__file__))
libpanda_fn = os.path.join(libpanda_dir, "libpanda.so")

ffi = FFI()

ffi.cdef("""
typedef struct {
  uint8_t flags;
  uint32_t addr;
  uint8_t checksum;
  uint8_t data[64];
} CANPacket_t;
""", packed=True)

ffi.cdef("""
int set_safety_hooks(uint16_t mode, uint16_t param);
""")

ffi.cdef("""
typedef struct {
  volatile uint32_t w_ptr;
  volatile uint32_t r_ptr;
  uint32_t fifo_size;
  CANPacket_t *elems;
} can_ring;

extern can_ring *rx_q;
extern can_ring *tx1_q;
extern can_ring *tx2_q;
extern can_ring *tx3_q;

bool can_pop(can_ring *q, CANPacket_t *elem);
bool can_push(can_ring *q, CANPacket_t *elem);
void can_set_checksum(CANPacket_t *packet);
int comms_can_read(uint8_t *data, uint32_t max_len);
void comms_can_write(uint8_t *data, uint32_t len);
void comms_can_reset(void);
uint32_t can_slots_empty(can_ring *q);

// Accessor functions for portable CAN packet structure
uint8_t can_get_fd(CANPacket_t *pkt);
uint8_t can_get_bus(CANPacket_t *pkt);
uint8_t can_get_data_len_code(CANPacket_t *pkt);
uint32_t can_get_addr(CANPacket_t *pkt);
uint8_t can_get_extended(CANPacket_t *pkt);
uint8_t can_get_returned(CANPacket_t *pkt);
uint8_t can_get_rejected(CANPacket_t *pkt);

void can_set_fd(CANPacket_t *pkt, uint8_t val);
void can_set_bus(CANPacket_t *pkt, uint8_t val);
void can_set_data_len_code(CANPacket_t *pkt, uint8_t val);
void can_set_addr(CANPacket_t *pkt, uint32_t val);
void can_set_extended(CANPacket_t *pkt, uint8_t val);
void can_set_returned(CANPacket_t *pkt, uint8_t val);
void can_set_rejected(CANPacket_t *pkt, uint8_t val);
""")

class CANPacket:
  reserved: int
  bus: int
  data_len_code: int
  rejected: int
  returned: int
  extended: int
  addr: int
  data: list[int]

class Panda(Protocol):
  # CAN
  tx1_q: Any
  tx2_q: Any
  tx3_q: Any
  def can_set_checksum(self, p: CANPacket) -> None: ...

  # safety
  def set_safety_hooks(self, mode: int, param: int) -> int: ...


_libpanda_raw = ffi.dlopen(libpanda_fn)

# Wrapper to handle CANPacketAccessor automatically
class PandaWrapper:
    def __getattr__(self, name):
        attr = getattr(_libpanda_raw, name)
        # Wrap functions that take CANPacket_t* to handle our wrapper
        if name in ['can_push', 'can_pop']:
            def wrapper(*args, **kwargs):
                # Convert CANPacketAccessor to raw CFFI packet
                new_args = []
                for arg in args:
                    if hasattr(arg, '_cffi'):
                        new_args.append(arg._cffi)
                    else:
                        new_args.append(arg)
                return attr(*new_args, **kwargs)
            return wrapper
        return attr
        
libpanda: Panda = PandaWrapper()


# helpers

class CANPacketWrapper:
    """Wrapper to provide bit field-like access to the new packet structure"""
    def __init__(self, cffi_packet):
        self._pkt = cffi_packet
    
    @property
    def addr(self):
        return _libpanda_raw.can_get_addr(self._pkt)
    
    @addr.setter  
    def addr(self, val):
        _libpanda_raw.can_set_addr(self._pkt, val)
    
    @property
    def bus(self):
        return _libpanda_raw.can_get_bus(self._pkt)
    
    @bus.setter
    def bus(self, val):
        _libpanda_raw.can_set_bus(self._pkt, val)
        
    @property
    def data_len_code(self):
        return _libpanda_raw.can_get_data_len_code(self._pkt)
    
    @data_len_code.setter
    def data_len_code(self, val):
        _libpanda_raw.can_set_data_len_code(self._pkt, val)
        
    @property
    def extended(self):
        return _libpanda_raw.can_get_extended(self._pkt)
    
    @extended.setter
    def extended(self, val):
        _libpanda_raw.can_set_extended(self._pkt, val)
        
    @property
    def fd(self):
        return _libpanda_raw.can_get_fd(self._pkt)
    
    @fd.setter
    def fd(self, val):
        _libpanda_raw.can_set_fd(self._pkt, val)
        
    @property
    def returned(self):
        return _libpanda_raw.can_get_returned(self._pkt)
    
    @returned.setter
    def returned(self, val):
        _libpanda_raw.can_set_returned(self._pkt, val)
        
    @property
    def rejected(self):
        return _libpanda_raw.can_get_rejected(self._pkt)
    
    @rejected.setter
    def rejected(self, val):
        _libpanda_raw.can_set_rejected(self._pkt, val)
        
    @property
    def checksum(self):
        return self._pkt[0].checksum
        
    @checksum.setter
    def checksum(self, val):
        self._pkt[0].checksum = val
        
    @property
    def data(self):
        return self._pkt[0].data

class CANPacketAccessor:
    """Array-like accessor to provide pkt[0] syntax"""
    def __init__(self, cffi_packet):
        self._cffi_packet = cffi_packet
        self._wrapper = CANPacketWrapper(cffi_packet)
        
    def __getitem__(self, index):
        if index == 0:
            return self._wrapper
        else:
            raise IndexError("Only index 0 is supported")
            
    @property
    def _cffi(self):
        """Get the underlying CFFI packet for queue operations"""
        return self._cffi_packet

def make_CANPacket(addr: int, bus: int, dat):
  ret = ffi.new('CANPacket_t *')
  
  # Use accessor functions for proper bit packing
  _libpanda_raw.can_set_addr(ret, addr)
  _libpanda_raw.can_set_bus(ret, bus)  
  _libpanda_raw.can_set_data_len_code(ret, LEN_TO_DLC[len(dat)])
  _libpanda_raw.can_set_extended(ret, 1 if addr >= 0x800 else 0)
  _libpanda_raw.can_set_fd(ret, 0)
  _libpanda_raw.can_set_returned(ret, 0)
  _libpanda_raw.can_set_rejected(ret, 0)
  
  ret[0].data = bytes(dat)
  _libpanda_raw.can_set_checksum(ret)

  return CANPacketAccessor(ret)
