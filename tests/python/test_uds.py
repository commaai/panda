#!/usr/bin/env python3
import struct
import time
import unittest
from unittest.mock import patch
from panda import Panda
from panda.python.uds import UdsClient, MessageTimeoutError, NegativeResponseError, SERVICE_TYPE, SESSION_TYPE, DATA_IDENTIFIER_TYPE, IsoTpMessage, CanClient
from parameterized import parameterized


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


def build_isotp_message(tx_addr: int, rx_addr: int, data: bytes, sub_addr: int = None):
  panda = FakePanda([])

  can_client_tx = CanClient(panda.can_send, panda.can_recv, tx_addr, rx_addr, 0, sub_addr, debug=True)
  can_client_rx = CanClient(panda.can_send, panda.can_recv, rx_addr, tx_addr, 0, sub_addr, debug=True)

  max_len = 8 if sub_addr is None else 7
  isotp_msg_tx = IsoTpMessage(can_client_tx, timeout=0, max_len=max_len, single_frame_mode=True)  # openpilot
  isotp_msg_rx = IsoTpMessage(can_client_rx, timeout=0, max_len=max_len)  # car ECU
  isotp_msg_rx.send(b"", setup_only=True)  # setup car ECU

  # send data to car ECU and process responses
  isotp_msg_tx.send(data)  # get first message
  # panda.msg = panda.last_tx_msg  # put tx message in panda recv buffer
  panda.msg = panda.tx_msgs.pop()  # put tx message in panda recv buffer
  print('OP tx done, rx done', (isotp_msg_tx.tx_done, isotp_msg_tx.rx_done))
  # return

  while not (isotp_msg_tx.rx_done and isotp_msg_tx.rx_done):
    time.sleep(0.1)
    # car ECU receives OP's message
    print(f'\ncar ECU receiving OP\'s message, {isotp_msg_rx.rx_done, isotp_msg_rx.tx_done=}')

    msg, updated = isotp_msg_rx.recv()
    print(f'car ECU receives OP\'s message, {isotp_msg_rx.rx_done, isotp_msg_rx.tx_done=}\n')
    if len(panda.tx_msgs):
      panda.msg = panda.tx_msgs.pop()

    if msg is not None:

      service_type = msg[0]
      if service_type == SERVICE_TYPE.TESTER_PRESENT:
        # send back positive response
        print('sending back TESTER PRESENT + 0x40')
        # isotp_msg_rx.send(bytes([SERVICE_TYPE.TESTER_PRESENT + 0x40]))
        data_to_send_to_op = b"\x7e\x7e\x7e\x7e\x7e\x7e\x7e"
        isotp_msg_rx.send(data_to_send_to_op)
        print('tx msgs', panda.tx_msgs, f'{isotp_msg_rx.rx_done, isotp_msg_rx.tx_done=}')
        # panda.msg = panda.last_tx_msg  # update rx message for OP to receive
        panda.msg = panda.tx_msgs.pop()  # update rx message for OP to receive
      else:
        raise Exception
        # unsupported service type, send back 0x7f
        print('sending back unsupported service type')
        isotp_msg_rx.send(bytes([0x7F, service_type]))
        panda.msg = panda.last_tx_msg  # update rx message for OP to receive

    print('\n--- OP RECEIVING')
    msg, _ = isotp_msg_tx.recv()
    print('OP RECEIVED:', msg)

    if msg is not None:
      return msg

    print('sending BACK TO CAR: {}'.format(panda.msg))
    panda.msg = panda.tx_msgs.pop()  # relay any messages from OP to car ECU (like flow control continue)
    # return
    # return
    # print(isotp_msg_tx.rx_done, isotp_msg_tx.tx_done)
    # print(f'{ecu_response_msg=}')
    # # print(panda.last_tx_msg)
    # if data[0] == SERVICE_TYPE.TESTER_PRESENT:

    # return

  return msg
  # print('\nStarting ECU side')
  # ecu_response_msg, _ = isotp_msg_rx.recv()  # now the ECU receives the tx message OP sent and responds
  # isotp_msg_rx.send(ecu_response_msg)
  # print('ecu_response_msg', panda.last_tx_msg)
  #
  # print('\nStarting OP side again')
  # ecu_response_msg, _ = isotp_msg_rx.recv()  # now we read what the car sent back
  # print('Final response!', ecu_response_msg)
  #
  # return panda.tx_msgs


class TestUds(unittest.TestCase):
  def test_something(self):
    # build_isotp_message(0x750, 0x750 + 8, bytes([SERVICE_TYPE.TESTER_PRESENT]))
    build_isotp_message(0x750, 0x750 + 8, b"\x3e", 0xf)
    # print('built', build_isotp_message(0x750, 0x750 + 8, b'\x3e'))


  # @parameterized.expand([
  #   (0x750, 0xf, 0x750 + 8),
  #   (0x750, None, 0x750 + 8),
  # ])
  # def test_uds_client_tester_present(self, tx_addr, sub_addr, rx_addr):
  #   """
  #   Tests UdsClient, IsoTpMessage, and the CanClient with a
  #   tester present request with and without sub-addresses
  #   """
  #
  #   ecu_response_dat = make_response_dat(SERVICE_TYPE.TESTER_PRESENT, sub_addr, b"\x00")
  #   panda = FakePanda(msg=(rx_addr, 0, bytes(ecu_response_dat), 0))
  #   uds_client = UdsClient(panda, tx_addr, rx_addr, sub_addr=sub_addr)
  #   uds_client.tester_present()

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
