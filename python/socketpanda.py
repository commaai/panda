import socket
import struct

# struct can_frame {
# 	canid_t can_id;  /* 32 bit CAN_ID + EFF/RTR/ERR flags */
# 	union {
# 		/* CAN frame payload length in byte (0 .. CAN_MAX_DLEN)
# 		 * was previously named can_dlc so we need to carry that
# 		 * name for legacy support
# 		 */
# 		__u8 len;
# 		__u8 can_dlc; /* deprecated */
# 	} __attribute__((packed)); /* disable padding added in some ABIs */
# 	__u8 __pad; /* padding */
# 	__u8 __res0; /* reserved / padding */
# 	__u8 len8_dlc; /* optional DLC for 8 byte payload length (9 .. 15) */
# 	__u8 data[CAN_MAX_DLEN] __attribute__((aligned(8)));
# };

CAN_FRAME_FMT = "=IB3x8s"

# socket.SO_RXQ_OVFL is missing
# https://github.com/torvalds/linux/blob/47ac09b91befbb6a235ab620c32af719f8208399/include/uapi/asm-generic/socket.h#L61
SO_RXQ_OVFL = 40

# Panda class substitue for socketcan device (to support using the uds/iso-tp/xcp/ccp library)
class SocketPanda():
  def __init__(self, interface:str="vcan0", bus=0, recv_buffer_size=212992) -> None:
    self.interface = interface
    self.bus = bus
    self.recv_buffer_size = recv_buffer_size
    # settings mostly from https://github.com/linux-can/can-utils/blob/master/candump.c
    self.socket = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, recv_buffer_size)
    assert self.socket.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF) == recv_buffer_size * 2
    # TODO: how to dectect and alert on buffer overflow?
    self.socket.setsockopt(socket.SOL_SOCKET, SO_RXQ_OVFL, 1)
    self.socket.bind((interface,))

  def can_send(self, addr, dat, bus=0, timeout=0):
    can_dlc = len(dat)
    can_dat = dat.ljust(8, b'\x00')
    can_frame = struct.pack(CAN_FRAME_FMT, addr, can_dlc, can_dat)
    self.socket.sendto(can_frame, (self.interface,))

  def can_recv(self):
    msgs = list()
    while True:
      try:
        dat, _ = self.socket.recvfrom(self.recv_buffer_size, socket.MSG_DONTWAIT)
        assert len(dat) == 16, f"ERROR: received {len(dat)} bytes"
        can_id, can_dlc, can_dat = struct.unpack(CAN_FRAME_FMT, dat)
        msgs.append((can_id, can_dat[:can_dlc], self.bus))
      except BlockingIOError:
        break # buffered data exhausted
    return msgs
