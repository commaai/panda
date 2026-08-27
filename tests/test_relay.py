from panda.tests.libpanda import libpanda_py


lpp = libpanda_py.libpanda

RELAY_CHECK_UNKNOWN = 0
RELAY_CHECK_PASS = 1
RELAY_CHECK_FAIL = 2

RELAY_SETTLE_TIMEOUT_US = 1_000_000
RELAY_OBSERVATION_TIMEOUT_US = 3_000_000


def send_packet(bus, address, data, timestamp):
  lpp.relay_test_set_timer(timestamp)
  packet = libpanda_py.make_CANPacket(address, bus, data)
  lpp.relay_monitor_rx(packet)


def send_matching_packets(count, start_time):
  for i in range(count):
    data = i.to_bytes(8, "little")
    send_packet(0, 0x100 + i, data, start_time + i * 1000)
    send_packet(2, 0x100 + i, data, start_time + i * 1000 + 100)


def send_distinct_packets(count, start_time):
  for i in range(count):
    send_packet(0, 0x100, i.to_bytes(8, "little"), start_time + i * 1000)
    send_packet(2, 0x200, i.to_bytes(8, "little"), start_time + i * 1000 + 100)


def send_one_sided_packets(count, start_time):
  for i in range(count):
    send_packet(0, 0x100, i.to_bytes(8, "little"), start_time + i * 1000)


def finish_observation():
  lpp.relay_test_set_timer(RELAY_SETTLE_TIMEOUT_US + RELAY_OBSERVATION_TIMEOUT_US)
  lpp.relay_monitor_tick()


class TestRelayMonitor:
  def setup_method(self):
    lpp.relay_test_set_timer(0)
    lpp.relay_monitor_init()

  def teardown_method(self):
    lpp.relay_monitor_init()

  def test_closed(self):
    send_matching_packets(5, RELAY_SETTLE_TIMEOUT_US)
    lpp.relay_monitor_tick()

    assert lpp.relay_test_get_closed_check() == RELAY_CHECK_PASS
    assert not lpp.relay_monitor_malfunction()

  def test_stuck_open(self):
    send_distinct_packets(100, RELAY_SETTLE_TIMEOUT_US)
    finish_observation()

    assert lpp.relay_test_get_closed_check() == RELAY_CHECK_FAIL
    assert lpp.relay_monitor_malfunction()

  def test_open(self):
    lpp.relay_monitor_set_state(True)
    send_one_sided_packets(100, RELAY_SETTLE_TIMEOUT_US)
    finish_observation()

    assert lpp.relay_test_get_open_check() == RELAY_CHECK_PASS
    assert not lpp.relay_monitor_malfunction()

  def test_stuck_closed(self):
    lpp.relay_monitor_set_state(True)
    send_matching_packets(5, RELAY_SETTLE_TIMEOUT_US)
    lpp.relay_monitor_tick()

    assert lpp.relay_test_get_open_check() == RELAY_CHECK_FAIL
    assert lpp.relay_monitor_malfunction()

  def test_insufficient_traffic_is_inconclusive(self):
    send_distinct_packets(99, RELAY_SETTLE_TIMEOUT_US)
    finish_observation()

    assert lpp.relay_test_get_closed_check() == RELAY_CHECK_UNKNOWN
    assert not lpp.relay_monitor_malfunction()

  def test_failed_check_latches_until_retested(self):
    send_distinct_packets(100, RELAY_SETTLE_TIMEOUT_US)
    finish_observation()
    assert lpp.relay_monitor_malfunction()

    lpp.relay_test_set_timer(10_000_000)
    lpp.relay_monitor_set_state(True)
    send_distinct_packets(100, 11_000_000)
    lpp.relay_test_set_timer(14_000_000)
    lpp.relay_monitor_tick()
    assert lpp.relay_test_get_open_check() == RELAY_CHECK_PASS
    assert lpp.relay_monitor_malfunction()

    lpp.relay_test_set_timer(20_000_000)
    lpp.relay_monitor_set_state(False)
    send_matching_packets(5, 21_000_000)
    lpp.relay_monitor_tick()
    assert lpp.relay_test_get_closed_check() == RELAY_CHECK_PASS
    assert not lpp.relay_monitor_malfunction()
