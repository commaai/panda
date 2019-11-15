def test_relay_malfunction(test, addr):
  # input is a test class and the address that, if seen on bus 0, triggers
  # the relay_malfunction logic
  test.assertFalse(test.safety.get_relay_malfunction())
  test.safety.safety_rx_hook(test._send_msg(0, addr, 8))
  test.assertTrue(test.safety.get_relay_malfunction())
  for a in range(1, 0x800):
    for b in range(0, 3):
      test.assertFalse(test.safety.safety_tx_hook(test._send_msg(b, a, 8)))
