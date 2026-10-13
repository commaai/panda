import binascii
import random
import struct
import threading
import time
import unittest
from unittest.mock import patch

from opendbc.car.structs import CarParams
from panda.tests.hitl.base import PandaTestCase
from panda import Panda
from panda.python.spi import PandaProtocolMismatch, PandaSpiNackResponse


class TestSpi(PandaTestCase):
  def _ping(self, panda):
    # should work with no retries
    with patch.object(panda._handle, '_wait_for_ack', wraps=panda._handle._wait_for_ack) as spy:
      panda.health()
      assert spy.call_count == 2

  def test_protocol_version_check(self):
    p = self.p
    for bootstub in (False, True):
      p.reset(enter_bootstub=bootstub)
      with patch('panda.python.spi.PandaSpiHandle.PROTOCOL_VERSION', return_value="abc"):
        # list should still work with wrong version
        assert p._serial in Panda.list()

        # connect but raise protocol error
        with self.assertRaises(PandaProtocolMismatch):
          Panda(p._serial)

  def test_protocol_version_data(self):
    p = self.p
    for bootstub in (False, True):
      p.reset(enter_bootstub=bootstub)
      v = p._handle.get_protocol_version()

      uid = binascii.hexlify(v[:12]).decode()
      assert uid == p.get_uid()

      hwtype = v[12]
      assert hwtype == ord(p.get_type())

      bstub = v[13]
      assert bstub == (0xEE if bootstub else 0xCC)

  def test_all_comm_types(self):
    p = self.p
    spy = self.enterContext(patch.object(p._handle, '_wait_for_ack', wraps=p._handle._wait_for_ack))

    # controlRead + controlWrite
    p.health()
    p.can_clear(0)
    assert spy.call_count == 2*2

    # bulkRead + bulkWrite
    p.can_recv()
    p.can_send(0x123, b"somedata", 0)
    assert spy.call_count == 2*4

  def test_bad_header(self):
    p = self.p
    with patch('panda.python.spi.SYNC', return_value=0):
      with self.assertRaises(PandaSpiNackResponse):
        p._handle.controlRead(Panda.REQUEST_IN, 0xd2, 0, 0, p.HEALTH_STRUCT.size, timeout=50)
    self._ping(p)

  def test_bad_checksum(self):
    p = self.p
    cnt = p.health()['spi_error_count']
    with patch('panda.python.spi.PandaSpiHandle._calc_checksum', return_value=0):
      with self.assertRaises(PandaSpiNackResponse):
        p._handle.controlRead(Panda.REQUEST_IN, 0xd2, 0, 0, p.HEALTH_STRUCT.size, timeout=50)
    self._ping(p)
    assert (p.health()['spi_error_count'] - cnt) > 0

  def test_non_existent_endpoint(self):
    p = self.p
    for _ in range(10):
      ep = random.randint(4, 20)
      with self.assertRaises(PandaSpiNackResponse):
        p._handle.bulkRead(ep, random.randint(1, 1000), timeout=50)

      self._ping(p)

      with self.assertRaises(PandaSpiNackResponse):
        p._handle.bulkWrite(ep, b"abc", timeout=50)

      self._ping(p)

  def test_multiple_clients(self):
    # every client's request runs, even when it's identical to another client's
    self.p.set_safety_mode(CarParams.SafetyModel.allOutput)
    self.p.set_can_loopback(True)
    with Panda(self.p._serial) as p1, Panda(self.p._serial) as p2:
      for _ in range(100):
        p1.can_send(0x123, b"\x01" * 8, 0)
        p2.can_send(0x123, b"\x01" * 8, 0)
    time.sleep(0.1)
    assert len([m for m in self.p.can_recv() if m[2] == 0]) == 200

  def test_can_exactly_once(self):
    # more than a full transfer each way, so writes and reads are split into chunks
    self.p.set_safety_mode(CarParams.SafetyModel.allOutput)
    self.p.set_can_loopback(True)
    sent = [struct.pack(">Q", i) for i in range(600)]
    self.p.can_send_many([(0x123, dat, 0) for dat in sent], timeout=0)
    time.sleep(0.5)
    got = []
    while len(r := self.p.can_recv()):
      got += [m[1] for m in r if m[2] == 0]
    assert sorted(got) == sent

  def test_thread_safe(self):
    # one handle shared by threads, like pandad
    errors = []
    def hammer():
      try:
        for _ in range(200):
          self.p.health()
      except Exception as e:
        errors.append(e)
    threads = [threading.Thread(target=hammer) for _ in range(4)]
    for t in threads:
      t.start()
    for t in threads:
      t.join()
    assert errors == []

  @unittest.expectedFailure  # SPI bulkRead ignores length on the old protocol
  def test_max_sizes(self):
    # around the first read size and the max transfer size
    for n in (1, 1013, 1014, 4032):
      assert len(self.p._handle.bulkRead(0xAB, n)) == n
