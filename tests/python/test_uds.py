#!/usr/bin/env python3
from parameterized import parameterized_class
import unittest
import struct
import threading

from panda.python.uds import SERVICE_TYPE, DATA_IDENTIFIER_TYPE, DEFAULT_VIN, IsoTpMessage, UdsClient, get_rx_addr_for_tx_addr


class UdsServer(UdsClient):
  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self.kill_event = threading.Event()
    self.uds_thread = threading.Thread(target=self._uds_response)

  def start(self):
    if self.uds_thread.is_alive():
      self.stop()
    self.uds_thread.start()

  def stop(self):
    self.kill_event.set()
    self.uds_thread.join()

  def _uds_response(self):
    # send request, wait for response
    max_len = 8 if self.sub_addr is None else 7
    isotp_msg = IsoTpMessage(self._server_can_client, timeout=0, debug=self.debug, max_len=max_len, single_frame_mode=True)
    isotp_msg.send(b"", setup_only=True)
    while not self.kill_event.is_set():
      resp, _ = isotp_msg.recv(0)
      print('UDS Server - got resp', resp)

      # message from client not fully built
      if resp is None:
        continue

      # got a message, now respond
      resp_sid = resp[0] if len(resp) > 0 else None
      print('got response!', resp, 'sid:', resp_sid)

      if resp_sid == SERVICE_TYPE.TESTER_PRESENT:
        dat = bytes([SERVICE_TYPE.TESTER_PRESENT + 0x40, 0x0])
        print('Car ECU - sending back:', dat)
        isotp_msg.send(dat)
      elif resp_sid == SERVICE_TYPE.READ_DATA_BY_IDENTIFIER:
        print('Car ECU - READ DATA')
        assert len(resp) > 1
        resp_data = struct.unpack('!H', resp[1:])[0]
        print(resp_data)
        if resp_data == DATA_IDENTIFIER_TYPE.VIN:
          dat = bytes([resp_sid + 0x40]) + resp[1:] + DEFAULT_VIN.encode('utf-8')
          print('Car ECU - sending back:', dat)
          isotp_msg.send(dat)
        else:
          dat = bytes([0x7F, resp_sid, *resp[1:]])
          print('Car ECU - bad data id, sending back:', dat)
          isotp_msg.send(dat)

      else:
        dat = [0x7F]
        if resp_sid is not None:
          dat.append(resp_sid)
        dat = bytes(dat)
        print('Car ECU - bad request, sending back:', dat)
        isotp_msg.send(dat)


class MockCanBuffer:
  def __init__(self):
    self.lock = threading.Lock()
    self.rx_msg = []

  def can_send(self, addr, dat, bus, timeout=0):
    with self.lock:
      self.rx_msg = [(addr, 0, dat, bus)]

  def can_recv(self):
    with self.lock:
      return self.rx_msg


@parameterized_class([
  {"tx_addr": 0x750, "sub_addr": None},
  {"tx_addr": 0x750, "sub_addr": 0xf},
])
class TestUds(unittest.TestCase):
  tx_addr: int
  sub_addr: int

  def setUp(self):
    self.rx_addr = get_rx_addr_for_tx_addr(self.tx_addr)
    can_buf = MockCanBuffer()
    self.uds_server = UdsServer(can_buf, get_rx_addr_for_tx_addr(self.tx_addr), self.tx_addr, sub_addr=self.sub_addr)
    self.uds_server.start()

  def tearDown(self):
    self.uds_server.stop()

  def test_tester_present(self):
    """
    Tests IsoTpMessage and CanClient both sending and responding to a
    tester present request with and without sub-addresses
    """

    self.uds_server.tester_present()

  def test_vin_query(self):
    """
    Tests all four ISO-TP frame types in both directions (sending as openpilot and the car ECU)
    """

    response = self.uds_server.read_data_by_identifier(DATA_IDENTIFIER_TYPE.VIN)
    self.assertEqual(response.decode('utf-8'), DEFAULT_VIN)


if __name__ == '__main__':
  unittest.main()
