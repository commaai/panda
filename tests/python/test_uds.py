#!/usr/bin/env python3
import struct
import time
import unittest
from unittest.mock import patch
from panda import Panda
from panda.python.uds import UdsClient, MessageTimeoutError, NegativeResponseError, SERVICE_TYPE, SESSION_TYPE, DATA_IDENTIFIER_TYPE, IsoTpMessage, CanClient
from parameterized import parameterized
import itertools


def p16(val):
  return struct.pack("!H", val)


class FakePanda(Panda):
  def __init__(self, msg):
    self.msg = msg
    # self.last_tx_msg = None
    self.tx_msgs = []

  def can_send(self, addr, dat, bus, timeout=0):
    # self.last_tx_msg = (addr, 0, dat, bus)
    self.tx_msgs.append((addr, 0, dat, bus))

  def can_recv(self):
    return [self.msg]


def make_response_dat(service_type, sub_addr, data: bytes = None):
  ecu_rx_dat = [service_type + 0x40]
  if data is not None:
    ecu_rx_dat += data
  ecu_rx_dat.insert(0, len(ecu_rx_dat))
  if sub_addr is not None:
    ecu_rx_dat.insert(0, sub_addr)
  ecu_rx_dat.extend([0x0] * (8 - len(ecu_rx_dat)))
  return bytes(ecu_rx_dat)


def simulate_isotp_comms(tx_addr: int, rx_addr: int, request: bytes, response: bytes = None,
                         sub_addr: int = None):
  panda = FakePanda([])
  max_len = 8 if sub_addr is None else 7

  can_client_tx = CanClient(panda.can_send, panda.can_recv, tx_addr, rx_addr, 0, sub_addr, debug=True)
  can_client_rx = CanClient(panda.can_send, panda.can_recv, rx_addr, tx_addr, 0, sub_addr, debug=True)

  # TODO: handle multiple messages in the buffer and test without single frame mode as well
  isotp_msg_openpilot = IsoTpMessage(can_client_tx, timeout=0, max_len=max_len, single_frame_mode=True)
  isotp_msg_ecu = IsoTpMessage(can_client_rx, timeout=0, max_len=max_len, single_frame_mode=True)

  # setup car ECU
  isotp_msg_ecu.send(b"", setup_only=True)

  # send data to car ECU and process responses
  isotp_msg_openpilot.send(request)
  panda.msg = panda.tx_msgs.pop()  # put message to tx in recv buffer
  print('sent first message to car ecu: {}'.format(request))

  while not (isotp_msg_openpilot.rx_done and isotp_msg_openpilot.rx_done):
    # time.sleep(0.1)
    print(f'\ncar ECU receiving OP\'s message, {isotp_msg_ecu.rx_done, isotp_msg_ecu.tx_done=}')
    # car ECU receives OP's message
    # put message to
    msg_from_op, _ = isotp_msg_ecu.recv()
    print(f'car ECU receives OP\'s message, {isotp_msg_ecu.rx_done, isotp_msg_ecu.tx_done=}\n')

    if (msg_from_op is not None) == len(panda.tx_msgs):
      print('SHOULD NOT BE HERE:', msg_from_op, panda.tx_msgs)
      raise Exception

    if msg_from_op is None:
      # Car ECU is either sending a consecutive frame or a flow control continue to OP
      print('LEN TX MSGS')
      panda.msg = panda.tx_msgs.pop()
      print(panda.msg, panda.msg[2].hex())

    else:
      print('MSG_FROM_OP NOT NONE')
      resp_sid = msg_from_op[0] if len(msg_from_op) > 0 else None
      if response is not None:
        print('sending back to OP', response)
        isotp_msg_ecu.send(response)
        panda.msg = panda.tx_msgs.pop()  # update rx message for OP to receive

      elif resp_sid == SERVICE_TYPE.TESTER_PRESENT:
        # send back positive response
        print('sending back TESTER PRESENT + 0x40')
        isotp_msg_ecu.send(bytes([SERVICE_TYPE.TESTER_PRESENT + 0x40]))
        print('tx msgs', panda.tx_msgs, f'{isotp_msg_ecu.rx_done, isotp_msg_ecu.tx_done=}')
        # panda.msg = panda.last_tx_msg  # update rx message for OP to receive
        panda.msg = panda.tx_msgs.pop()  # update rx message for OP to receive

      else:
        # unsupported service type, send back 0x7f
        print('sending back unsupported service type')
        isotp_msg_ecu.send(bytes([0x7F, resp_sid]))
        panda.msg = panda.tx_msgs.pop()  # update rx message for OP to receive

    print('\n--- OP RECEIVING')
    msg, _ = isotp_msg_openpilot.recv()
    print('OP RECEIVED:', msg)

    if msg is not None:
      return msg

    panda.msg = panda.tx_msgs.pop()  # relay any messages from OP to car ECU (like flow control continue)
    print('sending BACK TO CAR: {}'.format(panda.msg))


