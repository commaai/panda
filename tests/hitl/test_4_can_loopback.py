import os
import time
import random
from concurrent.futures import ThreadPoolExecutor
from collections import defaultdict

from opendbc.car.structs import CarParams
from panda.tests.hitl.base import PandaTestCase, panda_test
from panda.tests.hitl.helpers import get_random_can_messages, clear_can_buffers

class TestCanLoopback(PandaTestCase):
  @panda_test(expect_can_error=True)
  def test_gen2_loopback(self):
    p = self.p
    up = self.up
    def test(p_send, p_recv, address=None):
      for bus in range(4):
        obd = False
        if bus == 3:
          obd = True
          bus = 1

        # Clear buses
        clear_can_buffers(p_send)
        clear_can_buffers(p_recv)

        # Send a random string
        addr = address if address else random.randint(1, 2000)
        string = b"test" + os.urandom(4)
        p.set_obd(obd)
        # The UP exposes the OBD pair as bus 3 instead of switching bus 1.
        send_bus = 3 if obd and p_send is up else bus
        recv_bus = 3 if obd and p_recv is up else bus
        time.sleep(0.2)
        p_send.can_send(addr, string, send_bus)
        time.sleep(0.2)

        content = p_recv.can_recv()

        # Check amount of messages
        assert len(content) == 1

        # Check content
        assert content[0][0] == addr and content[0][1] == string

        # Check bus
        assert content[0][2] == recv_bus

        print("Bus:", bus, "address:", addr, "OBD:", obd, "OK")

    # Run tests in both directions
    p.set_safety_mode(CarParams.SafetyModel.allOutput)
    test(p, up)
    test(up, p)

    # Test extended frame address with ELM327 mode
    p.set_safety_mode(CarParams.SafetyModel.elm327)
    test(p, up, 0x18DB33F1)
    test(up, p, 0x18DB33F1)

    p.set_obd(False)

  def test_bulk_write(self):
    p = self.p
    p.set_safety_mode(CarParams.SafetyModel.allOutput)
    p.set_can_loopback(True)
    # Send far more than the panda's TX buffers can hold.
    NUM_MESSAGES_PER_BUS = 10000

    def flood_tx(panda):
      print('Sending!')
      msg = b"\xaa" * 8
      packet = []
      # start with many messages on a single bus (higher contention for single TX ring buffer)
      packet += [[0xaa, msg, 0]] * NUM_MESSAGES_PER_BUS
      # end with many messages on multiple buses
      packet += [[0xaa, msg, 0], [0xaa, msg, 1], [0xaa, msg, 2]] * NUM_MESSAGES_PER_BUS

      panda.can_send_many(packet, timeout=10000)
      print(f"Done sending {4 * NUM_MESSAGES_PER_BUS} messages!", time.monotonic())
      print(panda.health())

    # Drain looped-back frames and receipts while the writer fills the TX queues.
    with ThreadPoolExecutor(max_workers=1) as executor:
      sender = executor.submit(flood_tx, p)
      received = []
      deadline = time.monotonic() + 30
      while len(received) < 8 * NUM_MESSAGES_PER_BUS and time.monotonic() < deadline:
        received.extend(p.can_recv())
      sender.result()

    for bus, count in ((0, 2 * NUM_MESSAGES_PER_BUS), (1, NUM_MESSAGES_PER_BUS), (2, NUM_MESSAGES_PER_BUS)):
      for receipt in (0, 0x80):
        actual = received.count((0xaa, b"\xaa" * 8, bus | receipt))
        assert actual == count, (bus, receipt, actual, count)
    assert len(received) == 8 * NUM_MESSAGES_PER_BUS

  def test_message_integrity(self):
    p = self.p
    p.set_safety_mode(CarParams.SafetyModel.allOutput)
    p.set_can_loopback(True)
    for i in range(250):
      sent_msgs = defaultdict(set)
      for _ in range(random.randrange(10)):
        to_send = get_random_can_messages(random.randrange(100))
        for m in to_send:
          sent_msgs[m[2]].add((m[0], m[1]))
        p.can_send_many(to_send, timeout=0)

      start_time = time.monotonic()
      while time.monotonic() - start_time < 2 and any(len(sent_msgs[bus]) for bus in range(3)):
        recvd = p.can_recv()
        for msg in recvd:
          if msg[2] >= 128:
            k = (msg[0], bytes(msg[1]))
            bus = msg[2]-128
            assert k in sent_msgs[bus], f"message {k} was never sent on bus {bus}"
            sent_msgs[msg[2]-128].discard(k)

      # if a set isn't empty, messages got dropped
      for bus in range(3):
        assert not len(sent_msgs[bus]), f"loop {i}: bus {bus} missing {len(sent_msgs[bus])} messages"

    print("Got all messages intact")
