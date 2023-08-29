#!/usr/bin/env python3
from typing import *
from parameterized import parameterized_class
import unittest
import struct
import threading
from dataclasses import dataclass

from panda.python import uds
from panda.python.uds import SERVICE_TYPE, DATA_IDENTIFIER_TYPE, IsoTpMessage, UdsClient, get_rx_addr_for_tx_addr

DEFAULT_VIN_B = b'1H3110W0RLD5'


class UdsServer(UdsClient):
  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs, single_frame_mode=True)
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
    # {service_type: {subfunction: response (if True, positive, if bytes, return that)}}  # TODO: maybe just bytes
    # TODO: might want to use a dataclass so we aren't using dynamically shaped dicts
    # Dict[SERVICE_TYPE, Dict[bytes, bool | bytes]]
    lookup = {
      SERVICE_TYPE.TESTER_PRESENT: {
        b'\x00': {  # sub function to
          b'': b'',  # don't expect any extra data, don't respond with extra data
        },
      },
      SERVICE_TYPE.READ_DATA_BY_IDENTIFIER: {
        None: {  # no subfunction, only responds to data  # TODO: test no other subfunctions
          b'\xF1\x00': b'CV1 MFC  AT USA LHD 1.00 1.05 99210-CV000 211027',
          b'\xF1\x90': DEFAULT_VIN_B,
        }
      },
    }

    # send request, wait for response
    max_len = 8 if self.sub_addr is None else 7
    isotp_msg = IsoTpMessage(self._server_can_client, timeout=0, debug=self.debug, max_len=max_len, single_frame_mode=True)
    isotp_msg.send(b"", setup_only=True)
    while not self.kill_event.is_set():
      resp, _ = isotp_msg.recv(0)  # resp is request from client
      print('UDS Server - got resp', resp)

      # message from client not fully built
      if resp is None:
        continue

      # got a message, now respond
      resp_sid = resp[0] if len(resp) > 0 else None

      if resp_sid in lookup:
        # get subfunction from request if expected to have one
        resp_sfn = bytes([resp[1]]) if len(resp) > 1 and None not in lookup[resp_sid] else None

        # service_has_subfunction = None not in lookup[resp_sid]  # len(lookup[resp_sid]) == 1
        # print(f'{service_has_subfunction=}')
        if resp_sfn is None:
          data = resp[(1 if resp_sfn is None else 2):]
          print('data in service', data, lookup[resp_sid][None])
          if data in lookup[resp_sid][None]:
            print('here!')
            send_dat = bytes([resp_sid + 0x40]) + resp[1:] + lookup[resp_sid][None][data]
            isotp_msg.send(send_dat)
          else:
            # invalid data
            dat = [0x7F]
            if resp_sid is not None:
              dat.append(resp_sid)
            dat.append(0x31)  # request out of range
            dat = bytes(dat)
            print('Car ECU - bad request, sending back:', dat)
            isotp_msg.send(dat)

        else:  # has subfunction
          # resp_sfn = resp[1] if len(resp) > 1 else None
          print('resp_sfn', resp_sfn)
          if resp_sfn not in lookup[resp_sid]:
            # return invalid subfunction
            dat = [0x7F]
            if resp_sid is not None:  # TODO: should be always true
              dat.append(resp_sid)
            dat.append(0x7e)
            dat = bytes(dat)
            print('Car ECU - bad subfunction, sending back:', dat.hex())
            isotp_msg.send(dat)

          else:
            # valid subfunction, now check data
            data = resp[(1 if resp_sfn is None else 2):]
            if data in lookup[resp_sid][resp_sfn]:
              # valid data
              send_dat = [resp_sid + 0x40, int.from_bytes(resp_sfn)]
              if len(lookup[resp_sid][resp_sfn][data]):
                send_dat.append(lookup[resp_sid][resp_sfn][data])  # add data if exists
              print('send_dat', send_dat, resp_sid)
              send_dat = bytes(send_dat)
              isotp_msg.send(send_dat)

            else:
              # invalid data
              dat = [0x7F]
              if resp_sid is not None:
                dat.append(resp_sid)
              dat = bytes(dat)
              print('Car ECU - bad request, sending back:', dat)
              isotp_msg.send(dat)

      else:
        # invalid service
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


# @parameterized_class([
#   {"tx_addr": 0x750, "sub_addr": None},
#   {"tx_addr": 0x750, "sub_addr": 0xf},
# ])
class TestUds(unittest.TestCase):
  tx_addr: int = 0x750
  sub_addr: int = None

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
    self.assertEqual(response, DEFAULT_VIN_B)

    response = self.uds_server._uds_request(SERVICE_TYPE.READ_DATA_BY_IDENTIFIER, None, b'\xf1\x00')
    self.assertEqual(response, b'\xf1\x00CV1 MFC  AT USA LHD 1.00 1.05 99210-CV000 211027')

    with self.assertRaises(uds.NegativeResponseError):
      self.uds_server._uds_request(SERVICE_TYPE.READ_DATA_BY_IDENTIFIER, None, b'\xf1\x01')


if __name__ == '__main__':
  unittest.main()
