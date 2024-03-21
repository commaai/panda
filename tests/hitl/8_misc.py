from panda import Panda

def test_startup_time(p):
  p.reset(reconnect=False)
  assert Panda.wait_for_panda(p.serial, timeout=0.3)