class TestUds(unittest.TestCase):
  # def test_something(self):
  #   # build_isotp_message(0x750, 0x750 + 8, bytes([SERVICE_TYPE.TESTER_PRESENT]))
  #   for i in range(1, 200):
  #     simulate_isotp_comms(0x750, 0x750 + 8, bytes(range(i)), sub_addr=None, response=bytes(range(i)))
  #   # print('built', build_isotp_message(0x750, 0x750 + 8, b'\x3e'))

  # @parameterized.expand([
  #   (0x750, 0xf, 0x750 + 8),
  #   (0x750, None, 0x750 + 8),
  # ])
  # def test_tester_present(self, tx_addr, sub_addr, rx_addr):
  #   """
  #   Tests IsoTpMessage and CanClient both sending and responding to a
  #   tester present request with and without sub-addresses
  #   """
  #
  #   response = simulate_isotp_comms(tx_addr, rx_addr, bytes([SERVICE_TYPE.TESTER_PRESENT]), sub_addr)
  #   self.assertEqual(response, bytes([SERVICE_TYPE.TESTER_PRESENT + 0x40]))

  @parameterized.expand([
    (0x750, 0xf, 0x750 + 8),
    (0x750, None, 0x750 + 8),
  ])
  # @parameterized.expand(itertools.product([(0x750, 0xf, 0x750 + 8), (0x750, None, 0x750 + 8)], range(10)))
  def test_fw_query(self, tx_addr, sub_addr, rx_addr):
    """
    Tests all four ISO-TP frame types in both directions (sending as openpilot and the car ECU)
    """

    for dat_len in range(0x10):
      # same data for both directions
      data = bytes(range(dat_len))
      response = simulate_isotp_comms(tx_addr, rx_addr, data, data, sub_addr)
      self.assertEqual(response, data)

  # @parameterized.expand([
  #   (0x750, 0xf, 0x750 + 8),
  #   (0x750, None, 0x750 + 8),
  # ])
  # def test_uds_client_fw_query(self, tx_addr, sub_addr, rx_addr):
  #   """
  #   Tests UdsClient, IsoTpMessage, and the CanClient with a
  #   tester present request with and without sub-addresses
  #   """
  #   data = p16(DATA_IDENTIFIER_TYPE.APPLICATION_SOFTWARE_IDENTIFICATION) + b'\x09\x04'
  #   ecu_response_dat = make_response_dat(SERVICE_TYPE.READ_DATA_BY_IDENTIFIER, sub_addr, data=data)
  #   panda = FakePanda(msg=(0x750 + 0x8, 0, bytes(ecu_response_dat), 0))
  #   uds_client = UdsClient(panda, tx_addr, rx_addr, sub_addr=sub_addr)
  #   dat = uds_client.read_data_by_identifier(DATA_IDENTIFIER_TYPE.APPLICATION_SOFTWARE_IDENTIFICATION)
  #   print('final dat', dat)

if __name__ == '__main__':
  unittest.main()
