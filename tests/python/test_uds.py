#!/usr/bin/env python3
import time
from typing import Dict, Optional
from parameterized import parameterized_class
import unittest
import struct
import threading
from dataclasses import dataclass
from functools import partial

from panda.python import uds
from panda.python.uds import SERVICE_TYPE, DATA_IDENTIFIER_TYPE, IsoTpMessage, UdsClient, get_rx_addr_for_tx_addr

UdsServicesType = Dict[SERVICE_TYPE, Dict[Optional[bytes], Dict[bytes, bytes]]]

# TODO: use dataclass?
STANDARD_UDS_SERVER_SERVICES: UdsServicesType = {
  SERVICE_TYPE.TESTER_PRESENT: {
    b'\x00': {  # sub function to data in/out  # TODO: replace subfunction map with int
      b'': b'',  # don't expect any extra data, don't respond with extra data
    },
  },
  SERVICE_TYPE.DIAGNOSTIC_SESSION_CONTROL: {
    bytes([i]): {
      b'': b'',
    } for i in uds.SESSION_TYPE
  }
}


# TODO: it might make sense to not subclass, what do we even use from UdsClient now?
class UdsServer(UdsClient):
  services: UdsServicesType

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs, single_frame_mode=False)
    self.kill_event = threading.Event()
    self.uds_thread = threading.Thread(target=self._uds_response)

    # wrap can buffer functions with server=True so we know where to put messages
    panda = args[0]
    can_send_with_timeout_server = partial(panda.can_send, server=True)
    can_recv_server = partial(panda.can_recv, server=True)
    self._server_can_client = uds.CanClient(can_send_with_timeout_server, can_recv_server, self.tx_addr, self.rx_addr, self.bus, self.sub_addr, debug=self.debug)

  def set_services(self, services):
    self.services = services

    for service_type, service_info in self.services.items():
      assert service_type is not None, "Service type can't be undefined"
      # TODO: if we use a dataclass we don't need to test, it would be impossible to write
      assert len(set(map(type, service_info.keys()))) == 1, \
        f"Service {hex(service_type)} must not define some and none subfunctions"

  @staticmethod
  def _create_negative_response(service_type: SERVICE_TYPE, response_code: Optional[int] = None) -> bytes:
    dat = [0x7F]
    if service_type is not None:  # TODO: shouldn't be none
      dat.append(service_type)
    if response_code is not None:
      dat.append(response_code)
    return bytes(dat)

  def _uds_response(self):
    # send request, wait for response
    max_len = 8 if self.sub_addr is None else 7
    isotp_msg = IsoTpMessage(self._server_can_client, timeout=0, debug=self.debug, max_len=max_len, single_frame_mode=False)
    isotp_msg.send(b"", setup_only=True)
    while not self.kill_event.is_set():
      resp, _ = isotp_msg.recv(0)  # resp is request from client
      print('UDS Server - got resp', resp)
      time.sleep(0.01)

      # message from client not fully built
      if resp is None:
        continue

      # got a message, now respond
      resp_sid = resp[0] if len(resp) > 0 else None

      if resp_sid in self.services:
        # get subfunction from request if expected to have one
        has_subfunction = None not in self.services[resp_sid]  # len(self.services[resp_sid]) == 1
        resp_sfn = bytes([resp[1]]) if len(resp) > 1 and has_subfunction else None

        # print(f'{service_has_subfunction=}')
        if not has_subfunction:
          data = resp[(1 if resp_sfn is None else 2):]
          # print('data in service', data, self.services[resp_sid][None])
          if data in self.services[resp_sid][None]:
            print('here!')
            send_dat = bytes([resp_sid + 0x40]) + self.services[resp_sid][None][data]
            isotp_msg.send(send_dat)
          else:
            # invalid data
            dat = self._create_negative_response(resp_sid, 0x31)  # request out of range
            print('Car ECU - bad request, sending back:', dat)
            isotp_msg.send(dat)

        else:  # has subfunction
          # resp_sfn = resp[1] if len(resp) > 1 else None
          print('resp_sfn', resp_sfn)
          if resp_sfn not in self.services[resp_sid]:
            # return invalid subfunction
            dat = self._create_negative_response(resp_sid, 0x7E)
            print('Car ECU - bad subfunction, sending back:', dat.hex())
            isotp_msg.send(dat)

          else:
            # valid subfunction, now check data
            data = resp[(1 if resp_sfn is None else 2):]
            if data in self.services[resp_sid][resp_sfn]:
              # valid data
              send_dat = bytes([resp_sid + 0x40, int.from_bytes(resp_sfn)])
              if len(self.services[resp_sid][resp_sfn][data]):
                # send_dat.append(self.services[resp_sid][resp_sfn][data])  # add data if exists
                send_dat += (self.services[resp_sid][resp_sfn][data])  # add data if exists
              print('send_dat', send_dat, resp_sid)
              # send_dat = bytes(send_dat)
              isotp_msg.send(send_dat)

            else:
              # invalid data
              dat = self._create_negative_response(resp_sid, 0x31)  # request out of range
              print('Car ECU - bad request, sending back:', dat)
              isotp_msg.send(dat)

      else:
        # invalid service
        dat = self._create_negative_response(resp_sid, 0x11)  # service not supported
        print('Car ECU - bad request, sending back:', dat)
        isotp_msg.send(dat)

  def start(self):
    if self.uds_thread.is_alive():
      self.stop()
    self.uds_thread.start()

  def stop(self):
    self.kill_event.set()
    self.uds_thread.join()

