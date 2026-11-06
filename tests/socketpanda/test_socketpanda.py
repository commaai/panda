#!/usr/bin/env python3
import socket
import struct
import unittest
from unittest import mock

from panda.python import socketpanda
from panda.python.socketpanda import SocketPanda, CAN_HEADER_FMT, SO_RXQ_OVFL


def can_frame(addr: int, dat: bytes) -> bytes:
  return struct.pack(CAN_HEADER_FMT, addr, len(dat), 0) + dat.ljust(8, b'\x00')


class FakeCanSocket:
  # stands in for a SocketCAN socket: queued frames carry the kernel's cumulative drop count,
  # which is only reported when the caller passes room for ancillary data (as Linux does)
  def __init__(self, frames: list[tuple[bytes, int]]):
    self.frames = list(frames)

  def recvmsg(self, bufsize: int, ancbufsize: int = 0):
    if not self.frames:
      raise BlockingIOError
    dat, drops = self.frames.pop(0)
    ancdata = []
    if drops and ancbufsize >= socket.CMSG_SPACE(4):
      ancdata.append((socket.SOL_SOCKET, SO_RXQ_OVFL, struct.pack("=I", drops)))
    return dat, ancdata, 0, None

  def close(self):
    pass


class TestSocketPandaOverflow(unittest.TestCase):
  def make_panda(self, *sockets: FakeCanSocket) -> SocketPanda:
    patcher = mock.patch.object(socketpanda, "create_socketcan", side_effect=list(sockets))
    patcher.start()
    self.addCleanup(patcher.stop)
    return SocketPanda("vcan0")

  def test_no_drops(self):
    p = self.make_panda(FakeCanSocket([(can_frame(0x123, b'\x01\x02'), 0)]))
    self.assertEqual(p.can_recv(), [(0x123, b'\x01\x02', 0)])
    self.assertEqual(p.rx_buffer_overflow, 0)

  def test_drops_counted_and_logged(self):
    frames = [
      (can_frame(0x100, b'\x00'), 0),
      (can_frame(0x101, b'\x01'), 3),  # 3 frames lost before this one
      (can_frame(0x102, b'\x02'), 3),  # no new drops
      (can_frame(0x103, b'\x03'), 5),  # 2 more
    ]
    p = self.make_panda(FakeCanSocket(frames))
    with self.assertLogs("panda", level="WARNING") as logs:
      msgs = p.can_recv()
    self.assertEqual([m[0] for m in msgs], [0x100, 0x101, 0x102, 0x103])
    self.assertEqual(p.rx_buffer_overflow, 5)
    self.assertEqual(len(logs.records), 2)

  def test_drop_count_resets_with_socket(self):
    p = self.make_panda(FakeCanSocket([(can_frame(0x100, b''), 4)]),
                        FakeCanSocket([(can_frame(0x100, b''), 2)]))
    with self.assertLogs("panda", level="WARNING"):
      p.can_recv()
      p.can_clear(0)  # new socket, kernel count starts over
      p.can_recv()
    self.assertEqual(p.rx_buffer_overflow, 6)

  def test_drop_counter_wraps(self):
    p = self.make_panda(FakeCanSocket([(can_frame(0x100, b''), 0xFFFFFFFE), (can_frame(0x100, b''), 1)]))
    with self.assertLogs("panda", level="WARNING"):
      p.can_recv()
    self.assertEqual(p.rx_buffer_overflow, 0xFFFFFFFE + 3)


if __name__ == "__main__":
  unittest.main()
