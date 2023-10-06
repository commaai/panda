#!/usr/bin/env python3
import os
import time

from panda import Panda


if __name__ == "__main__":
  flag_set = False
  while True:
    try:
      with Panda(disable_checks=False) as p:
        if not flag_set:
          p.set_heartbeat_disabled()
          p.set_safety_mode(Panda.SAFETY_ELM327, 30)
          flag_set = True

        # shutdown when told
        if p.health()['safety_mode'] == Panda.SAFETY_SILENT:
          os.system("sudo poweroff")
    except Exception:
      pass
    time.sleep(0.5)
