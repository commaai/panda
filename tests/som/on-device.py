#!/usr/bin/env python3
import os
import time

from panda import Panda


if __name__ == "__main__":
  while True:
    try:
      with Panda() as p:
        # set flag to indicate SOM is up
        p.set_safety_mode(Panda.SAFETY_ELM327, 30)

        # shutdown when told
        if p.health()['car_harness_status'] == 1:
          os.system("sudo poweroff")
    except Exception:
      pass
    time.sleep(0.5)
