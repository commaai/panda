#!/usr/bin/env python3
import time
from panda import Panda, PandaDFU

class GPIO:
  STM_RST_N = 124
  STM_BOOT0 = 134
  HUB_RST_N = 30


def gpio_set(pin, high):
  with open(f"/sys/class/gpio/gpio{pin}/direction", 'wb') as f:
    f.write(b"out")
  with open(f"/sys/class/gpio/gpio{pin}/value", 'wb') as f:
    f.write(b"1" if high else b"0")


def reflash_bootstub():
  # we have to try a few times since there's a race condition
  # between SPI and USB in the ST bootloader
  for tryy in range(5):
    print("resetting into DFU")
    gpio_set(GPIO.STM_RST_N, 1)
    gpio_set(GPIO.STM_BOOT0, 1)
    time.sleep(0.1)
    gpio_set(GPIO.STM_RST_N, 0)
    gpio_set(GPIO.STM_BOOT0, 0)
    if Panda.wait_for_dfu(None, timeout=5):
      break

  print("flashing bootstub")
  PandaDFU(None).recover()

  gpio_set(GPIO.STM_RST_N, 1)
  time.sleep(0.5)
  gpio_set(GPIO.STM_RST_N, 0)


if __name__ == "__main__":
  # reset USB hub for dos
  gpio_set(GPIO.HUB_RST_N, 0)
  time.sleep(0.5)
  gpio_set(GPIO.HUB_RST_N, 1)

  reflash_bootstub()
  time.sleep(1)

  print("flashing app")
  p = Panda()
  assert p.bootstub
  p.flash()
