from panda import Panda

def test_boot_time(p):
  # boot time should be instant
  p.reset(reconnect=False)
  assert Panda.wait_for_panda(p.get_usb_serial(), timeout=0.3)
