import time

from panda import Panda, PandaDFU
from panda.tests.hitl.base import PandaTestCase

# openpilot v0.11.1 system/hardware/tici/hardware.py
STM_RST_N = 124
STM_BOOT0 = 134


def gpio_set(pin, high):
  with open(f"/sys/class/gpio/gpio{pin}/direction", 'wb') as f:
    f.write(b"out")
  with open(f"/sys/class/gpio/gpio{pin}/value", 'wb') as f:
    f.write(b"1" if high else b"0")


def reset_internal_panda():
  gpio_set(STM_RST_N, 1)
  gpio_set(STM_BOOT0, 0)
  time.sleep(0.01)
  gpio_set(STM_RST_N, 0)


def recover_internal_panda():
  gpio_set(STM_RST_N, 1)
  gpio_set(STM_BOOT0, 1)
  time.sleep(0.01)
  gpio_set(STM_RST_N, 0)
  time.sleep(0.01)
  gpio_set(STM_BOOT0, 0)


class TestBoot(PandaTestCase):
  panda_types = Panda.INTERNAL_DEVICES

  def test_pandad_boot_flow(self):
    p = self.p
    for count in range(10):
      # openpilot v0.11.1 selfdrive/pandad/pandad.py
      if (count % 2) == 0:
        reset_internal_panda()
      else:
        recover_internal_panda()
      for serial in PandaDFU.list():
        PandaDFU(serial).recover()
        time.sleep(1)

      assert Panda.wait_for_panda(None, 10)
      p.reconnect()
      if p.bootstub:
        p.flash()
        p.reconnect()

      time.sleep(2)
      h = p.health()
      log = p.debug_read().decode(errors='replace')
      print(f"{count=} uptime={h['uptime']} faults={h['faults']:#x}")
      for line in log.splitlines():
        if "Register" in line or "fault" in line:
          print(line)