def pop_all(l):
  r, l[:] = l[:], []
  return r


class MockCanBuffer:
  def __init__(self):
    self.lock = threading.Lock()
    # two buffers prevents server from reading its own messages and vice versa
    self.rx_msgs_server = []  # TODO: name these better
    self.rx_msgs_client = []

  def can_send(self, addr, dat, bus, server=False, timeout=0):
    with self.lock:
      print(f'added to can_send {server=}', (addr, 0, dat, bus))
      buf = self.rx_msgs_server if server else self.rx_msgs_client
      buf.append((addr, 0, dat, bus))

  def can_recv(self, server=False):
    with self.lock:
      buf = self.rx_msgs_client if server else self.rx_msgs_server
      if len(buf):
        print(f'returned {server=}', buf[0])

        ret = pop_all(buf)

        return ret
        # return buf.pop(0)
      else:
        return []


# @parameterized_class([
#   {"tx_addr": 0x750, "sub_addr": None},
#   {"tx_addr": 0x750, "sub_addr": 0xf},
# ])
class TestUds(unittest.TestCase):
  tx_addr: int = 0x750
  sub_addr: Optional[int] = None

  TEST_UDS_SERVER_SERVICES: UdsServicesType = {
    SERVICE_TYPE.READ_DATA_BY_IDENTIFIER: {
      None: {  # no subfunction
        b'\xF1\x00': b'\xF1\x00CV1 MFC  AT USA LHD 1.00 1.05 99210-CV000 211027',
        b'\xF1\x90': b'\xF1\x901H3110W0RLD5',
      }
    }
  }

  def setUp(self):
    self.rx_addr = get_rx_addr_for_tx_addr(self.tx_addr)
    can_buf = MockCanBuffer()
    self.uds_server = UdsServer(can_buf, get_rx_addr_for_tx_addr(self.tx_addr), self.tx_addr, sub_addr=self.sub_addr)
    self.uds_server.set_services(STANDARD_UDS_SERVER_SERVICES | self.TEST_UDS_SERVER_SERVICES)
    self.uds_server.start()

  def tearDown(self):
    self.uds_server.stop()

  def test_server_set_services(self):
    """
    Test we can't add service with subfunctions both defined and not defined.
    None is no subfunction supported
    """

    # add new service with subfunction
    services: UdsServicesType = {SERVICE_TYPE.COMMUNICATION_CONTROL: {b'\x00': {b'': b''}}}
    self.uds_server.set_services(STANDARD_UDS_SERVER_SERVICES | services)

    # add None subfunction (no subfunction), can't exist with subfunction above
    services[SERVICE_TYPE.COMMUNICATION_CONTROL][None] = {b'': b''}
    with self.assertRaises(AssertionError):
      self.uds_server.set_services(STANDARD_UDS_SERVER_SERVICES | services)

    # add None service type
    services: UdsServicesType = {None: {b'\x00': {b'': b''}}}
    with self.assertRaises(AssertionError):
      self.uds_server.set_services(STANDARD_UDS_SERVER_SERVICES | services)

  def test_transceive_data(self):
    """
    Asserts no data is lost with back and forth communication.
    Catches bug with sub-addresses and multi-frame communication
    """

    max_bytes = 20
    readback_service: UdsServicesType = {0x00: {None: {b'\x00' * i: b'\x00' * i for i in range(max_bytes)}}}
    self.uds_server.set_services(STANDARD_UDS_SERVER_SERVICES | readback_service)

    for i in range(max_bytes):
      data = b'\x00' * i
      response = self.uds_server._uds_request(0x00, data=data)
      self.assertEqual(data, response)

  def test_tester_present(self):
    """
    Tests IsoTpMessage and CanClient both sending and responding to a
    tester present request with and without sub-addresses
    """

    self.uds_server._uds_request(SERVICE_TYPE.TESTER_PRESENT, 0)

    with self.assertRaises(uds.NegativeResponseError):
      self.uds_server._uds_request(SERVICE_TYPE.TESTER_PRESENT, 0, b'\x00')

    with self.assertRaises(uds.NegativeResponseError):
      self.uds_server._uds_request(SERVICE_TYPE.TESTER_PRESENT)

    with self.assertRaises(uds.NegativeResponseError):
      self.uds_server._uds_request(SERVICE_TYPE.TESTER_PRESENT, 1)

  def test_multi_subfunctions(self):
    for session_type in uds.SESSION_TYPE:
      self.uds_server.diagnostic_session_control(session_type)

    with self.assertRaises(uds.NegativeResponseError):
      self.uds_server.diagnostic_session_control(max(uds.SESSION_TYPE) + 1)

  def test_unsupported_service_type(self):
    # TODO: test exact response bytes, UdsClient doesn't support that yet
    with self.assertRaises(uds.NegativeResponseError):
      self.uds_server._uds_request(100)

    with self.assertRaises(uds.NegativeResponseError):
      self.uds_server._uds_request(SERVICE_TYPE.ACCESS_TIMING_PARAMETER)

  def test_read_by_identifier(self):
    """
    Tests all four ISO-TP frame types in both directions (sending as openpilot and the car ECU)
    """

    response = self.uds_server.read_data_by_identifier(DATA_IDENTIFIER_TYPE.VIN)
    self.assertEqual(response, b'1H3110W0RLD5')

    # test non-standard id
    response = self.uds_server._uds_request(SERVICE_TYPE.READ_DATA_BY_IDENTIFIER, data=b'\xf1\x00')
    self.assertEqual(response, b'\xf1\x00CV1 MFC  AT USA LHD 1.00 1.05 99210-CV000 211027')

    with self.assertRaises(uds.NegativeResponseError):
      self.uds_server._uds_request(SERVICE_TYPE.READ_DATA_BY_IDENTIFIER, 0, data=b'\xf1\x00')

    with self.assertRaises(uds.NegativeResponseError):
      self.uds_server._uds_request(SERVICE_TYPE.READ_DATA_BY_IDENTIFIER, data=b'\xf1\x01')


if __name__ == '__main__':
  unittest.main()
